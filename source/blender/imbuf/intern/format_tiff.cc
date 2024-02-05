/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "oiio/openimageio_support.hh"

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_tiff(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "tif");
}

ImBuf *imb_load_tiff(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{mem, size, "tif", IMB_FTYPE_TIF, flags};

  /* All TIFFs should be in default byte colorspace. */
  ctx.use_colorspace_role = COLOR_ROLE_DEFAULT_BYTE;

  ImBuf *ibuf = imb_oiio_read(ctx, config, colorspace, spec);
  if (ibuf) {
    if (flags & IB_alphamode_detect) {
      if (spec.nchannels == 4 && spec.format == TypeDesc::UINT16) {
        ibuf->flags |= IB_alphamode_premul;
      }
    }
  }

  return ibuf;
}

bool imb_save_tiff(ImBuf *ibuf, const char *filepath, int flags)
{
  const bool is_16bit = ((ibuf->foptions.flag & TIF_16BIT) && ibuf->float_buffer.data);
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = is_16bit ? TypeDesc::UINT16 : TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("tif", ibuf, flags, is_16bit);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  if (is_16bit && file_channels == 4) {
    file_spec.attribute("oiio:UnassociatedAlpha", 0);
  }
  else {
    file_spec.attribute("oiio:UnassociatedAlpha", 1);
  }

  if (ibuf->foptions.flag & TIF_COMPRESS_DEFLATE) {
    file_spec.attribute("compression", "zip");
  }
  else if (ibuf->foptions.flag & TIF_COMPRESS_LZW) {
    file_spec.attribute("compression", "lzw");
  }
  else if (ibuf->foptions.flag & TIF_COMPRESS_PACKBITS) {
    file_spec.attribute("compression", "packbits");
  }
  else if (ibuf->foptions.flag & TIF_COMPRESS_NONE) {
    file_spec.attribute("compression", "none");
  }

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
