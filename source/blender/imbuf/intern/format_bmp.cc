/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.h"
#include "IMB_imbuf_types.h"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_bmp(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "bmp");
}

ImBuf *imb_load_bmp(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;

  /* Keep historical behavior - do not use a 1-channel format for a black-white image. */
  config.attribute("bmp:monochrome_detect", 0);

  ReadContext ctx{mem, size, "bmp", IMB_FTYPE_BMP, flags};
  return imb_oiio_read(ctx, config, colorspace, spec);
}

bool imb_save_bmp(struct ImBuf *ibuf, const char *filepath, int flags)
{
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("bmp", ibuf, flags, false);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
