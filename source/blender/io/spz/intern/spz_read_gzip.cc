/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#include "spz_read.hh"

#include <zlib.h>

#include <array>
#include <cstdint>
#include <optional>

#include "BLI_array.hh"
#include "BLI_assert.hh"
#include "BLI_span.hh"

#include "BKE_lib_id.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"

#include "CLG_log.h"

#include "IO_gsplat.hh"
#include "IO_validate.hh"

#include "spz_read_common.hh"
#include "spz_types.hh"

namespace blender::io::spz {

static CLG_LogRef LOG = {"io.spz"};

namespace {

struct PackedGaussiansHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t num_points;
  uint8_t sh_degree;
  uint8_t fractional_bits;
  uint8_t flags;
  uint8_t reserved;
};
static_assert(sizeof(PackedGaussiansHeader) == 16);

/* Helper class that takes care of reading data from FILE in chunks. */
class BufferedFileReader {
  static constexpr int BUFFER_SIZE = 65536;

  FILE *file_ = nullptr;
  Array<uint8_t> buffer_;

 public:
  explicit BufferedFileReader(FILE *file) : file_(file), buffer_(BUFFER_SIZE) {}

  std::optional<Span<uint8_t>> read()
  {
    const size_t num_read_bytes = fread(buffer_.data(), 1, buffer_.size(), file_);
    if (ferror(file_)) {
      return std::nullopt;
    }
    return buffer_.as_span().slice(0, num_read_bytes);
  }
};

/* Helper class that allows to read a Gzip-compressed file from disk with the minimum amount of
 * extra memory usage, but allowing to easily access data of specific size. */
class StreamedGzipReader {
  BufferedFileReader buffered_reader_;
  z_stream stream_{};

 public:
  explicit StreamedGzipReader(FILE *file) : buffered_reader_(file) {}

  ~StreamedGzipReader()
  {
    inflateEnd(&stream_);
  }

  bool initialize()
  {
    /* The window size matches the SPZ library: here 16 means enable automatic gzip header
     * detection; consider switching this to 32 to enable both automated gzip and zlib header
     * detection. */
    return inflateInit2(&stream_, 16 | MAX_WBITS) == Z_OK;
  }

  bool read_buffer(const MutableSpan<uint8_t> buffer)
  {
    stream_.avail_out = buffer.size();
    stream_.next_out = buffer.data();

    do {
      /* There might be left-over data in the input since previous read.
       * If there is, process that data first. Otherwise, load the next chunk from the input file.
       */
      if (stream_.avail_in == 0) {
        std::optional<Span<uint8_t>> read_result = buffered_reader_.read();
        if (!read_result.has_value() || read_result->size() == 0) {
          return false;
        }
        stream_.avail_in = read_result->size();
        stream_.next_in = const_cast<Bytef *>(read_result->data());
      }

      const int ret = inflate(&stream_, Z_NO_FLUSH);
      BLI_assert(ret != Z_STREAM_ERROR);
      if (ret == Z_STREAM_END) {
        BLI_assert(stream_.avail_out == 0);
        break;
      }
      if (ret != Z_OK) {
        /* TODO(sergey): Report error. */
        return false;
      }
    } while (stream_.avail_out != 0);

    // TODO(sergey): Investigate whether decompressing more data ahead of time helps performance.

    return true;
  }

  template<class T> bool read(T &data)
  {
    return read_buffer(MutableSpan<uint8_t>(reinterpret_cast<uint8_t *>(&data), sizeof(T)));
  }
};

}  // namespace

PointCloud *read_spz_gzip_compressed_file(FILE *file, ReportList *reports)
{
  StreamedGzipReader reader(file);

  if (!reader.initialize()) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing Zlib decompression");
    return nullptr;
  }

  PackedGaussiansHeader header;
  if (!reader.read(header)) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading header from GZip stream");
    return nullptr;
  }

  CLOG_DEBUG(&LOG, "SPZ header magic: 0x%X", header.magic);
  CLOG_DEBUG(&LOG, "SPZ header version: %u", header.version);

  if (header.magic != SPZ_HEADER_MAGIC) {
    BKE_reportf(reports, RPT_ERROR, "SPZ Read: Unexpected SPZ header magic 0x%X", header.magic);
    return nullptr;
  }
  if (header.version != 2 && header.version != 3) {
    BKE_reportf(reports, RPT_ERROR, "SPZ Read: Unsupported SPZ version %u", header.version);
    return nullptr;
  }

  CLOG_DEBUG(&LOG, "SPZ header num_points: %u", header.num_points);
  CLOG_DEBUG(&LOG, "SPZ header sh_degree: %d", int(header.sh_degree));
  CLOG_DEBUG(&LOG, "SPZ header fractional_bits: %d", int(header.fractional_bits));
  CLOG_DEBUG(&LOG, "SPZ header flags: %d", header.flags);

  if (header.flags & SPZ_HEADER_ANTIALIASED) {
    /* TODO(sergey): Support antialiased data. */
    CLOG_WARN(&LOG, "SPZ data was trained with antialiasing which is not fully supported");
  }
  if (header.flags & SPZ_HEADER_HAS_EXTENSIONS) {
    /* TODO(sergey): Support extensions. */
    CLOG_WARN(&LOG, "SPZ file contains extensions that are not yet supported");
  }

  if (!validate::size_fits_in_int(header.num_points)) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Too many points");
    return nullptr;
  }

  PointCloud *point_cloud = BKE_pointcloud_new_nomain(PT_TYPE_GSPLAT, header.num_points);
  gsplat::GsplatMutableAttributeAccessor accessor(*point_cloud, header.sh_degree);

  const MutableSpan<float3> positions = accessor.positions_for_write();
  const MutableSpan<math::Quaternion> rotations = accessor.rotations_for_write();
  const Span<MutableSpan<float3>> sh_attrs = accessor.sh_for_write();

  if (!read_positions(reader, header.fractional_bits, positions) ||
      !read_alphas(reader, accessor.radiance_base_for_write()) ||
      !read_colors(reader, accessor.radiance_base_for_write()) ||
      !read_scales(reader, accessor.scales_for_write()) ||
      !read_rotations(reader, header.version, rotations) || !read_sh(reader, sh_attrs))
  {
    accessor.finish();
    BKE_id_free(nullptr, &point_cloud->id);
    return nullptr;
  }

  /* TODO(sergey): Handle extensions. */

  convert_axis_to_blender(positions, rotations, sh_attrs);

  accessor.finish();

  return point_cloud;
}

}  // namespace blender::io::spz
