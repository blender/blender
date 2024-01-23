/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_tga(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "tga");
}

ImBuf *imb_load_tga(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{mem, size, "tga", IMB_FTYPE_TGA, flags};
  return imb_oiio_read(ctx, config, colorspace, spec);
}

bool imb_save_tga(ImBuf *ibuf, const char *filepath, int flags)
{
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("tga", ibuf, flags, false);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);
  file_spec.attribute("oiio:UnassociatedAlpha", 1);
  file_spec.attribute("compression", (ibuf->foptions.flag & RAWTGA) ? "none" : "rle");

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
