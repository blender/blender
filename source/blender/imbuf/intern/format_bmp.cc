/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"

namespace blender {

const char *imb_file_extensions_bmp[] = {".bmp", ".dib", nullptr};

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

bool imb_is_a_bmp(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "bmp");
}

ImBuf *imb_load_bmp(const uchar *mem,
                    size_t size,
                    ImBufFlags flags,
                    ImFileColorSpace &r_colorspace)
{
  ImageSpec config, spec;

  /* Keep historical behavior - do not use a 1-channel format for a black-white image. */
  config.attribute("bmp:monochrome_detect", 0);

  ReadContext ctx{mem, size, "bmp", IMB_FTYPE_BMP, flags};
  return imb_oiio_read(ctx, config, r_colorspace, spec);
}

static std::tuple<WriteContext, ImageSpec> prepare_save_bmp(ImBuf *ibuf, ImBufFlags flags)
{
  int file_channels = ibuf->color_mode_channels_get();
  /* BMP does not support 2-channel (gray + alpha) writes; promote to RGBA. */
  if (file_channels == 2) {
    file_channels = 4;
  }
  const TypeDesc data_format = TypeDesc::UINT8;
  WriteContext ctx = imb_create_write_context("bmp", ibuf, flags, false);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);
  return {ctx, file_spec};
}

bool imb_save_bmp(ImBuf *ibuf, const char *filepath, ImBufFlags flags)
{
  const auto [ctx, file_spec] = prepare_save_bmp(ibuf, flags);
  return imb_oiio_write(ctx, filepath, file_spec);
}

Vector<uint8_t> imb_save_buffer_bmp(ImBuf *ibuf, ImBufFlags flags)
{
  const auto [ctx, file_spec] = prepare_save_bmp(ibuf, flags);
  return imb_oiio_write_buffer(ctx, file_spec);
}

}  // namespace blender
