/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.h"
#include "IMB_imbuf_types.h"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_hdr(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "hdr");
}

ImBuf *imb_load_hdr(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;

  ReadContext ctx{mem, size, "hdr", IMB_FTYPE_RADHDR, flags};

  /* Always create ImBufs with a 4th alpha channel despite the format only supporting 3. */
  ctx.use_all_planes = true;

  ImBuf *ibuf = imb_oiio_read(ctx, config, colorspace, spec);
  if (ibuf) {
    if (flags & IB_alphamode_detect) {
      ibuf->flags |= IB_alphamode_premul;
    }
    if (flags & IB_rect) {
      IMB_rect_from_float(ibuf);
    }
  }

  return ibuf;
}

bool imb_save_hdr(struct ImBuf *ibuf, const char *filepath, int flags)
{
  const int file_channels = 3;
  const TypeDesc data_format = TypeDesc::FLOAT;

  WriteContext ctx = imb_create_write_context("hdr", ibuf, flags);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
