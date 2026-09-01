/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#include "spz_read.hh"

#include <zstd.h>

#include <cstdint>
#include <utility>

#include "BLI_array.hh"
#include "BLI_fileops.hh"
#include "BLI_math_base.hh"

#include "BKE_lib_id.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"

#include "IO_gsplat.hh"
#include "IO_validate.hh"

#include "CLG_log.h"

#include "spz_read_common.hh"
#include "spz_types.hh"

namespace blender::io::spz {

static CLG_LogRef LOG = {"io.spz"};

namespace {

struct NgspFileHeader {
  uint32_t magic;   /* 0x5053474e ("NGSP") */
  uint32_t version; /* 4 */
  uint32_t num_points;
  uint8_t sh_degree;
  uint8_t fractional_bits;
  uint8_t flags;
  uint8_t num_streams;      /* The number of ZSTD-compressed attribute streams (typically 6). */
  uint32_t toc_byte_offset; /* Byte offset from file start to the TOC */
  uint8_t reserved[12];     /* zero, reserved for future use. */
};
static_assert(sizeof(NgspFileHeader) == 32);

struct StreamInfo {
  /* Offset of the stream from the beginning of the file. */
  uint64_t compressed_offset;

  uint64_t compressed_size;
  uint64_t uncompressed_size;
};

/* Helper class that takes care of reading data from SPZ stream from file in chunks. */
class BufferedStreamReader {
  static constexpr int BUFFER_SIZE = 65536;

  FILE *file_ = nullptr;
  StreamInfo stream_info_;
  Array<uint8_t> buffer_;

  uint64_t num_read_bytes_ = 0;

 public:
  BufferedStreamReader(FILE *file, const StreamInfo &stream_info)
      : file_(file), stream_info_(stream_info), buffer_(BUFFER_SIZE)
  {
  }

  bool initialize()
  {
    return BLI_fseek(file_, int64_t(stream_info_.compressed_offset), SEEK_SET) != -1;
  }

  std::optional<Span<uint8_t>> read()
  {
    const uint64_t num_bytes_to_read = math::min(stream_info_.compressed_size - num_read_bytes_,
                                                 uint64_t(buffer_.size()));
    const size_t num_read_bytes = fread(buffer_.data(), 1, num_bytes_to_read, file_);
    if (ferror(file_) || num_bytes_to_read != num_read_bytes) {
      return std::nullopt;
    }
    num_read_bytes_ += num_bytes_to_read;
    return buffer_.as_span().slice(0, num_read_bytes);
  }
};

/* Helper class that allows to read a Gzip-compressed file from disk with the minimum amount of
 * extra memory usage, but allowing to easily access data of specific size. */
class StreamedZstdReader {
  BufferedStreamReader buffered_reader_;
  ZSTD_DCtx *dctx_ = nullptr;
  ZSTD_inBuffer input_buffer_;

 public:
  explicit StreamedZstdReader(FILE *file, const StreamInfo &stream_info)
      : buffered_reader_(file, stream_info)
  {
  }

  ~StreamedZstdReader()
  {
    if (dctx_) {
      ZSTD_freeDCtx(dctx_);
    }
  }

  bool initialize()
  {
    if (!buffered_reader_.initialize()) {
      return false;
    }

    dctx_ = ZSTD_createDCtx();

    input_buffer_.src = nullptr;
    input_buffer_.size = 0;
    input_buffer_.pos = 0;

    return true;
  }

  bool read_buffer(const MutableSpan<uint8_t> buffer)
  {
    ZSTD_outBuffer output_buffer = {.dst = buffer.data(), .size = size_t(buffer.size()), .pos = 0};

    do {
      /* There might be left-over data in the input since previous read.
       * If there is, process that data first. Otherwise, load the next chunk from the input file.
       */
      if (input_buffer_.pos == input_buffer_.size) {
        std::optional<Span<uint8_t>> read_result = buffered_reader_.read();
        if (!read_result.has_value() || read_result->size() == 0) {
          return false;
        }
        input_buffer_.src = read_result->data();
        input_buffer_.size = read_result->size();
        input_buffer_.pos = 0;
      }

      const size_t ret = ZSTD_decompressStream(dctx_, &output_buffer, &input_buffer_);
      if (ZSTD_isError(ret)) {
        // TODO(sergey): Report error using ZSTD_getErrorName(lastResult) ?
        return false;
      }
    } while (output_buffer.pos < output_buffer.size);

    return true;
  }

  template<class T> bool read(T &data)
  {
    return read_buffer(MutableSpan<uint8_t>(reinterpret_cast<uint8_t *>(&data), sizeof(T)));
  }
};

}  // namespace

template<class T> static bool read_spz_struct(FILE *file, T &data)
{
  if (fread(&data, sizeof(T), 1, file) != 1) {
    return false;
  }
  return true;
}

static bool read_toc(FILE *file, const NgspFileHeader &header, Array<StreamInfo> &stream_infos)
{
  struct PackedStreamInfo {
    uint64_t compressed_size;
    uint64_t uncompressed_size;
  };
  static_assert(sizeof(PackedStreamInfo) == 16);

  if (header.num_streams == 0) {
    return true;
  }

  if (BLI_fseek(file, header.toc_byte_offset, SEEK_SET) == -1) {
    return false;
  }

  Vector<PackedStreamInfo> packed_stream_infos(header.num_streams);
  if (fread(packed_stream_infos.data(), sizeof(PackedStreamInfo), header.num_streams, file) !=
      header.num_streams)
  {
    return false;
  }

  stream_infos.reinitialize(header.num_streams);
  stream_infos[0].compressed_offset = header.toc_byte_offset +
                                      packed_stream_infos.size() * sizeof(PackedStreamInfo);
  stream_infos[0].compressed_size = packed_stream_infos[0].compressed_size;
  stream_infos[0].uncompressed_size = packed_stream_infos[0].uncompressed_size;

  for (int i = 1; i < header.num_streams; ++i) {
    stream_infos[i].compressed_offset = stream_infos[i - 1].compressed_offset +
                                        stream_infos[i - 1].compressed_size;
    stream_infos[i].compressed_size = packed_stream_infos[i].compressed_size;
    stream_infos[i].uncompressed_size = packed_stream_infos[i].uncompressed_size;
  }

  return true;
}

PointCloud *read_spz_ngsp_file(FILE *file, ReportList *reports)
{
  NgspFileHeader header;
  if (!read_spz_struct(file, header)) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading NGSP header");
    return nullptr;
  }

  CLOG_DEBUG(&LOG, "SPZ header magic: 0x%X", header.magic);
  CLOG_DEBUG(&LOG, "SPZ header version: %u", header.version);

  if (header.magic != SPZ_HEADER_MAGIC) {
    BKE_reportf(reports, RPT_ERROR, "SPZ Read: Unexpected SPZ header magic 0x%X", header.magic);
    return nullptr;
  }
  if (header.version != 4) {
    BKE_reportf(reports, RPT_ERROR, "SPZ Read: Unsupported SPZ version %u", header.version);
    return nullptr;
  }

  CLOG_DEBUG(&LOG, "SPZ header num_points: %u", header.num_points);
  CLOG_DEBUG(&LOG, "SPZ header sh_degree: %d", int(header.sh_degree));
  CLOG_DEBUG(&LOG, "SPZ header fractional_bits: %d", int(header.fractional_bits));
  CLOG_DEBUG(&LOG, "SPZ header flags: %d", int(header.flags));
  CLOG_DEBUG(&LOG, "SPZ header num_streams: %d", int(header.num_streams));
  CLOG_DEBUG(&LOG, "SPZ header toc_byte_offset: %u", header.toc_byte_offset);

  if (header.flags & SPZ_HEADER_ANTIALIASED) {
    /* TODO(sergey): Support antialiased data. */
    CLOG_WARN(&LOG, "SPZ data was trained with antialiasing which is not fully supported");
  }
  if (header.flags & SPZ_HEADER_HAS_EXTENSIONS) {
    /* TODO(sergey): Support extensions. */
    CLOG_WARN(&LOG, "SPZ file contains extensions that are not yet supported");
  }

  if (header.toc_byte_offset < sizeof(NgspFileHeader)) {
    BKE_report(
        reports, RPT_ERROR, "SPZ Read: TOC byte offset is less than the size of the header");
    return nullptr;
  }

  Array<StreamInfo> stream_infos;
  if (!read_toc(file, header, stream_infos)) {
    return nullptr;
  }

  /* There is expected to be 6 streams: positions, alphas, colors, scales, rotations, sh. */
  if (stream_infos.size() < 6) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Unexpected number of Zstd streams");
    return nullptr;
  }

  if (!validate::size_fits_in_int(header.num_points)) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Too many points");
    return nullptr;
  }

  PointCloud *point_cloud = BKE_pointcloud_new_nomain(PT_TYPE_GSPLAT, header.num_points);
  gsplat::GsplatMutableAttributeAccessor accessor(*point_cloud, header.sh_degree);

  bool ok = true;

  const MutableSpan<float3> positions = accessor.positions_for_write();
  const MutableSpan<math::Quaternion> rotations = accessor.rotations_for_write();
  const Span<MutableSpan<float3>> sh_attrs = accessor.sh_for_write();

  /* Positions. */
  if (ok) {
    StreamedZstdReader reader(file, stream_infos[0]);
    if (!reader.initialize()) {
      BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing positions reader");
      ok = false;
    }
    else {
      ok = read_positions(reader, header.fractional_bits, positions);
      if (!ok) {
        BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading positions");
      }
    }
  }

  /* Alphas and colors. */
  if (ok) {
    StreamedZstdReader reader(file, stream_infos[1]);
    if (!reader.initialize()) {
      BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing alphas reader");
      ok = false;
    }
    else {
      ok = read_alphas(reader, accessor.radiance_base_for_write());
      if (!ok) {
        BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading alphas");
      }
    }
  }
  if (ok) {
    StreamedZstdReader reader(file, stream_infos[2]);
    if (!reader.initialize()) {
      BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing colors reader");
      ok = false;
    }
    else {
      ok = read_colors(reader, accessor.radiance_base_for_write());
      if (!ok) {
        BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading colors");
      }
    }
  }

  /* Scales. */
  if (ok) {
    StreamedZstdReader reader(file, stream_infos[3]);
    if (!reader.initialize()) {
      BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing scales reader");
      ok = false;
    }
    else {
      ok = read_scales(reader, accessor.scales_for_write());
      if (!ok) {
        BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading scales");
      }
    }
  }

  /* Rotations. */
  if (ok) {
    StreamedZstdReader reader(file, stream_infos[4]);
    if (!reader.initialize()) {
      BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing rotations reader");
      ok = false;
    }
    else {
      ok = read_rotations(reader, header.version, rotations);
      if (!ok) {
        BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading rotations");
      }
    }
  }

  /* SH. */
  if (ok) {
    StreamedZstdReader reader(file, stream_infos[5]);
    if (!reader.initialize()) {
      BKE_report(reports, RPT_ERROR, "SPZ Read: Error initializing SH reader");
      ok = false;
    }
    else {
      ok = read_sh(reader, sh_attrs);
      if (!ok) {
        BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading spherical harmonics");
      }
    }
  }

  if (!ok) {
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
