/* SPDX-FileCopyrightText: 2005 `Gernot Ziegler <gz@lysator.liu.se>`. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup openexr
 */

#include "IMB_filetype.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

/* The OpenEXR version can reliably be found in this header file from OpenEXR,
 * for both 2.x and 3.x:
 */
#include <OpenEXR/OpenEXRConfig.h>
#define COMBINED_OPENEXR_VERSION \
  ((10000 * OPENEXR_VERSION_MAJOR) + (100 * OPENEXR_VERSION_MINOR) + OPENEXR_VERSION_PATCH)

#if COMBINED_OPENEXR_VERSION >= 20599
/* >=2.5.99 -> OpenEXR >=3.0 */
#  include <Imath/half.h>
#  include <OpenEXR/ImfFrameBuffer.h>
#  define exr_file_offset_t uint64_t
#else
/* OpenEXR 2.x, use the old locations. */
#  include <OpenEXR/half.h>
#  define exr_file_offset_t Int64
#endif

#include <OpenEXR/Iex.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfAttribute.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfChromaticities.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfIO.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfIntAttribute.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfPreviewImage.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfStandardAttributes.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfVersion.h>

/* multiview/multipart */
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfMultiPartOutputFile.h>
#include <OpenEXR/ImfMultiView.h>
#include <OpenEXR/ImfOutputPart.h>
#include <OpenEXR/ImfPartHelper.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfTiledOutputPart.h>

#include "DNA_scene_types.h" /* For OpenEXR compression constants */

#include <openexr_api.h>

#if defined(WIN32)
#  include "utfconv.hh"
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_half.hh"
#include "BLI_mmap.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_threads.h"

#include "BKE_blender_version.h"
#include "BKE_idprop.hh"
#include "BKE_image.hh"

#include "CLG_log.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_openexr.hh"

namespace blender {

const char *imb_file_extensions_openexr[] = {".exr", nullptr};

static CLG_LogRef LOG = {"image.openexr"};

using namespace Imf;
using namespace Imath;

/* prototype */
static bool exr_has_multiview(MultiPartInputFile &file);
static bool exr_has_multipart_file(MultiPartInputFile &file);
static bool exr_has_alpha(MultiPartInputFile &file);
static bool exr_has_channels(MultiPartInputFile &file);
static bool imb_exr_is_multi(MultiPartInputFile &file);
static const ColorSpace *imb_exr_part_colorspace(const Header &header);

/* XYZ with Illuminant E */
static Imf::Chromaticities CHROMATICITIES_XYZ_E{
    {1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f / 3.0f, 1.0f / 3.0f}};
/* Values matching ChromaticitiesForACES in https://github.com/ampas/aces_container */
static Imf::Chromaticities CHROMATICITIES_ACES_2065_1{
    {0.7347f, 0.2653f}, {0.0f, 1.0f}, {0.0001f, -0.077f}, {0.32168f, 0.33767f}};

/* Memory Input Stream */

class IMemStream : public Imf::IStream {
 public:
  IMemStream(uchar *exrbuf, size_t exrsize) : IStream("<memory>"), _exrpos(0), _exrsize(exrsize)
  {
    _exrbuf = exrbuf;
  }

  bool read(char c[], int n) override
  {
    if (n + _exrpos <= _exrsize) {
      memcpy(c, (void *)(&_exrbuf[_exrpos]), n);
      _exrpos += n;
      return true;
    }

    return false;
  }

  exr_file_offset_t tellg() override
  {
    return _exrpos;
  }

  void seekg(exr_file_offset_t pos) override
  {
    _exrpos = pos;
  }

  void clear() override {}

 private:
  exr_file_offset_t _exrpos;
  exr_file_offset_t _exrsize;
  uchar *_exrbuf;
};

/* Memory-Mapped Input Stream */

class IMMapStream : public Imf::IStream {
 public:
  IMMapStream(const char *filepath) : IStream(filepath)
  {
    const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
    if (file < 0) {
      throw IEX_NAMESPACE::InputExc("file not found");
    }
    _exrpos = 0;
    _mmap_file = BLI_mmap_open(file);
    close(file);
    if (_mmap_file == nullptr) {
      throw IEX_NAMESPACE::InputExc("BLI_mmap_open failed");
    }
    _exrsize = BLI_mmap_get_length(_mmap_file);
  }

  ~IMMapStream() override
  {
    BLI_mmap_free(_mmap_file);
  }

  /**
   * This is implementing regular `read`, not `readMemoryMapped`, because DWAA and DWAB
   * decompressors load on unaligned offsets. Therefore we can't avoid the memory copy.
   */
  bool read(char c[], int n) override
  {
    if (_exrpos + n > _exrsize) {
      throw Iex::InputExc("Unexpected end of file.");
    }

    if (!BLI_mmap_read(_mmap_file, c, _exrpos, n)) {
      throw Iex::InputExc("Error reading file.");
    }

    _exrpos += n;

    return _exrpos < _exrsize;
  }

  exr_file_offset_t tellg() override
  {
    return _exrpos;
  }

  void seekg(exr_file_offset_t pos) override
  {
    _exrpos = pos;
  }

 private:
  BLI_mmap_file *_mmap_file;
  exr_file_offset_t _exrpos;
  exr_file_offset_t _exrsize;
};

/* File Input Stream */

class IFileStream : public Imf::IStream {
 public:
  IFileStream(const char *filepath) : IStream(filepath)
  {
    /* UTF8 file path support on windows. */
#if defined(WIN32)
    wchar_t *wfilepath = alloc_utf16_from_8(filepath, 0);
    ifs.open(wfilepath, std::ios_base::binary);
    free(wfilepath);
#else
    ifs.open(filepath, std::ios_base::binary);
#endif

    if (!ifs) {
      Iex::throwErrnoExc();
    }
  }

  bool read(char c[], int n) override
  {
    if (!ifs) {
      throw Iex::InputExc("Unexpected end of file.");
    }

    errno = 0;
    ifs.read(c, n);
    return check_error();
  }

  exr_file_offset_t tellg() override
  {
    return std::streamoff(ifs.tellg());
  }

  void seekg(exr_file_offset_t pos) override
  {
    ifs.seekg(pos);
    check_error();
  }

  void clear() override
  {
    ifs.clear();
  }

 private:
  bool check_error()
  {
    if (!ifs) {
      if (errno) {
        Iex::throwErrnoExc();
      }

      return false;
    }

    return true;
  }

  std::ifstream ifs;
};

/* Memory Output Stream */

struct OMemStream : public OStream {
  OMemStream() : OStream("<memory>"), offset(0)
  {
    buffer.reserve(80 * 1024);
  }

  void write(const char c[], int n) override
  {
    ensure_size(offset + n);
    memcpy(buffer.data() + offset, c, n);
    offset += n;
  }

  exr_file_offset_t tellp() override
  {
    return offset;
  }

  void seekp(exr_file_offset_t pos) override
  {
    offset = pos;
    ensure_size(offset);
  }

  void ensure_size(exr_file_offset_t size)
  {
    if (size > buffer.size()) {
      buffer.resize(size);
    }
  }

  Vector<uint8_t> buffer;
  exr_file_offset_t offset;
};

/* File Output Stream */

class OFileStream : public OStream {
 public:
  OFileStream(const char *filepath) : OStream(filepath)
  {
    /* UTF8 file path support on windows. */
#if defined(WIN32)
    wchar_t *wfilepath = alloc_utf16_from_8(filepath, 0);
    ofs.open(wfilepath, std::ios_base::binary);
    free(wfilepath);
#else
    ofs.open(filepath, std::ios_base::binary);
#endif

    if (!ofs) {
      Iex::throwErrnoExc();
    }
  }

  void write(const char c[], int n) override
  {
    errno = 0;
    ofs.write(c, n);
    check_error();
  }

  exr_file_offset_t tellp() override
  {
    return std::streamoff(ofs.tellp());
  }

  void seekp(exr_file_offset_t pos) override
  {
    ofs.seekp(pos);
    check_error();
  }

 private:
  void check_error()
  {
    if (!ofs) {
      if (errno) {
        Iex::throwErrnoExc();
      }

      throw Iex::ErrnoExc("File output failed.");
    }
  }

  std::ofstream ofs;
};

bool imb_is_a_openexr(const uchar *mem, const size_t size)
{
  /* No define is exposed for this size. */
  if (size < 4) {
    return false;
  }
  return Imf::isImfMagic((const char *)mem);
}

static int openexr_jpg_like_quality_to_dwa_compression_level(int q)
{
  q = math::clamp(q, 0, 100);

  /* Map default JPG quality of 90 to default DWA level of 45,
   * "lossless" JPG quality of 100 to DWA level of 0, and everything else
   * linearly based on those. */
  constexpr int x0 = 100, y0 = 0;
  constexpr int x1 = 90, y1 = 45;
  q = y0 + (q - x0) * (y1 - y0) / (x1 - x0);
  return q;
}

static float compression_half_max(const int compression, const int quality)
{
  if (ELEM(compression, R_IMF_EXR_CODEC_DWAA, R_IMF_EXR_CODEC_DWAB)) {
    /* Empirically determined margin to prevent DWAA/DWAB lossy compression
     * from overshooting to infinity. The DWA compression uses log2-luminance
     * DCT, and overshoot increases with the compression level.
     *
     * Tested with randomized pixel patterns across various compression levels
     * to find a tight bound. */
    const int level = openexr_jpg_like_quality_to_dwa_compression_level(quality);
    const float margin = (level <= 50) ? (2048.0f + 32.0f * level) :
                                         (3624.0f + 128.0f * (level - 50));
    return HALF_MAX - margin;
  }

  return HALF_MAX;
}

static void openexr_header_compression(Header *header, int compression, int quality)
{
  switch (compression) {
    case R_IMF_EXR_CODEC_NONE:
      header->compression() = NO_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_PXR24:
      header->compression() = PXR24_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_ZIP:
      header->compression() = ZIP_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_PIZ:
      header->compression() = PIZ_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_RLE:
      header->compression() = RLE_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_ZIPS:
      header->compression() = ZIPS_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_B44:
      header->compression() = B44_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_B44A:
      header->compression() = B44A_COMPRESSION;
      break;
#if OPENEXR_VERSION_MAJOR > 2 || (OPENEXR_VERSION_MAJOR >= 2 && OPENEXR_VERSION_MINOR >= 2)
    case R_IMF_EXR_CODEC_DWAA:
      header->compression() = DWAA_COMPRESSION;
      header->dwaCompressionLevel() = openexr_jpg_like_quality_to_dwa_compression_level(quality);
      break;
    case R_IMF_EXR_CODEC_DWAB:
      header->compression() = DWAB_COMPRESSION;
      header->dwaCompressionLevel() = openexr_jpg_like_quality_to_dwa_compression_level(quality);
      break;
#endif
#if COMBINED_OPENEXR_VERSION >= 30400
      /* Always writing 32 scan-lines for now, could add an option for 256 scan-lines
       * for slightly smaller files if there is demand for it. */
    case R_IMF_EXR_CODEC_HTJ2K:
      header->compression() = HTJ2K32_COMPRESSION;
      break;
#endif
    default:
      header->compression() = ZIP_COMPRESSION;
      break;
  }
}

static int openexr_header_get_compression(const Header &header)
{
  switch (header.compression()) {
    case NO_COMPRESSION:
      return R_IMF_EXR_CODEC_NONE;
    case RLE_COMPRESSION:
      return R_IMF_EXR_CODEC_RLE;
    case ZIPS_COMPRESSION:
      return R_IMF_EXR_CODEC_ZIPS;
    case ZIP_COMPRESSION:
      return R_IMF_EXR_CODEC_ZIP;
    case PIZ_COMPRESSION:
      return R_IMF_EXR_CODEC_PIZ;
    case PXR24_COMPRESSION:
      return R_IMF_EXR_CODEC_PXR24;
    case B44_COMPRESSION:
      return R_IMF_EXR_CODEC_B44;
    case B44A_COMPRESSION:
      return R_IMF_EXR_CODEC_B44A;
    case DWAA_COMPRESSION:
      return R_IMF_EXR_CODEC_DWAA;
    case DWAB_COMPRESSION:
      return R_IMF_EXR_CODEC_DWAB;
#if COMBINED_OPENEXR_VERSION >= 30400
    case HTJ2K256_COMPRESSION:
    case HTJ2K32_COMPRESSION:
      return R_IMF_EXR_CODEC_HTJ2K;
#endif
    case NUM_COMPRESSION_METHODS:
      return R_IMF_EXR_CODEC_NONE;
  }
  return R_IMF_EXR_CODEC_NONE;
}

static void openexr_header_metadata_global(Header *header, const IDProperty *metadata)
{
  header->insert(
      "Software",
      TypedAttribute<std::string>(std::string("Blender ") + BKE_blender_version_string()));

  if (metadata) {
    for (IDProperty &prop : metadata->data.group) {
      /* Do not blindly pass along compression or colorInteropID, as they might have
       * changed and will already be written when appropriate. */
      if ((prop.type == IDP_STRING) && !STR_ELEM(prop.name, "compression", "colorInteropID")) {
        header->insert(prop.name, StringAttribute(IDP_string_get(&prop)));
      }
    }
  }
}

static void openexr_header_metadata_pixelinfo(Header *header, const double ppm[2])
{
  if (ppm[0] > 0.0 && ppm[1] > 0.0) {
    /* Convert meters to inches. */
    addXDensity(*header, ppm[0] * 0.0254);
    header->pixelAspectRatio() = math::safe_divide(ppm[1], ppm[0]);
  }
}

static void openexr_header_metadata_colorspace(Header *header, const ColorSpace *colorspace)
{
  if (colorspace == nullptr) {
    return;
  }

  const char *aces_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_ACES_INTERCHANGE);
  const char *ibuf_colorspace = IMB_colormanagement_colorspace_get_name(colorspace);

  /* Write chromaticities for ACES-2065-1, as required by ACES container format. */
  if (aces_colorspace && STREQ(aces_colorspace, ibuf_colorspace)) {
    header->insert("chromaticities", TypedAttribute<Chromaticities>(CHROMATICITIES_ACES_2065_1));
    header->insert("adoptedNeutral", TypedAttribute<V2f>(CHROMATICITIES_ACES_2065_1.white));
  }

  /* Write interop ID if available. */
  StringRefNull interop_id = IMB_colormanagement_space_get_interop_id(colorspace);
  if (!interop_id.is_empty()) {
    header->insert("colorInteropID", TypedAttribute<std::string>(interop_id));
  }
}

static void openexr_header_metadata_colorspace(Header *header, const ImBuf *ibuf)
{
  /* Get colorspace from image buffer. */
  const ColorSpace *colorspace = nullptr;
  if (ibuf->float_data()) {
    colorspace = ibuf->float_buffer.colorspace;
    if (colorspace == nullptr) {
      colorspace = IMB_colormanagement_space_get_named(
          IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR));
    }
  }
  else if (ibuf->byte_data()) {
    colorspace = ibuf->byte_buffer.colorspace;
  }

  openexr_header_metadata_colorspace(header, colorspace);
}

static void openexr_header_metadata_callback(void *data,
                                             const char *propname,
                                             char *prop,  // NOLINT
                                             int /*len*/)
{
  Header *header = (Header *)data;
  header->insert(propname, StringAttribute(prop));
}

struct RGBAHalf {
  uint16_t r;
  uint16_t g;
  uint16_t b;
  uint16_t a;
};

static void convert_input_to_half_rgba(const ImBuf *ibuf, RGBAHalf *dst)
{
  const int channels = ibuf->channels;
  const bool is_alpha = (channels >= 4) && (ibuf->can_contain_alpha());
  const int width = ibuf->x;
  const int height = ibuf->y;

  if (ibuf->float_data()) {
    const float *float_data = ibuf->float_data();

    const int compression = ibuf->foptions.flag & OPENEXR_CODEC_MASK;
    const float half_max_val = compression_half_max(compression, ibuf->foptions.quality);
    const int comp_r = 0;
    const int comp_g = channels >= 2 ? 1 : 0;
    const int comp_b = channels >= 3 ? 2 : 0;
    const int comp_a = channels >= 4 ? 3 : 0;

    threading::parallel_for_each(IndexRange(height), [&](int64_t y) {
      Array<float4> row_buffer(width);
      RGBAHalf *to_row = dst + y * width;

      const float *from = float_data + int64_t(channels) * (ibuf->y - 1 - y) * width;
      for (int x = 0; x < ibuf->x; x++) {
        row_buffer[x] = float4(from[comp_r], from[comp_g], from[comp_b], from[comp_a]);
        from += channels;
      }
      math::float_to_half_clamp_array(
          &row_buffer.data()->x, &to_row->r, 4 * width, -half_max_val, half_max_val);
    });
  }
  else {
    uint16_t color_to_half[256];
    uint16_t alpha_to_half[256];
    for (int v = 0; v < 256; v++) {
      color_to_half[v] = math::float_to_half(BLI_color_from_srgb_table[v]);
      alpha_to_half[v] = math::float_to_half(float(v) / 255.0f);
    }

    const uchar *byte_data = ibuf->byte_data();

    for (int i = ibuf->y - 1; i >= 0; i--) {
      const uchar *from = byte_data + int64_t(4) * i * width;

      for (int j = ibuf->x; j > 0; j--) {
        dst->r = color_to_half[from[0]];
        dst->g = color_to_half[from[1]];
        dst->b = color_to_half[from[2]];
        dst->a = is_alpha ? alpha_to_half[from[3]] : 0x3c00; /* 0x3c00 = FP16 1.0 */
        dst++;
        from += 4;
      }
    }
  }
}

static void save_setup_header(const ImBuf *ibuf,
                              bool half_precision,
                              bool is_alpha,
                              Header &header)
{
  const int compression = ibuf->foptions.flag & OPENEXR_CODEC_MASK;
  openexr_header_compression(&header, compression, ibuf->foptions.quality);
  openexr_header_metadata_global(&header, ibuf->metadata());
  openexr_header_metadata_pixelinfo(&header, ibuf->ppm);
  openexr_header_metadata_colorspace(&header, ibuf);

  /* create channels */
  Channel channel(half_precision ? HALF : FLOAT);
  header.channels().insert("R", channel);
  header.channels().insert("G", channel);
  header.channels().insert("B", channel);
  if (is_alpha) {
    header.channels().insert("A", channel);
  }
}

static void save_setup_framebuffer(const ImBuf *ibuf,
                                   bool half_precision,
                                   bool is_alpha,
                                   Vector<RGBAHalf> &half_pixels,
                                   FrameBuffer &frameBuffer)
{
  const int width = ibuf->x;
  const int height = ibuf->y;
  const int channels = ibuf->channels;

  if (half_precision) {
    half_pixels.resize(int64_t(height) * width);
    RGBAHalf *to = half_pixels.data();
    int xstride = sizeof(RGBAHalf);
    int ystride = xstride * width;
    frameBuffer.insert("R", Slice(HALF, (char *)&to->r, xstride, ystride));
    frameBuffer.insert("G", Slice(HALF, (char *)&to->g, xstride, ystride));
    frameBuffer.insert("B", Slice(HALF, (char *)&to->b, xstride, ystride));
    if (is_alpha) {
      frameBuffer.insert("A", Slice(HALF, (char *)&to->a, xstride, ystride));
    }

    convert_input_to_half_rgba(ibuf, half_pixels.data());
  }
  else {
    int xstride = sizeof(float) * channels;
    int ystride = -xstride * width;

    /* Last scan-line, stride negative. */
    const float *src_data = ibuf->float_data() + int64_t(channels) * (height - 1) * width;
    frameBuffer.insert("R", Slice(FLOAT, (char *)src_data, xstride, ystride));
    frameBuffer.insert(
        "G", Slice(FLOAT, (char *)(src_data + (channels >= 2 ? 1 : 0)), xstride, ystride));
    frameBuffer.insert(
        "B", Slice(FLOAT, (char *)(src_data + (channels >= 3 ? 2 : 0)), xstride, ystride));
    if (is_alpha) {
      frameBuffer.insert("A", Slice(FLOAT, (char *)(src_data + 3), xstride, ystride));
    }
  }
}

bool imb_save_openexr(ImBuf *ibuf, const char *filepath, ImBufFlags /*flags*/)
{
  /* Use half precision when asked for it, or if source is a
   * byte image (half precision is always enough for that case). */
  const bool half_precision = (ibuf->foptions.flag & OPENEXR_HALF) ||
                              ibuf->float_data() == nullptr;
  const bool is_alpha = (ibuf->channels >= 4) && (ibuf->can_contain_alpha());
  try {
    Header header(ibuf->x, ibuf->y);
    save_setup_header(ibuf, half_precision, is_alpha, header);

    OFileStream file_stream(filepath);
    OutputFile file(file_stream, header);

    FrameBuffer frameBuffer;
    Vector<RGBAHalf> half_pixels;
    save_setup_framebuffer(ibuf, half_precision, is_alpha, half_pixels, frameBuffer);
    file.setFrameBuffer(frameBuffer);
    file.writePixels(ibuf->y);
    return true;
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
    return false;
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
    return false;
  }
}

Vector<uint8_t> imb_save_buffer_openexr(ImBuf *ibuf, ImBufFlags /*flags*/)
{
  /* Use half precision when asked for it, or if source is a
   * byte image (half precision is always enough for that case). */
  const bool half_precision = (ibuf->foptions.flag & OPENEXR_HALF) ||
                              ibuf->float_data() == nullptr;

  const bool is_alpha = (ibuf->channels >= 4) && (ibuf->can_contain_alpha());
  try {
    Header header(ibuf->x, ibuf->y);
    save_setup_header(ibuf, half_precision, is_alpha, header);

    OMemStream mem_stream;
    OutputFile file(mem_stream, header);

    FrameBuffer frameBuffer;
    Vector<RGBAHalf> half_pixels;
    save_setup_framebuffer(ibuf, half_precision, is_alpha, half_pixels, frameBuffer);
    file.setFrameBuffer(frameBuffer);
    file.writePixels(ibuf->y);
    return std::move(mem_stream.buffer);
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }
  return {};
}

/* ******* Nicer API, MultiLayer and with Tile file support ************************************ */

/* naming rules:
 * - parse name from right to left
 * - last character is channel ID, 1 char like 'A' 'R' 'G' 'B' 'X' 'Y' 'Z' 'W' 'U' 'V'
 * - separated with a dot; the Pass name (like "Depth", "Color", "Diffuse" or "Combined")
 * - separated with a dot: the Layer name (like "Light1" or "Walls" or "Characters")
 */

/** Flattened out channel. */
struct ExrChannel {
  /** Name and number of the part. */
  std::string part_name;
  int part_number = 0;

  /** Full name of the channel. */
  std::string name;
  /** Name as stored in the header. */
  std::string internal_name;
  /** Channel view. */
  std::string view;

  /** Color-space. */
  const ColorSpace *colorspace;

  /** Step to next pixel. */
  int xstride = 0;
  /** Step to next scan-line. */
  int ystride = 0;
  /** Channel index within the pass buffer. */
  int offset = 0;
  /** First pointer to write in */
  float *rect = nullptr;
  /** Quick lookup of channel char */
  char chan_id = 0;
  /** When saving use half float for file storage. */
  bool use_half_float = false;
};

/** Hierarchical; layers -> passes -> channels[]. */
struct ExrPass {
  std::string name;
  int totchan = 0;
  ExrChannel *chan[EXR_PASS_MAXCHAN] = {};
  /* Extra byte for null termination of string. */
  char chan_id[EXR_PASS_MAXCHAN + 1] = {};

  /** Name with no view. */
  std::string internal_name;
  std::string view;
};

struct ExrLayer {
  std::string name;
  Vector<ExrPass> passes;
};

struct ExrReadHandle {
  IStream *ifile_stream = nullptr;
  MultiPartInputFile *ifile = nullptr;

  int width = 0, height = 0;

  StringVector views;

  /** Flat list of channels in the file. */
  Vector<ExrChannel> channels;
  /** Layers and passes. */
  Vector<ExrLayer> layers;
  /** True once the layer/pass info has been parsed from the file header.
   * Parsing is deferred to the first call that needs its. */
  bool channels_parsed = false;
};

struct ExrWriteHandle {
  OFileStream *ofile_stream = nullptr;
  MultiPartOutputFile *mpofile = nullptr;
  OutputFile *ofile = nullptr;

  bool write_multipart = false;
  bool has_layer_pass_names = false;
  float half_max_val = HALF_MAX;

  int width = 0, height = 0;

  StringVector views;

  /** Channels to write. */
  Vector<ExrChannel> channels;
};

static bool imb_exr_multilayer_ensure_channels_parsed(ExrReadHandle *handle);
static Vector<ExrChannel> exr_channels_in_multi_part_file(const MultiPartInputFile &file);
static void imb_exr_write_channels(ExrWriteHandle *handle);

/* ********************** */

ExrWriteHandle *IMB_exr_write_begin(const bool write_multipart)
{
  ExrWriteHandle *handle = MEM_new<ExrWriteHandle>("ExrWriteHandle");
  handle->write_multipart = write_multipart;
  return handle;
}

/* multiview functions */

void IMB_exr_write_view(ExrWriteHandle *handle, const char *viewname)
{
  handle->views.emplace_back(viewname);
}

static int imb_exr_get_multiView_id(StringVector &views, const std::string &name)
{
  int count = 0;
  for (StringVector::const_iterator i = views.begin(); count < views.size(); ++i) {
    if (name == *i) {
      return count;
    }

    count++;
  }

  /* no views or wrong name */
  return -1;
}

static StringVector imb_exr_get_views(MultiPartInputFile &file)
{
  StringVector views;

  for (int p = 0; p < file.parts(); p++) {
    /* Views stored in separate parts. */
    if (file.header(p).hasView()) {
      const std::string &view = file.header(p).view();
      if (imb_exr_get_multiView_id(views, view) == -1) {
        views.push_back(view);
      }
    }

    /* Part containing multiple views. */
    if (hasMultiView(file.header(p))) {
      StringVector multiview = multiView(file.header(p));
      for (const std::string &view : multiview) {
        if (imb_exr_get_multiView_id(views, view) == -1) {
          views.push_back(view);
        }
      }
    }
  }

  return views;
}

void IMB_exr_write_pass(ExrWriteHandle *handle,
                        StringRefNull layerpassname,
                        StringRefNull channelnames,
                        StringRefNull viewname,
                        StringRefNull colorspace,
                        size_t xstride,
                        size_t ystride,
                        const float *rect,
                        bool use_half_float)
{
  /* For multipart, part name includes view since part names must be unique. */
  std::string part_name;
  if (handle->write_multipart) {
    part_name = layerpassname;
    if (!viewname.is_empty()) {
      if (part_name.empty()) {
        part_name = viewname;
      }
      else {
        part_name = part_name + "-" + viewname;
      }
    }
  }

  /* If there are layer and pass names, we will write Blender multichannel metadata. */
  if (!layerpassname.is_empty()) {
    handle->has_layer_pass_names = true;
  }

  for (size_t channel = 0; channel < channelnames.size(); channel++) {
    /* Full channel name including view (when not using multipart) and channel. */
    std::string full_name = layerpassname;
    if (!handle->write_multipart && !viewname.is_empty()) {
      if (full_name.empty()) {
        full_name = viewname;
      }
      else {
        full_name = full_name + "." + viewname;
      }
    }
    if (full_name.empty()) {
      full_name = channelnames[channel];
    }
    else {
      full_name = full_name + "." + channelnames[channel];
    }

    handle->channels.append_as();
    ExrChannel &echan = handle->channels.last();

    echan.name = full_name;
    echan.internal_name = full_name;
    echan.part_name = part_name;
    echan.view = viewname;
    echan.colorspace = IMB_colormanagement_space_get_named(colorspace.c_str());

    echan.xstride = xstride;
    echan.ystride = ystride;
    /* This is used for writing, the data should not be modified. ????????? */
    echan.rect = const_cast<float *>(rect + channel);
    echan.use_half_float = use_half_float;
  }

  CLOG_DEBUG(&LOG, "Added pass %s %s", layerpassname.c_str(), channelnames.c_str());
}

static void openexr_header_metadata_multi(ExrWriteHandle *handle,
                                          Header &header,
                                          const StampData *stamp)
{
  openexr_header_metadata_global(&header, nullptr);
  if (handle->has_layer_pass_names) {
    header.insert("BlenderMultiChannel", StringAttribute("Blender V2.55.1 and newer"));
  }
  if (!handle->write_multipart && !handle->views.empty() && !handle->views[0].empty()) {
    addMultiView(header, handle->views);
  }
  BKE_stamp_info_callback(
      &header, const_cast<StampData *>(stamp), openexr_header_metadata_callback, false);
}

bool IMB_exr_write_end(ExrWriteHandle *handle,
                       const char *filepath,
                       int width,
                       int height,
                       const double ppm[2],
                       int compress,
                       int quality,
                       const StampData *stamp)
{
  if (handle->channels.is_empty()) {
    CLOG_ERROR(&LOG, "Attempt to save MultiLayer without layers.");
    MEM_delete(handle);
    return false;
  }

  bool ok = true;

  try {
    Header header(width, height);

    handle->width = width;
    handle->height = height;

    openexr_header_compression(&header, compress, quality);
    handle->half_max_val = compression_half_max(compress, quality);

    openexr_header_metadata_pixelinfo(&header, ppm);

    if (!handle->write_multipart) {
      /* If we're writing single part, we can only add one colorspace even if there are
       * multiple passes with potentially different spaces. Prefer to write non-data
       * colorspace in that case, since readers can detect data passes based on
       * channels names being e.g. XYZ instead of RGB. */
      bool found = false;
      for (const ExrChannel &echan : handle->channels) {
        if (echan.colorspace && !IMB_colormanagement_space_is_data(echan.colorspace)) {
          openexr_header_metadata_colorspace(&header, echan.colorspace);
          found = true;
          break;
        }
      }
      if (!found) {
        if (const ColorSpace *colorspace = handle->channels[0].colorspace) {
          openexr_header_metadata_colorspace(&header, colorspace);
        }
      }
    }

    Vector<Header> part_headers;

    StringRefNull last_part_name;

    for (const ExrChannel &echan : handle->channels) {
      if (part_headers.is_empty() || last_part_name != echan.part_name) {
        Header part_header = header;

        /* When writing multipart, set name, view,type and colorspace in each part. */
        if (handle->write_multipart) {
          part_header.setName(echan.part_name);
          if (!echan.view.empty()) {
            part_header.insert("view", StringAttribute(echan.view));
          }
          part_header.insert("type", StringAttribute(SCANLINEIMAGE));
          openexr_header_metadata_colorspace(&part_header, echan.colorspace);
        }

        /* Store global metadata in the first header only. Large metadata like cryptomatte would
         * be bad to duplicate many times. */
        if (part_headers.is_empty()) {
          openexr_header_metadata_multi(handle, part_header, stamp);
        }

        part_headers.append(std::move(part_header));
        last_part_name = echan.part_name;
      }

      part_headers.last().channels().insert(
          echan.name, Channel(echan.use_half_float ? Imf::HALF : Imf::FLOAT));
    }

    BLI_assert(!(handle->write_multipart == false && part_headers.size() > 1));

    /* Manually create `ofstream`, so we can handle UTF8 file-paths on windows. */
    handle->ofile_stream = new OFileStream(filepath);
    if (handle->write_multipart) {
      handle->mpofile = new MultiPartOutputFile(
          *(handle->ofile_stream), part_headers.data(), part_headers.size());
    }
    else {
      handle->ofile = new OutputFile(*(handle->ofile_stream), part_headers[0]);
    }

    imb_exr_write_channels(handle);
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
    ok = false;
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
    ok = false;
  }

  delete handle->ofile;
  delete handle->mpofile;
  delete handle->ofile_stream;

  MEM_delete(handle);

  return ok;
}

ExrReadHandle *IMB_exr_open(const char *filepath)
{
  /* 32 is arbitrary, but zero length files crashes exr. */
  if (!(BLI_exists(filepath) && BLI_file_size(filepath) > 32)) {
    return nullptr;
  }

  ExrReadHandle *handle = MEM_new<ExrReadHandle>("ExrReadHandle");
  bool ok = true;

  /* avoid crash/abort when we don't have permission to write here */
  try {
    handle->ifile_stream = new IFileStream(filepath);
    handle->ifile = new MultiPartInputFile(*(handle->ifile_stream));

    const Box2i dw = handle->ifile->header(0).dataWindow();
    handle->width = dw.max.x - dw.min.x + 1;
    handle->height = dw.max.y - dw.min.y + 1;
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
    ok = false;
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
    ok = false;
  }

  if (!ok) {
    delete handle->ifile;
    delete handle->ifile_stream;

    handle->ifile = nullptr;
    handle->ifile_stream = nullptr;

    IMB_exr_close(handle);
    return nullptr;
  }

  return handle;
}

static void imb_exr_write_channels(ExrWriteHandle *handle)
{
  if (handle->channels.is_empty()) {
    CLOG_ERROR(&LOG, "Attempt to save MultiLayer without layers.");
    return;
  }

  const size_t num_pixels = size_t(handle->width) * handle->height;
  const size_t num_parts = (handle->mpofile) ? handle->mpofile->parts() : 1;

  for (size_t part_num = 0; part_num < num_parts; part_num++) {
    const std::string &part_id = (handle->mpofile) ? handle->mpofile->header(part_num).name() : "";
    /* We allocate temporary storage for half pixels for all the channels at once. */
    int num_half_channels = 0;
    for (const ExrChannel &echan : handle->channels) {
      if (echan.part_name == part_id && echan.use_half_float) {
        num_half_channels++;
      }
    }

    Vector<uint16_t> rect_half;
    uint16_t *current_rect_half = nullptr;
    if (num_half_channels > 0) {
      rect_half.resize(size_t(num_half_channels) * num_pixels);
      current_rect_half = rect_half.data();
    }

    FrameBuffer frameBuffer;

    for (const ExrChannel &echan : handle->channels) {
      /* Writing starts from last scan-line, stride negative. */
      if (echan.part_name != part_id) {
        continue;
      }

      if (echan.use_half_float) {
        const float *src_float = echan.rect;
        /* Convert & clamp input floats to half-floats. */
        threading::parallel_for(IndexRange(num_pixels), 16 * 1024, [&](IndexRange range) {
          Array<float> gathered_floats(range.size());
          int64_t i = 0;
          for (int64_t index : range) {
            gathered_floats[i++] = src_float[index * echan.xstride];
          }
          math::float_to_half_clamp_array(gathered_floats.data(),
                                          current_rect_half + range.first(),
                                          range.size(),
                                          -handle->half_max_val,
                                          handle->half_max_val);
        });

        uint16_t *rect_to_write = current_rect_half + (handle->height - 1L) * handle->width;
        frameBuffer.insert(
            echan.name,
            Slice(Imf::HALF, (char *)rect_to_write, sizeof(half), -handle->width * sizeof(half)));
        current_rect_half += num_pixels;
      }
      else {
        float *rect = echan.rect + echan.xstride * (handle->height - 1L) * handle->width;
        frameBuffer.insert(echan.name,
                           Slice(Imf::FLOAT,
                                 (char *)rect,
                                 echan.xstride * sizeof(float),
                                 -echan.ystride * sizeof(float)));
      }
    }

    try {
      if (handle->mpofile) {
        OutputPart part(*handle->mpofile, part_num);
        part.setFrameBuffer(frameBuffer);
        part.writePixels(handle->height);
      }
      else {
        handle->ofile->setFrameBuffer(frameBuffer);
        handle->ofile->writePixels(handle->height);
      }
    }
    catch (const std::exception &exc) {
      CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
    }
    catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
      CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
    }
  }
}

/** Read pixels for channels that have a rect buffer set. */
static void imb_exr_read_channels(ExrReadHandle *handle)
{
  try {
    int numparts = handle->ifile->parts();

    /* Check if EXR was saved with previous versions of blender which flipped images. */
    const StringAttribute *ta = handle->ifile->header(0).findTypedAttribute<StringAttribute>(
        "BlenderMultiChannel");

    /* 'previous multilayer attribute, flipped. */
    short flip = (ta && STRPREFIX(ta->value().c_str(), "Blender V2.43"));

    CLOG_DEBUG(&LOG,
               "\nIMB_exr_read_channels\n%s %-6s %-22s "
               "\"%s\"\n---------------------------------------------------------------------",
               "p",
               "view",
               "name",
               "internal_name");

    for (int i = 0; i < numparts; i++) {
      /* Skip parts from which we don't need any channel. */
      bool part_has_buffer = false;
      for (const ExrChannel &echan : handle->channels) {
        if (echan.part_number == i && echan.rect) {
          part_has_buffer = true;
          break;
        }
      }
      if (!part_has_buffer) {
        continue;
      }

      try {
        /* Read part header. */
        InputPart in(*handle->ifile, i);
        Header header = in.header();
        Box2i dw = header.dataWindow();

        /* Insert all matching channel into frame-buffer. */
        FrameBuffer frameBuffer;

        for (const ExrChannel &echan : handle->channels) {
          if (echan.part_number != i) {
            continue;
          }

          CLOG_DEBUG(&LOG,
                     "%d %-6s %-22s \"%s\"\n",
                     echan.part_number,
                     echan.view.c_str(),
                     echan.name.c_str(),
                     echan.internal_name.c_str());

          if (echan.rect) {
            float *rect = echan.rect;
            size_t xstride = echan.xstride * sizeof(float);
            size_t ystride = echan.ystride * sizeof(float);

            if (!flip) {
              /* Inverse correct first pixel for data-window coordinates. */
              rect -= echan.xstride * (dw.min.x - dw.min.y * handle->width);
              /* Move to last scan-line to flip to Blender convention. */
              rect += echan.xstride * (handle->height - 1) * handle->width;
              ystride = -ystride;
            }
            else {
              /* Inverse correct first pixel for data-window coordinates. */
              rect -= echan.xstride * (dw.min.x + dw.min.y * handle->width);
            }

            frameBuffer.insert(echan.internal_name,
                               Slice(Imf::FLOAT, (char *)rect, xstride, ystride));
          }
        }

        /* Read pixels. */
        in.setFrameBuffer(frameBuffer);
        CLOG_DEBUG(&LOG, "readPixels:readPixels[%d]: min.y: %d, max.y: %d", i, dw.min.y, dw.max.y);
        in.readPixels(dw.min.y, dw.max.y);
      }
      catch (const std::exception &exc) {
        CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
        break;
      }
      catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
        CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
        break;
      }
    }
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }
}

int2 IMB_exr_get_size(ExrReadHandle *handle)
{
  return int2(handle->width, handle->height);
}

/* Stamp ppm + display window from the EXR file header onto the ImBuf. */
static void imb_exr_set_ibuf_display_window(MultiPartInputFile &file, ImBuf *ibuf);

Vector<ExrPassInfo> IMB_exr_get_passes(ExrReadHandle *handle)
{
  imb_exr_multilayer_ensure_channels_parsed(handle);
  Vector<ExrPassInfo> out;
  for (ExrLayer &lay : handle->layers) {
    for (ExrPass &pass : lay.passes) {
      if (pass.totchan == 0) {
        continue;
      }
      out.append({.layer = lay.name,
                  .pass = pass.internal_name,
                  .view = pass.view,
                  .chan_id = StringRefNull(pass.chan_id, strnlen(pass.chan_id, EXR_PASS_MAXCHAN)),
                  .channels = pass.totchan});
    }
  }
  return out;
}

Vector<StringRefNull> IMB_exr_get_views(ExrReadHandle *handle)
{
  imb_exr_multilayer_ensure_channels_parsed(handle);
  Vector<StringRefNull> out;
  for (const std::string &view : handle->views) {
    out.append(view);
  }
  return out;
}

void IMB_exr_read_passes(ExrReadHandle *handle,
                         MutableSpan<ExrPassInfo> entries,
                         const char *src_colorspace,
                         const bool predivide)
{
  if (handle == nullptr || entries.is_empty()) {
    return;
  }

  imb_exr_multilayer_ensure_channels_parsed(handle);

  const int width = handle->width;
  const int height = handle->height;

  /* Find matching layer and pass for every pass info to read .*/
  for (ExrPassInfo &entry : entries) {
    bool found = false;
    for (ExrLayer &lay : handle->layers) {
      if (lay.name != entry.layer) {
        continue;
      }
      for (ExrPass &pass : lay.passes) {
        if (pass.internal_name != entry.pass || pass.view != entry.view) {
          continue;
        }

        /* Allocate a destination ImBuf if the caller didn't provide one. */
        if (entry.ibuf == nullptr) {
          entry.ibuf = IMB_allocImBuf(width, height, ImBufFlags::Zero);
          if (entry.ibuf == nullptr) {
            break;
          }
          entry.ibuf->color_mode = IMB_color_mode_from_channels(pass.totchan);
          if (!IMB_alloc_float_pixels(entry.ibuf, pass.totchan)) {
            IMB_freeImBuf(entry.ibuf);
            entry.ibuf = nullptr;
            found = true;
            break;
          }
        }
        else {
          BLI_assert(entry.ibuf->channels == pass.totchan && entry.ibuf->x == width &&
                     entry.ibuf->y == height);
        }

        /* Populate per channel pixel pointers. */
        float *data = entry.ibuf->float_data_for_write();
        for (int a = 0; a < pass.totchan; a++) {
          ExrChannel &echan = *pass.chan[a];
          echan.rect = data + echan.offset;
        }
        found = true;
        break;
      }
      if (found) {
        break;
      }
    }
  }

  /* Read the requested channels. */
  imb_exr_read_channels(handle);

  /* Clear per channel pixel pointers again. */
  for (ExrLayer &lay : handle->layers) {
    for (ExrPass &pass : lay.passes) {
      for (int a = 0; a < pass.totchan; a++) {
        pass.chan[a]->rect = nullptr;
      }
    }
  }

  for (ExrPassInfo &entry : entries) {
    if (entry.ibuf == nullptr) {
      continue;
    }

    /* Add metadata. */
    imb_exr_set_ibuf_display_window(*handle->ifile, entry.ibuf);

    /* Convert and assign colorspace. */
    if (src_colorspace) {
      if (IMB_chan_id_is_color(entry.chan_id)) {
        const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
            COLOR_ROLE_SCENE_LINEAR);
        IMB_colormanagement_transform_float(entry.ibuf->float_data_for_write(),
                                            entry.ibuf->x,
                                            entry.ibuf->y,
                                            entry.ibuf->channels,
                                            src_colorspace,
                                            to_colorspace,
                                            predivide);
      }
      else {
        const char *data_colorspace = IMB_colormanagement_role_colorspace_name_get(
            COLOR_ROLE_DATA);
        IMB_colormanagement_assign_float_colorspace(entry.ibuf, data_colorspace);
      }
    }
  }
}

static bool imb_exr_multi_read_single_pass(ExrReadHandle *handle, ImBuf *ibuf)
{
  Vector<ExrPassInfo> passes = IMB_exr_get_passes(handle);
  if (passes.is_empty()) {
    return false;
  }

  /* Prefer the combined pass, then any other RGB(A) pass, then the first pass. */
  ExrPassInfo *combined = nullptr;
  ExrPassInfo *rgb = nullptr;
  for (ExrPassInfo &info : passes) {
    if (info.pass == RE_PASSNAME_COMBINED) {
      combined = &info;
      break;
    }
    if (rgb == nullptr && (info.chan_id == "RGBA" || info.chan_id == "RGB")) {
      rgb = &info;
    }
  }
  ExrPassInfo *chosen = combined ? combined : rgb ? rgb : &passes.first();

  /* Read pixels. */
  if (!IMB_alloc_float_pixels(ibuf, chosen->channels)) {
    return false;
  }
  ibuf->color_mode = IMB_color_mode_from_channels(chosen->channels);
  chosen->ibuf = ibuf;

  MutableSpan<ExrPassInfo> single(chosen, 1);
  IMB_exr_read_passes(handle, single, nullptr, false);
  return true;
}

void IMB_exr_close(ExrReadHandle *handle)
{
  delete handle->ifile;
  delete handle->ifile_stream;

  MEM_delete(handle);
}

/* ********* */

/** Get a sub-string from the end of the name, separated by '.'. */
static int imb_exr_split_token(const char *str, const char *end, const char **token)
{
  const char delims[] = {'.', '\0'};
  const char *sep;

  BLI_str_partition_ex(str, end, delims, &sep, token, true);

  if (!sep) {
    *token = str;
  }

  return int(end - *token);
}

static void imb_exr_pass_name_from_channel(char *passname,
                                           const ExrChannel &echan,
                                           const char *channelname,
                                           const bool has_xyz_channels)
{
  const int passname_maxncpy = EXR_TOT_MAXNAME;

  if (echan.chan_id == 'Z' && (!has_xyz_channels || BLI_strcaseeq(channelname, "depth"))) {
    BLI_strncpy(passname, "Depth", passname_maxncpy);
  }
  else if (echan.chan_id == 'Y' && !has_xyz_channels) {
    BLI_strncpy(passname, channelname, passname_maxncpy);
  }
  else if (ELEM(echan.chan_id, 'R', 'G', 'B', 'A', 'V', 'X', 'Y', 'Z')) {
    BLI_strncpy(passname, "Combined", passname_maxncpy);
  }
  else {
    BLI_strncpy(passname, channelname, passname_maxncpy);
  }
}

static void imb_exr_pass_name_from_channel_name(char *passname,
                                                const ExrChannel & /*echan*/,
                                                const char *channelname,
                                                const bool /*has_xyz_channels*/)
{
  const int passname_maxncpy = EXR_TOT_MAXNAME;

  /* TODO: Are special tricks similar to imb_exr_pass_name_from_channel() needed here?
   * Note that unknown passes are default to chan_id='X'. The place where this function is called
   * is when the channel name is more than 1 character, so perhaps using just channel ID is not
   * fully correct here. */

  BLI_strncpy(passname, channelname, passname_maxncpy);
}

static int imb_exr_split_channel_name(ExrChannel &echan,
                                      char *layname,
                                      char *passname,
                                      bool has_xyz_channels)
{
  const int layname_maxncpy = EXR_TOT_MAXNAME;
  const char *name = echan.name.c_str();
  const char *end = name + strlen(name);
  const char *token;

  /* Some multi-layers have the combined buffer with names V, RGBA, or XYZ saved. Additionally, the
   * Z channel can be interpreted as a Depth channel, but we only detect it as such if no X and Y
   * channels exists, since the Z in this case is part of XYZ. The same goes for the Y channel,
   * which can be detected as a luminance channel with the same name. */
  if (name[1] == 0) {
    /* Notice that we will be comparing with this upper-case version of the channel name, so the
     * below comparisons are effectively not case sensitive, and would also consider lowercase
     * versions of the listed channels. */
    echan.chan_id = BLI_toupper_ascii(name[0]);
    layname[0] = '\0';
    imb_exr_pass_name_from_channel(passname, echan, name, has_xyz_channels);
    return 1;
  }

  /* last token is channel identifier */
  size_t len = imb_exr_split_token(name, end, &token);
  if (len == 0) {
    CLOG_ERROR(&LOG, "Multilayer read: bad channel name: %s", name);
    return 0;
  }

  char channelname[EXR_TOT_MAXNAME];
  BLI_strncpy(channelname, token, std::min(len + 1, sizeof(channelname)));

  if (len == 1) {
    echan.chan_id = BLI_toupper_ascii(channelname[0]);
  }
  else {
    BLI_assert(len > 1); /* Checks above ensure. */
    if (len == 2) {
      /* Some multi-layers are using two-letter channels name,
       * like, MX or NZ, which is basically has structure of
       *   <pass_prefix><component>
       *
       * This is a bit silly, but see file from #35658.
       *
       * Here we do some magic to distinguish such cases.
       */
      const char chan_id = BLI_toupper_ascii(channelname[1]);
      if (ELEM(chan_id, 'X', 'Y', 'Z', 'R', 'G', 'B', 'U', 'V', 'A')) {
        echan.chan_id = chan_id;
      }
      else {
        echan.chan_id = 'X'; /* Default to X if unknown. */
      }
    }
    else if (BLI_strcaseeq(channelname, "red")) {
      echan.chan_id = 'R';
    }
    else if (BLI_strcaseeq(channelname, "green")) {
      echan.chan_id = 'G';
    }
    else if (BLI_strcaseeq(channelname, "blue")) {
      echan.chan_id = 'B';
    }
    else if (BLI_strcaseeq(channelname, "alpha")) {
      echan.chan_id = 'A';
    }
    else if (BLI_strcaseeq(channelname, "depth")) {
      echan.chan_id = 'Z';
    }
    else {
      echan.chan_id = 'X'; /* Default to X if unknown. */
    }
  }
  end -= len + 1; /* +1 to skip '.' separator */

  if (end > name) {
    /* second token is pass name */
    len = imb_exr_split_token(name, end, &token);
    if (len == 0) {
      CLOG_ERROR(&LOG, "Multilayer read: bad channel name: %s", name);
      return 0;
    }
    BLI_strncpy(passname, token, len + 1);
    end -= len + 1; /* +1 to skip '.' separator */
  }
  else {
    /* Single token, determine pass name from channel name. */
    imb_exr_pass_name_from_channel_name(passname, echan, channelname, has_xyz_channels);
  }

  /* all preceding tokens combined as layer name */
  if (end > name) {
    BLI_strncpy(layname, name, std::min(layname_maxncpy, int(end - name) + 1));
  }
  else {
    layname[0] = '\0';
  }

  return 1;
}

static ExrLayer *imb_exr_get_layer(ExrReadHandle *handle, const char *layname)
{
  for (ExrLayer &lay : handle->layers) {
    if (lay.name == layname) {
      return &lay;
    }
  }

  handle->layers.append_as();
  ExrLayer &lay = handle->layers.last();
  lay.name = layname;
  return &lay;
}

static ExrPass *imb_exr_get_pass(ExrLayer &lay, const char *passname)
{
  for (ExrPass &pass : lay.passes) {
    if (pass.name == passname) {
      return &pass;
    }
  }

  ExrPass pass;
  pass.name = passname;

  if (STREQ(passname, "Combined")) {
    lay.passes.prepend(std::move(pass));
    return &lay.passes.first();
  }

  lay.passes.append(std::move(pass));
  return &lay.passes.last();
}

static bool exr_has_xyz_channels(ExrReadHandle *exr_handle)
{
  bool x_found = false;
  bool y_found = false;
  bool z_found = false;
  for (const ExrChannel &echan : exr_handle->channels) {
    if (ELEM(echan.name, "X", "x")) {
      x_found = true;
    }
    if (ELEM(echan.name, "Y", "y")) {
      y_found = true;
    }
    if (ELEM(echan.name, "Z", "z")) {
      z_found = true;
    }
  }

  return x_found && y_found && z_found;
}

/**
 * Replacement for OpenEXR GetChannelsInMultiPartFile, that also handles the
 * case where parts are used for passes instead of multi-view.
 */
static Vector<ExrChannel> exr_channels_in_multi_part_file(const MultiPartInputFile &file)
{
  Vector<ExrChannel> channels;
  const ColorSpace *global_colorspace = imb_exr_part_colorspace(file.header(0));

  /* Get channels from each part. */
  for (int p = 0; p < file.parts(); p++) {
    const ChannelList &c = file.header(p).channels();

    /* Parse color-space. Per part color-spaces are not currently used, but
     * might as well populate them for consistency with writing. */
    const ColorSpace *colorspace = imb_exr_part_colorspace(file.header(p));
    if (colorspace == nullptr) {
      colorspace = global_colorspace;
    }

    /* There are two ways of storing multi-view EXRs:
     * - Multiple views in part with multiView attribute.
     * - Each view in its own part with view attribute. */
    const bool has_multiple_views_in_part = hasMultiView(file.header(p));
    StringVector views_in_part;
    if (has_multiple_views_in_part) {
      views_in_part = multiView(file.header(p));
    }
    StringRef part_view;
    if (file.header(p).hasView()) {
      part_view = file.header(p).view();
    }

    /* Parse part name. */
    StringRef part_name;
    if (file.header(p).hasName()) {
      part_name = file.header(p).name();

      /* Strip view name suffix if views are stored in separate parts.
       * They need to be included to make the part names unique. */
      if (!has_multiple_views_in_part) {
        if (part_name.endswith("." + part_view)) {
          part_name = part_name.drop_known_suffix("." + part_view);
        }
        else if (part_name.endswith("-" + part_view)) {
          part_name = part_name.drop_known_suffix("-" + part_view);
        }
      }
    }

    /* Parse channels. */
    for (ChannelList::ConstIterator i = c.begin(); i != c.end(); i++) {
      ExrChannel echan;
      echan.name = std::string(i.name());
      echan.internal_name = echan.name;

      if (has_multiple_views_in_part) {
        echan.view = viewFromChannelName(echan.name, views_in_part);
        echan.name = removeViewName(echan.internal_name, echan.view);
      }
      else {
        echan.view = part_view;
      }

      /* Prepend part name as potential layer or pass name. According to OpenEXR docs
       * this should not be needed, but Houdini writes files like this. */
      if (!part_name.is_empty() && !StringRef(echan.name).startswith(part_name + ".")) {
        echan.name = part_name + "." + echan.name;
      }

      echan.part_number = p;
      echan.colorspace = colorspace;
      channels.append(std::move(echan));
    }
  }

  return channels;
}

static bool imb_exr_multilayer_ensure_channels_parsed(ExrReadHandle *handle)
{
  if (handle->channels_parsed) {
    return !handle->layers.is_empty();
  }
  handle->channels_parsed = true;

  try {
    handle->views = imb_exr_get_views(*handle->ifile);
    handle->channels = exr_channels_in_multi_part_file(*handle->ifile);
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
    handle->views.clear();
    handle->channels.clear();
    return false;
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
    handle->views.clear();
    handle->channels.clear();
    return false;
  }

  const bool has_xyz_channels = exr_has_xyz_channels(handle);

  /* now try to sort out how to assign memory to the channels */
  /* first build hierarchical layer list */
  for (ExrChannel &echan : handle->channels) {
    char layname[EXR_TOT_MAXNAME], passname[EXR_TOT_MAXNAME];
    if (imb_exr_split_channel_name(echan, layname, passname, has_xyz_channels)) {
      const char *view = echan.view.c_str();
      std::string internal_name = passname;

      if (view[0] != '\0') {
        char tmp_pass[EXR_PASS_MAXNAME];
        SNPRINTF(tmp_pass, "%s.%s", passname, view);
        STRNCPY(passname, tmp_pass);
      }

      ExrLayer *lay = imb_exr_get_layer(handle, layname);
      ExrPass *pass = imb_exr_get_pass(*lay, passname);

      pass->chan[pass->totchan] = &echan;
      pass->totchan++;
      pass->view = view;
      pass->internal_name = internal_name;

      if (pass->totchan >= EXR_PASS_MAXCHAN) {
        CLOG_ERROR(&LOG, "Too many channels in one pass: %s", echan.name.c_str());
        return false;
      }
    }
  }

  /* With some heuristics, try to merge the channels into logical layers and passes. */
  for (ExrLayer &lay : handle->layers) {
    for (ExrPass &pass : lay.passes) {
      if (pass.totchan == 0) {
        continue;
      }
      if (pass.totchan == 1) {
        ExrChannel &echan = *pass.chan[0];
        echan.offset = 0;
        echan.xstride = 1;
        echan.ystride = handle->width;
        pass.chan_id[0] = echan.chan_id;
      }
      else {
        char lookup[256];

        memset(lookup, 0, sizeof(lookup));

        bool known_channel_layout = false;

        /* we can have RGB(A), XYZ(W), UVA */
        if (ELEM(pass.totchan, 3, 4)) {
          known_channel_layout = true;
          if (pass.chan[0]->chan_id == 'B' || pass.chan[1]->chan_id == 'B' ||
              pass.chan[2]->chan_id == 'B')
          {
            lookup[uint('R')] = 0;
            lookup[uint('G')] = 1;
            lookup[uint('B')] = 2;
            lookup[uint('A')] = 3;
          }
          else if (pass.chan[0]->chan_id == 'Y' || pass.chan[1]->chan_id == 'Y' ||
                   pass.chan[2]->chan_id == 'Y')
          {
            lookup[uint('X')] = 0;
            lookup[uint('Y')] = 1;
            lookup[uint('Z')] = 2;
            lookup[uint('W')] = 3;
          }
          else {
            lookup[uint('U')] = 0;
            lookup[uint('V')] = 1;
            lookup[uint('A')] = 2;
          }
        }

        for (int a = 0; a < pass.totchan; a++) {
          ExrChannel &echan = *pass.chan[a];
          const int offset = known_channel_layout ? lookup[uint(echan.chan_id)] : a;
          echan.offset = offset;
          echan.xstride = pass.totchan;
          echan.ystride = handle->width * pass.totchan;
          pass.chan_id[offset] = echan.chan_id;
        }
      }
    }
  }

  return true;
}

/* ********************************************************* */

static void exr_print_filecontents(MultiPartInputFile &file)
{
  int numparts = file.parts();
  if (numparts == 1 && hasMultiView(file.header(0))) {
    const StringVector views = multiView(file.header(0));
    CLOG_DEBUG(&LOG, "MultiView file");
    CLOG_DEBUG(&LOG, "Default view: %s", defaultViewName(views).c_str());
    for (const std::string &view : views) {
      CLOG_DEBUG(&LOG, "Found view %s", view.c_str());
    }
  }
  else if (numparts > 1) {
    CLOG_DEBUG(&LOG, "MultiPart file");
    for (int i = 0; i < numparts; i++) {
      if (file.header(i).hasView()) {
        CLOG_DEBUG(&LOG, "Part %d: view = \"%s\"", i, file.header(i).view().c_str());
      }
    }
  }

  for (int j = 0; j < numparts; j++) {
    const ChannelList &channels = file.header(j).channels();
    for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
      const Channel &channel = i.channel();
      CLOG_DEBUG(&LOG, "Found channel %s of type %d", i.name(), channel.type);
    }
  }
}

/** For non-multi-layer, map R G B A channel names to something that's in this file. */
static const char *exr_rgba_channelname(MultiPartInputFile &file, const char *chan)
{
  const ChannelList &channels = file.header(0).channels();

  for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
    // const Channel &channel = i.channel(); /* Not used yet. */
    const char *str = i.name();
    int len = strlen(str);
    if (len) {
      if (BLI_strcasecmp(chan, str + len - 1) == 0) {
        return str;
      }
    }
  }
  return chan;
}

static int exr_has_rgb(MultiPartInputFile &file, const char *rgb_channels[3])
{
  /* Common names for RGB-like channels in order. The V channel name is used by convention for BW
   * images, which will be broadcast to RGB channel at the end. */
  static const char *channel_names[] = {
      "V", "R", "Red", "G", "Green", "B", "Blue", "AR", "RA", "AG", "GA", "AB", "BA", nullptr};

  const Header &header = file.header(0);
  int num_channels = 0;

  for (int i = 0; channel_names[i]; i++) {
    /* Also try to match lower case variant of the channel names. */
    std::string lower_case_name = std::string(channel_names[i]);
    std::transform(lower_case_name.begin(),
                   lower_case_name.end(),
                   lower_case_name.begin(),
                   [](uchar c) { return std::tolower(c); });

    if (header.channels().findChannel(channel_names[i]) ||
        header.channels().findChannel(lower_case_name))
    {
      rgb_channels[num_channels++] = channel_names[i];
      if (num_channels == 3) {
        break;
      }
    }
  }

  return num_channels;
}

static bool exr_has_luma(MultiPartInputFile &file)
{
  /* Y channel is the luma and should always present fir luma space images,
   * optionally it could be also channels for chromas called BY and RY.
   */
  const Header &header = file.header(0);
  return header.channels().findChannel("Y") != nullptr;
}

static bool exr_has_chroma(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return header.channels().findChannel("BY") != nullptr &&
         header.channels().findChannel("RY") != nullptr;
}

static bool exr_has_alpha(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return !(header.channels().findChannel("A") == nullptr);
}

static bool exr_has_xyz(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return (header.channels().findChannel("X") != nullptr ||
          header.channels().findChannel("x") != nullptr) &&
         (header.channels().findChannel("Y") != nullptr ||
          header.channels().findChannel("y") != nullptr) &&
         (header.channels().findChannel("Z") != nullptr ||
          header.channels().findChannel("z") != nullptr);
}

static bool exr_has_channels(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return header.channels().begin() != header.channels().end();
}

static const char *exr_unknown_channel_name(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return header.channels().begin().name();
}

static bool exr_is_half_float(MultiPartInputFile &file)
{
  const ChannelList &channels = file.header(0).channels();
  for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
    const Channel &channel = i.channel();
    if (channel.type != HALF) {
      return false;
    }
  }
  return true;
}

static bool imb_exr_is_multilayer_file(MultiPartInputFile &file)
{
  const ChannelList &channels = file.header(0).channels();
  std::set<std::string> layerNames;

  /* This will not include empty layer names, so files with just R/G/B/A
   * channels without a layer name will be single layer. */
  channels.layers(layerNames);

  return !layerNames.empty();
}

static bool exr_has_multiview(MultiPartInputFile &file)
{
  for (int p = 0; p < file.parts(); p++) {
    if (hasMultiView(file.header(p))) {
      return true;
    }
  }

  return false;
}

static bool exr_has_multipart_file(MultiPartInputFile &file)
{
  return file.parts() > 1;
}

/** It returns true if the file is multi-layer or multi-view. */
static bool imb_exr_is_multi(MultiPartInputFile &file)
{
  try {
    /* Multipart files are treated as multilayer in blender -
     * even if they are single layer openexr with multiview. */
    if (exr_has_multipart_file(file)) {
      return true;
    }

    if (exr_has_multiview(file)) {
      return true;
    }

    if (imb_exr_is_multilayer_file(file)) {
      return true;
    }
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }

  return false;
}

static bool imb_check_chromaticity_val(float test_v, float ref_v)
{
  const float tolerance_v = 0.000001f;
  return (test_v < (ref_v + tolerance_v)) && (test_v > (ref_v - tolerance_v));
}

/* https://openexr.com/en/latest/TechnicalIntroduction.html#recommendations */
static bool imb_check_chromaticity_matches(const Imf::Chromaticities &a,
                                           const Imf::Chromaticities &b)
{
  return imb_check_chromaticity_val(a.red.x, b.red.x) &&
         imb_check_chromaticity_val(a.red.y, b.red.y) &&
         imb_check_chromaticity_val(a.green.x, b.green.x) &&
         imb_check_chromaticity_val(a.green.y, b.green.y) &&
         imb_check_chromaticity_val(a.blue.x, b.blue.x) &&
         imb_check_chromaticity_val(a.blue.y, b.blue.y) &&
         imb_check_chromaticity_val(a.white.x, b.white.x) &&
         imb_check_chromaticity_val(a.white.y, b.white.y);
}

static void imb_exr_set_known_colorspace(const Header &header, ImFileColorSpace &r_colorspace)
{
  r_colorspace.is_hdr_float = true;

  /* Read ACES container format metadata. */
  const IntAttribute *header_aces_container = header.findTypedAttribute<IntAttribute>(
      "acesImageContainerFlag");
  const ChromaticitiesAttribute *header_chromaticities =
      header.findTypedAttribute<ChromaticitiesAttribute>("chromaticities");

  if ((header_aces_container && header_aces_container->value() == 1) ||
      (header_chromaticities &&
       imb_check_chromaticity_matches(header_chromaticities->value(), CHROMATICITIES_ACES_2065_1)))
  {
    const char *known_colorspace = IMB_colormanagement_role_colorspace_name_get(
        COLOR_ROLE_ACES_INTERCHANGE);
    if (known_colorspace) {
      STRNCPY_UTF8(r_colorspace.metadata_colorspace, known_colorspace);
    }
    return;
  }

  const StringAttribute *header_interop_id = header.findTypedAttribute<StringAttribute>(
      "colorInteropID");

  /* Next try interop ID. */
  if (header_interop_id && !header_interop_id->value().empty()) {
    const ColorSpace *colorspace = IMB_colormanagement_space_from_interop_id(
        header_interop_id->value());
    if (colorspace) {
      STRNCPY_UTF8(r_colorspace.metadata_colorspace,
                   IMB_colormanagement_colorspace_get_name(colorspace));
      return;
    }
  }

  /* Try chromaticities. */
  if (header_chromaticities &&
      imb_check_chromaticity_matches(header_chromaticities->value(), CHROMATICITIES_XYZ_E))
  {
    /* Only works for the Blender default configuration due to fixed name. */
    STRNCPY_UTF8(r_colorspace.metadata_colorspace, "Linear CIE-XYZ E");
  }
}

static const ColorSpace *imb_exr_part_colorspace(const Header &header)
{
  ImFileColorSpace colorspace;
  imb_exr_set_known_colorspace(header, colorspace);
  return IMB_colormanagement_space_get_named(colorspace.metadata_colorspace);
}

static bool exr_get_ppm(MultiPartInputFile &file, double ppm[2])
{
  try {
    const Header &header = file.header(0);
    if (!hasXDensity(header)) {
      return false;
    }
    ppm[0] = double(xDensity(header)) / 0.0254;
    ppm[1] = ppm[0] * double(header.pixelAspectRatio());
    return true;
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
    return false;
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
    return false;
  }
}

bool IMB_exr_get_ppm(ExrReadHandle *handle, double ppm[2])
{
  return exr_get_ppm(*handle->ifile, ppm);
}

static void get_exr_display_window(const MultiPartInputFile &file,
                                   int display_size[2],
                                   int display_offset[2],
                                   int data_offset[2])
{
  try {
    const Header &header = file.header(0);
    const Box2i data_window = header.dataWindow();
    const Box2i display_window = header.displayWindow();
    display_size[0] = display_window.size()[0] + 1;
    display_size[1] = display_window.size()[1] + 1;
    display_offset[0] = display_window.min[0];
    display_offset[1] = display_window.min[1];
    data_offset[0] = data_window.min[0] - display_window.min[0];
    data_offset[1] = data_window.min[1] - display_window.min[1];
    return;
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }

  display_size[0] = display_size[1] = 0;
  display_offset[0] = display_offset[1] = 0;
  data_offset[0] = data_offset[1] = 0;
}

void IMB_exr_get_display_window(ExrReadHandle *handle,
                                int display_size[2],
                                int display_offset[2],
                                int data_offset[2])
{
  get_exr_display_window(*handle->ifile, display_size, display_offset, data_offset);
}

static void imb_exr_set_ibuf_display_window(MultiPartInputFile &file, ImBuf *ibuf)
{
  ibuf->flags |= ImBufFlags::HasDisplayWindow;
  get_exr_display_window(file, ibuf->display_size, ibuf->display_offset, ibuf->data_offset);
  exr_get_ppm(file, ibuf->ppm);
}

ExrReadHandle *IMB_exr_open_multilayer(const char *filepath)
{
  ExrReadHandle *handle = IMB_exr_open(filepath);
  if (handle == nullptr) {
    return nullptr;
  }
  if (!imb_exr_is_multi(*handle->ifile)) {
    IMB_exr_close(handle);
    return nullptr;
  }
  return handle;
}

ExrReadHandle *IMB_exr_open_multilayer_from_memory(const uchar *mem, const size_t size)
{
  IMemStream *membuf = nullptr;
  MultiPartInputFile *file = nullptr;

  try {
    membuf = new IMemStream(const_cast<uchar *>(mem), size);
    file = new MultiPartInputFile(*membuf);

    if (!imb_exr_is_multi(*file)) {
      delete file;
      delete membuf;
      return nullptr;
    }

    const Box2i dw = file->header(0).dataWindow();

    ExrReadHandle *handle = MEM_new<ExrReadHandle>("ExrReadHandle");
    handle->ifile_stream = membuf;
    handle->ifile = file;
    handle->width = dw.max.x - dw.min.x + 1;
    handle->height = dw.max.y - dw.min.y + 1;
    return handle;
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }

  delete file;
  delete membuf;
  return nullptr;
}

ImBuf *imb_load_openexr(const uchar *mem,
                        size_t size,
                        ImBufFlags flags,
                        ImFileColorSpace &r_colorspace)
{
  ImBuf *ibuf = nullptr;
  IMemStream *membuf = nullptr;
  MultiPartInputFile *file = nullptr;

  if (imb_is_a_openexr(mem, size) == 0) {
    return nullptr;
  }

  try {
    membuf = new IMemStream(const_cast<uchar *>(mem), size);
    file = new MultiPartInputFile(*membuf);

    const Header &file_header = file->header(0);
    Box2i dw = file_header.dataWindow();
    const size_t width = dw.max.x - dw.min.x + 1;
    const size_t height = dw.max.y - dw.min.y + 1;

    CLOG_DEBUG(&LOG, "Image data window %d %d %d %d", dw.min.x, dw.min.y, dw.max.x, dw.max.y);

    if (CLOG_CHECK(&LOG, CLG_LEVEL_DEBUG)) {
      exr_print_filecontents(*file);
    }

    {
      const bool is_alpha = exr_has_alpha(*file);

      ibuf = IMB_allocImBuf(width, height, ImBufFlags::Zero);
      ibuf->color_mode = is_alpha ? ImColorMode::RGBA : ImColorMode::RGB;
      ibuf->foptions.flag |= exr_is_half_float(*file) ? OPENEXR_HALF : 0;
      ibuf->foptions.flag |= openexr_header_get_compression(file_header);

      imb_exr_set_ibuf_display_window(*file, ibuf);

      imb_exr_set_known_colorspace(file_header, r_colorspace);

      ibuf->ftype = IMB_FTYPE_OPENEXR;

      /* Tell caller they have to read multilayer themselves, if they specified they
       * are capable of handling multilayer. */
      const bool is_multi = imb_exr_is_multi(*file);
      const bool defer_multilayer = is_multi && flag_is_set(flags, ImBufFlags::MultiLayer);
      if (defer_multilayer) {
        ibuf->flags |= ImBufFlags::MultiLayer;
      }

      if (!flag_is_set(flags, ImBufFlags::Test)) {

        if (flag_is_set(flags, ImBufFlags::Metadata)) {
          Header::ConstIterator iter;

          IDProperty *metadata = ibuf->metadata_for_write();
          for (iter = file_header.begin(); iter != file_header.end(); iter++) {
            const StringAttribute *attr = file_header.findTypedAttribute<StringAttribute>(
                iter.name());

            /* not all attributes are string attributes so we might get some NULLs here */
            if (attr) {
              IMB_metadata_set_field(metadata, iter.name(), attr->value().c_str());
              ibuf->flags |= ImBufFlags::Metadata;
            }
          }
        }

        if (!defer_multilayer && is_multi) {
          /* Multilayer but caller can not handle it, read a single pass. */
          ExrReadHandle *handle = MEM_new<ExrReadHandle>("ExrReadHandle");
          handle->ifile = file;
          handle->ifile_stream = membuf;
          handle->width = int(width);
          handle->height = int(height);
          const bool ok = imb_exr_multi_read_single_pass(handle, ibuf);
          IMB_exr_close(handle);
          file = nullptr;
          membuf = nullptr;
          if (!ok) {
            IMB_freeImBuf(ibuf);
            return nullptr;
          }
        }
        else if (!defer_multilayer) {
          /* Read single layer EXR. */
          const char *rgb_channels[3];
          const int num_rgb_channels = exr_has_rgb(*file, rgb_channels);
          const int has_channels = exr_has_channels(*file);
          const bool has_luma = exr_has_luma(*file);
          const bool has_xyz = exr_has_xyz(*file);
          FrameBuffer frameBuffer;
          float *first;
          size_t xstride = sizeof(float[4]);
          size_t ystride = -xstride * width;

          /* No need to clear image memory, it will be fully written below. */
          IMB_alloc_float_pixels(ibuf, 4, false);

          /* Inverse correct first pixel for data-window
           * coordinates (- dw.min.y because of y flip). */
          first = ibuf->float_data_for_write() - 4 * (dw.min.x - dw.min.y * width);
          /* But, since we read y-flipped (negative y stride) we move to last scan-line. */
          first += 4 * (height - 1) * width;

          if (num_rgb_channels > 0) {
            for (int i = 0; i < num_rgb_channels; i++) {
              frameBuffer.insert(exr_rgba_channelname(*file, rgb_channels[i]),
                                 Slice(Imf::FLOAT, (char *)(first + i), xstride, ystride));
            }
          }
          else if (has_xyz) {
            frameBuffer.insert(exr_rgba_channelname(*file, "X"),
                               Slice(Imf::FLOAT, (char *)first, xstride, ystride));
            frameBuffer.insert(exr_rgba_channelname(*file, "Y"),
                               Slice(Imf::FLOAT, (char *)(first + 1), xstride, ystride));
            frameBuffer.insert(exr_rgba_channelname(*file, "Z"),
                               Slice(Imf::FLOAT, (char *)(first + 2), xstride, ystride));
          }
          else if (has_luma) {
            frameBuffer.insert(exr_rgba_channelname(*file, "Y"),
                               Slice(Imf::FLOAT, (char *)first, xstride, ystride));
            frameBuffer.insert(
                exr_rgba_channelname(*file, "BY"),
                Slice(Imf::FLOAT, (char *)(first + 1), xstride, ystride, 1, 1, 0.5f));
            frameBuffer.insert(
                exr_rgba_channelname(*file, "RY"),
                Slice(Imf::FLOAT, (char *)(first + 2), xstride, ystride, 1, 1, 0.5f));
          }
          else if (has_channels) {
            frameBuffer.insert(exr_unknown_channel_name(*file),
                               Slice(Imf::FLOAT, (char *)first, xstride, ystride, 1, 1));
          }

          /* 1.0 is fill value, this still needs to be assigned even when (is_alpha == 0) */
          frameBuffer.insert(exr_rgba_channelname(*file, "A"),
                             Slice(Imf::FLOAT, (char *)(first + 3), xstride, ystride, 1, 1, 1.0f));

          InputPart in(*file, 0);
          in.setFrameBuffer(frameBuffer);
          in.readPixels(dw.min.y, dw.max.y);

          /* XXX, ImBuf has no nice way to deal with this.
           * ideally IM_rect would be used when the caller wants a rect BUT
           * at the moment all functions use IM_rect.
           * Disabling this is ok because all functions should check
           * if a rect exists and create one on demand.
           *
           * Disabling this because the sequencer frees immediate. */
#if 0
          if (flag & IM_rect) {
            IMB_byte_from_float(ibuf);
          }
#endif

          float *float_data = ibuf->float_data_for_write();
          if (num_rgb_channels == 0 && has_luma && exr_has_chroma(*file)) {
            for (size_t a = 0; a < size_t(ibuf->x) * ibuf->y; a++) {
              float *color = float_data + a * 4;
              ycc_to_rgb(color[0] * 255.0f,
                         color[1] * 255.0f,
                         color[2] * 255.0f,
                         &color[0],
                         &color[1],
                         &color[2],
                         BLI_YCC_ITU_BT709);
            }
          }
          else if (!has_xyz && num_rgb_channels <= 1) {
            /* Convert 1 to 3 channels. */
            for (size_t a = 0; a < size_t(ibuf->x) * ibuf->y; a++) {
              float *color = float_data + a * 4;
              color[1] = color[0];
              color[2] = color[0];
            }
          }
        }
      }

      /* file is no longer needed */
      delete membuf;
      delete file;

      if (flag_is_set(flags, ImBufFlags::AlphaDetect)) {
        ibuf->flags |= ImBufFlags::AlphaPremul;
      }
    }
    return ibuf;
  }
  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }

  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }
  delete file;
  delete membuf;

  return nullptr;
}

ImBuf *imb_load_filepath_thumbnail_openexr(const char *filepath,
                                           const ImBufFlags /*flags*/,
                                           const size_t max_thumb_size,
                                           ImFileColorSpace &r_colorspace,
                                           size_t *r_width,
                                           size_t *r_height)
{
  ImBuf *ibuf = nullptr;
  IStream *stream = nullptr;
  Imf::RgbaInputFile *file = nullptr;

  /* OpenExr uses exceptions for error-handling. */
  try {

    /* The memory-mapped stream is faster, but don't use for huge files as it requires contiguous
     * address space and we are processing multiple files at once (typically one per processor
     * core). The 100 MB limit here is arbitrary, but seems reasonable and conservative. */
    if (BLI_file_size(filepath) < 100 * 1024 * 1024) {
      stream = new IMMapStream(filepath);
    }
    else {
      stream = new IFileStream(filepath);
    }

    /* imb_initopenexr() creates a global pool of worker threads. But we thumbnail multiple images
     * at once, and by default each file will attempt to use the entire pool for itself, stalling
     * the others. So each thumbnail should use a single thread of the pool. */
    file = new RgbaInputFile(*stream, 1);

    if (!file->isComplete()) {
      delete file;
      delete stream;
      return nullptr;
    }

    Imath::Box2i dw = file->dataWindow();
    int source_w = dw.max.x - dw.min.x + 1;
    int source_h = dw.max.y - dw.min.y + 1;
    *r_width = source_w;
    *r_height = source_h;

    const Header &file_header = file->header();

    /* If there is an embedded thumbnail, return that instead of making a new one. */
    if (file_header.hasPreviewImage()) {
      const Imf::PreviewImage &preview = file->header().previewImage();
      ImBuf *ibuf = IMB_allocFromBuffer(
          (uint8_t *)preview.pixels(), nullptr, preview.width(), preview.height(), 4);
      delete file;
      delete stream;
      IMB_flipy(ibuf);
      return ibuf;
    }

    /* No effect yet for thumbnails, but will work once it is supported. */
    imb_exr_set_known_colorspace(file_header, r_colorspace);

    /* Create a new thumbnail. */
    float scale_factor = std::min(float(max_thumb_size) / float(source_w),
                                  float(max_thumb_size) / float(source_h));
    int dest_w = std::max(int(source_w * scale_factor), 1);
    int dest_h = std::max(int(source_h * scale_factor), 1);

    ibuf = IMB_allocImBuf(dest_w, dest_h, ImBufFlags::FloatData);

    /* A single row of source pixels. */
    Imf::Array<Imf::Rgba> pixels(source_w);

    /* Loop through destination thumbnail rows. */
    float *float_data = ibuf->float_data_for_write();
    for (int h = 0; h < dest_h; h++) {

      /* Load the single source row that corresponds with destination row. */
      int source_y = int(float(h) / scale_factor) + dw.min.y;
      file->setFrameBuffer(&pixels[0] - dw.min.x - source_y * source_w, 1, source_w);
      file->readPixels(source_y);

      for (int w = 0; w < dest_w; w++) {
        /* For each destination pixel find single corresponding source pixel. */
        int source_x = int(std::min<int>((w / scale_factor), dw.max.x - 1));
        float *dest_px = &float_data[(h * dest_w + w) * 4];
        dest_px[0] = pixels[source_x].r;
        dest_px[1] = pixels[source_x].g;
        dest_px[2] = pixels[source_x].b;
        dest_px[3] = pixels[source_x].a;
      }
    }

    if (file->lineOrder() == INCREASING_Y) {
      IMB_flipy(ibuf);
    }

    delete file;
    delete stream;

    return ibuf;
  }

  catch (const std::exception &exc) {
    CLOG_ERROR(&LOG, "%s: %s", __func__, exc.what());
  }
  catch (...) { /* Catch-all for RTTI or symbol visibility mismatches. */
    CLOG_ERROR(&LOG, "Unknown error in %s", __func__);
  }

  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }

  delete file;
  delete stream;
  return nullptr;
}

void imb_initopenexr()
{
  /* In a multithreaded program, staticInitialize() must be called once during startup, before the
   * program accesses any other functions or classes in the IlmImf library. */
  Imf::staticInitialize();
  Imf::setGlobalThreadCount(BLI_system_thread_count());
}

void imb_exitopenexr()
{
  /* Tells OpenEXR to free thread pool, also ensures there is no running tasks. */
  Imf::setGlobalThreadCount(0);
}

}  // namespace blender
