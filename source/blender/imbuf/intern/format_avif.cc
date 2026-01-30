/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <fmt/format.h>

#include "oiio/openimageio_support.hh"

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"

namespace blender {

OIIO_NAMESPACE_USING
using namespace imbuf;

bool imb_is_a_avif(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "heif");
}

ImBuf *imb_load_avif(const uchar *mem, size_t size, int flags, ImFileColorSpace &r_colorspace)
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{.mem_start = mem,
                  .mem_size = size,
                  .file_format = "heif",
                  .file_type = IMB_FTYPE_AVIF,
                  .flags = flags};

  ImBuf *ibuf = imb_oiio_read(ctx, config, r_colorspace, spec);
  if (ibuf) {
    const int bits_per_sample = spec.get_int_attribute("oiio:BitsPerSample", 8);
    if (bits_per_sample == 10) {
      ibuf->foptions.flag |= AVIF_10BIT;
    }
    else if (bits_per_sample == 12) {
      ibuf->foptions.flag |= AVIF_12BIT;
    }
  }

  /* Assume SDR by default, CICP will indicate if it's HDR and set a colorspace. */
  r_colorspace.is_hdr_float = false;

  return ibuf;
}

bool imb_save_avif(ImBuf *ibuf, const char *filepath, int flags)
{
  const int bits_per_sample = (ibuf->foptions.flag & AVIF_10BIT) ? 10 :
                              (ibuf->foptions.flag & AVIF_12BIT) ? 12 :
                                                                   8;
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = bits_per_sample > 8 ? TypeDesc::UINT16 : TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("heif", ibuf, flags, bits_per_sample > 8);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  /* Skip if the float buffer was managed already. */
  if (bits_per_sample > 8 &&
      (ibuf->float_buffer.colorspace || (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA)))
  {
    file_spec.attribute("oiio:UnassociatedAlpha", 0);
  }
  else {
    file_spec.attribute("oiio:UnassociatedAlpha", 1);
  }

  file_spec.attribute("Compression", fmt::format("avif:{}", int(ibuf->foptions.quality)));
  file_spec.attribute("oiio:BitsPerSample", bits_per_sample);
  file_spec.attribute("oiio:UnassociatedAlpha", 1);

  return imb_oiio_write(ctx, filepath, file_spec);
}

};  // namespace blender
