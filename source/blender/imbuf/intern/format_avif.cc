/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstring>

#include <fmt/format.h>

#include "oiio/openimageio_support.hh"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

namespace blender {

const char *imb_file_extensions_avif[] = {".avif", nullptr};

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

/** Block size (in pixels) that the AV1 encoder processes at a time. */
static constexpr int AVIF_BLOCK_SIZE = 64;

/**
 * Work around a buffer overflow when saving AVIF images whose width
 * is not a multiple of #AVIF_BLOCK_SIZE.
 *
 * The encoder may read up to one block past the end of the last row.
 * Interior rows are unaffected because their over-read lands in the next row.
 * Allocate a padded copy so the over-read stays in bounds.
 */
static uchar *imb_save_avif_padding_workaround_begin(const ImBuf *ibuf,
                                                     WriteContext &ctx,
                                                     const bool prefer_float)
{
  /* The bug only affects the 8-bit path (the 10/12-bit path copies per-pixel). */
  const bool use_float = prefer_float && (ibuf->float_buffer.data != nullptr);
  if (use_float || (ibuf->x % AVIF_BLOCK_SIZE) == 0) {
    return nullptr;
  }

  const size_t size_orig = size_t(ibuf->y) * ctx.mem_ystride;
  const size_t size_pad = size_orig + (AVIF_BLOCK_SIZE * ctx.mem_xstride);
  const uchar *src_base = ibuf->byte_buffer.data;

  uchar *buf_padded = MEM_new_array_uninitialized<uchar>(size_pad, __func__);
  memcpy(buf_padded, src_base, size_orig);

  /* `mem_start` points to the last row (images are stored bottom-to-top). */
  const size_t y_flip_offset = size_t(ibuf->y - 1) * ctx.mem_ystride;
  ctx.mem_start = buf_padded + y_flip_offset;

  return buf_padded;
}

static void imb_save_avif_padding_workaround_end(const uchar *buf_padded)
{
  /* May be null. */
  MEM_delete(buf_padded);
}

bool imb_save_avif(ImBuf *ibuf, const char *filepath, int flags)
{
  const int bits_per_sample = (ibuf->foptions.flag & AVIF_10BIT) ? 10 :
                              (ibuf->foptions.flag & AVIF_12BIT) ? 12 :
                                                                   8;
  const bool use_float = bits_per_sample > 8;
  const int file_channels = ibuf->planes >> 3;
  const TypeDesc data_format = use_float ? TypeDesc::UINT16 : TypeDesc::UINT8;

  WriteContext ctx = imb_create_write_context("heif", ibuf, flags, use_float);
  ImageSpec file_spec = imb_create_write_spec(ctx, file_channels, data_format);

  const uchar *buf_padded = nullptr;
  if (OIIO_VERSION_LESS(3, 2, 0)) {
    buf_padded = imb_save_avif_padding_workaround_begin(ibuf, ctx, use_float);
  }

  /* Skip if the float buffer was managed already. */
  if (use_float &&
      (ibuf->float_buffer.colorspace || (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA)))
  {
    file_spec.attribute("oiio:UnassociatedAlpha", 0);
  }
  else {
    file_spec.attribute("oiio:UnassociatedAlpha", 1);
  }

  file_spec.attribute("Compression", fmt::format("avif:{}", int(ibuf->foptions.quality)));
  file_spec.attribute("oiio:BitsPerSample", bits_per_sample);

  const bool ok = imb_oiio_write(ctx, filepath, file_spec);

  imb_save_avif_padding_workaround_end(buf_padded);

  return ok;
}

};  // namespace blender
