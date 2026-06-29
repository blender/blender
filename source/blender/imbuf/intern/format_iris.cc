/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * The SGI Image File Format.
 * https://en.wikipedia.org/wiki/Silicon_Graphics_Image
 *
 * \note this format uses big-endian values.
 */

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"

namespace blender {
OIIO_NAMESPACE_USING
using namespace blender::imbuf;

const char *imb_file_extensions_iris[] = {".sgi", ".rgb", ".rgba", ".bw", nullptr};

bool imb_is_a_iris(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "rgb");
}

ImBuf *imb_loadiris(const uchar *mem,
                    size_t size,
                    ImBufFlags flags,
                    ImFileColorSpace &r_colorspace)
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{mem, size, "rgb", IMB_FTYPE_IRIS, flags};

  ImBuf *ibuf = imb_oiio_read(ctx, config, r_colorspace, spec);

  /* Both 8 and 16 bit iris should be in default byte colorspace. */
  r_colorspace.is_hdr_float = false;

  return ibuf;
}

static std::tuple<WriteContext, ImageSpec> prepare_save_iris(ImBuf *ibuf, ImBufFlags flags)
{
  const int file_channels = ibuf->color_mode_channels_get();
  const TypeDesc data_format = TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("rgb", ibuf, flags, false);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  /* Always use RLE compression to match historical behavior. */
  file_spec.attribute("compression", "rle");

  return {ctx, file_spec};
}

bool imb_saveiris(ImBuf *ibuf, const char *filepath, ImBufFlags flags)
{
  const auto [ctx, file_spec] = prepare_save_iris(ibuf, flags);
  return imb_oiio_write(ctx, filepath, file_spec);
}

Vector<uint8_t> imb_save_buffer_iris(ImBuf *ibuf, ImBufFlags flags)
{
  const auto [ctx, file_spec] = prepare_save_iris(ibuf, flags);
  return imb_oiio_write_buffer(ctx, file_spec);
}

}  // namespace blender
