/* SPDX-FileCopyrightText: 2023 Blender Authors
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
bool imb_is_a_dpx(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "dpx");
}

ImBuf *imb_load_dpx(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;

  ReadContext ctx{mem, size, "dpx", IMB_FTYPE_DPX, flags};

  ctx.use_colorspace_role = COLOR_ROLE_DEFAULT_FLOAT;

  ImBuf *ibuf = imb_oiio_read(ctx, config, colorspace, spec);
  if (ibuf) {
    if (flags & IB_alphamode_detect) {
      ibuf->flags |= IB_alphamode_premul;
    }
  }

  return ibuf;
}

bool imb_save_dpx(ImBuf *ibuf, const char *filepath, int flags)
{
  int bits_per_sample = 8;
  if (ibuf->foptions.flag & CINEON_10BIT) {
    bits_per_sample = 10;
  }
  else if (ibuf->foptions.flag & CINEON_12BIT) {
    bits_per_sample = 12;
  }
  else if (ibuf->foptions.flag & CINEON_16BIT) {
    bits_per_sample = 16;
  }

  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = bits_per_sample == 8 ? TypeDesc::UINT8 : TypeDesc::UINT16;

  WriteContext ctx = imb_create_write_context("dpx", ibuf, flags);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  const float max_value = powf(2, bits_per_sample) - 1.0f;
  file_spec.attribute("oiio:BitsPerSample", bits_per_sample);
  file_spec.attribute("dpx:WhiteLevel", 685.0f / 1023.0f * max_value);
  file_spec.attribute("dpx:BlackLevel", 95.0f / 1023.0f * max_value);
  file_spec.attribute("dpx:HighData", max_value);
  file_spec.attribute("dpx:LowData", 0);
  file_spec.attribute("dpx:LowQuantity", 0.0f);

  if (ibuf->foptions.flag & CINEON_LOG) {
    /* VERIFY: This matches previous code but seems odd. Needs a comment if confirmed. */
    file_spec.attribute("dpx:Transfer", "Printing density");
    file_spec.attribute("dpx:HighQuantity", 2.048f);
  }
  else {
    file_spec.attribute("dpx:Transfer", "Linear");
    file_spec.attribute("dpx:HighQuantity", max_value);
  }

  if (ELEM(bits_per_sample, 8, 16)) {
    file_spec.attribute("dpx:Packing", "Packed");
  }
  else {
    file_spec.attribute("dpx:Packing", "Filled, method A");
  }

  return imb_oiio_write(ctx, filepath, file_spec);
}
}
