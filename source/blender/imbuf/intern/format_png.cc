/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "oiio/openimageio_support.hh"

#include "IMB_colormanagement.h"
#include "IMB_filetype.h"
#include "IMB_imbuf_types.h"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_png(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "png");
}

ImBuf *imb_load_png(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{mem, size, "png", IMB_FTYPE_PNG, flags};

  /* Both 8 and 16 bit PNGs should be in default byte colorspace. */
  ctx.use_colorspace_role = COLOR_ROLE_DEFAULT_BYTE;

  ImBuf *ibuf = imb_oiio_read(ctx, config, colorspace, spec);
  if (ibuf) {
    if (spec.format == TypeDesc::UINT16) {
      ibuf->flags |= PNG_16BIT;
    }
  }

  return ibuf;
}

bool imb_save_png(struct ImBuf *ibuf, const char *filepath, int flags)
{
  const bool is_16bit = (ibuf->foptions.flag & PNG_16BIT);
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = is_16bit ? TypeDesc::UINT16 : TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("png", ibuf, flags, is_16bit);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  /* Skip if the float buffer was managed already. */
  if (is_16bit && (ibuf->float_colorspace || (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA))) {
    file_spec.attribute("oiio:UnassociatedAlpha", 0);
  }
  else {
    file_spec.attribute("oiio:UnassociatedAlpha", 1);
  }

  int compression = int(float(ibuf->foptions.quality) / 11.1111f);
  compression = compression < 0 ? 0 : (compression > 9 ? 9 : compression);
  file_spec.attribute("png:compressionLevel", compression);

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
