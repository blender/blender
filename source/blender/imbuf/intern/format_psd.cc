/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.h"

#include "IMB_imbuf_types.h"

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

extern "C" {

bool imb_is_a_psd(const uchar *mem, size_t size)
{
  const uchar magic[4] = {'8', 'B', 'P', 'S'};
  if (size < sizeof(magic)) {
    return false;
  }
  return memcmp(magic, mem, sizeof(magic)) == 0;
}

ImBuf *imb_load_psd(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;
  config.attribute("oiio:UnassociatedAlpha", 1);

  ReadContext ctx{mem, size, "psd", IMB_FTYPE_PSD, flags};

  /* PSD should obey color space information embedded in the file. */
  ctx.use_embedded_colorspace = true;

  return imb_oiio_read(ctx, config, colorspace, spec);
}
}
