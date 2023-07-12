/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "oiio/openimageio_support.hh"

#include "IMB_colormanagement.h"
#include "IMB_filetype.h"
#include "IMB_imbuf_types.h"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_tiff(const uchar *mem, size_t size)
{
  constexpr int MAGIC_SIZE = 4;
  if (size < MAGIC_SIZE) {
    return false;
  }

  const char big_endian[MAGIC_SIZE] = {0x4d, 0x4d, 0x00, 0x2a};
  const char lil_endian[MAGIC_SIZE] = {0x49, 0x49, 0x2a, 0x00};
  return ((memcmp(big_endian, mem, MAGIC_SIZE) == 0) ||
          (memcmp(lil_endian, mem, MAGIC_SIZE) == 0));
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

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
