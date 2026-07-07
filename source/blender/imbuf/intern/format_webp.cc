/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include <fcntl.h>
#include <string>
#include <webp/decode.h>

#include "oiio/openimageio_support.hh"

#include "BLI_fileops.h"
#include "BLI_mmap.h"

#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"image.webp"};

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

bool imb_is_a_webp(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "webp");
}

ImBuf *imb_loadwebp(const uchar *mem, size_t size, int flags, ImFileColorSpace &r_colorspace)
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{mem, size, "webp", IMB_FTYPE_WEBP, flags};
  ImBuf *ibuf = imb_oiio_read(ctx, config, r_colorspace, spec);

  r_colorspace.is_hdr_float = false;
  return ibuf;
}

ImBuf *imb_load_filepath_thumbnail_webp(const char *filepath,
                                        const int /*flags*/,
                                        const size_t max_thumb_size,
                                        ImFileColorSpace & /*r_colorspace*/,
                                        size_t *r_width,
                                        size_t *r_height)
{
  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }

  BLI_mmap_file *mmap_file = BLI_mmap_open(file);
  close(file);
  if (mmap_file == nullptr) {
    return nullptr;
  }

  const uchar *data = static_cast<const uchar *>(BLI_mmap_get_pointer(mmap_file));
  const size_t data_size = BLI_mmap_get_length(mmap_file);

  WebPDecoderConfig config;
  if (!data || !WebPInitDecoderConfig(&config) ||
      WebPGetFeatures(data, data_size, &config.input) != VP8_STATUS_OK ||
      BLI_mmap_any_io_error(mmap_file))
  {
    CLOG_ERROR(&LOG, "Invalid file");
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  /* Return full size of the image. */
  *r_width = size_t(config.input.width);
  *r_height = size_t(config.input.height);

  const float scale = float(max_thumb_size) / std::max(config.input.width, config.input.height);
  const int dest_w = std::max(int(config.input.width * scale), 1);
  const int dest_h = std::max(int(config.input.height * scale), 1);

  ImBuf *ibuf = IMB_allocImBuf(dest_w, dest_h, 32, IB_byte_data);
  if (ibuf == nullptr) {
    CLOG_ERROR(&LOG, "Failed to allocate image memory");
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  config.options.no_fancy_upsampling = 1;
  config.options.use_scaling = 1;
  config.options.scaled_width = dest_w;
  config.options.scaled_height = dest_h;
  config.options.bypass_filtering = 1;
  config.options.use_threads = 0;
  config.options.flip = 1;
  config.output.is_external_memory = 1;
  config.output.colorspace = MODE_RGBA;
  config.output.u.RGBA.rgba = ibuf->byte_buffer.data;
  config.output.u.RGBA.stride = 4 * ibuf->x;
  config.output.u.RGBA.size = size_t(config.output.u.RGBA.stride) * size_t(ibuf->y);

  if (WebPDecode(data, data_size, &config) != VP8_STATUS_OK || BLI_mmap_any_io_error(mmap_file)) {
    CLOG_ERROR(&LOG, "Failed to decode image");
    IMB_freeImBuf(ibuf);
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  /* Free the output buffer. */
  WebPFreeDecBuffer(&config.output);

  BLI_mmap_free(mmap_file);

  return ibuf;
}

bool imb_savewebp(ImBuf *ibuf, const char *filepath, int flags)
{
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("webp", ibuf, flags, false);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  file_spec.attribute("oiio:UnassociatedAlpha", 1);

  /* A general quality/speed trade-off (0=fast, 6=slower-better). 4 matches historical value. */
  file_spec.attribute("webp:method", 4);

  if (ibuf->foptions.quality == 100.0f) {
    /* Lossless compression. */
    /* Use 70 to match historical value (see libwebp's LOSSLESS_DEFAULT_QUALITY). */
    file_spec.attribute("compression", "lossless:70");
  }
  else {
    /* Lossy compression. */
    file_spec.attribute("compression",
                        std::string("webp:") + std::to_string(ibuf->foptions.quality));
  }

  return imb_oiio_write(ctx, filepath, file_spec);
}

}  // namespace blender
