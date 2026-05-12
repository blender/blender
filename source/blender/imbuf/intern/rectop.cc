/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <algorithm>
#include <cstdlib>

#include "BLI_math_base.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "IMB_colormanagement.hh"

#include "MEM_guardedalloc.h"

#include <cstring>

namespace blender {

void IMB_blend_color_byte(uchar dst[4],
                          const uchar src1[4],
                          const uchar src2[4],
                          IMB_BlendMode mode)
{
  switch (mode) {
    case IMB_BLEND_MIX:
      blend_color_mix_byte(dst, src1, src2);
      break;
    case IMB_BLEND_ADD:
      blend_color_add_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SUB:
      blend_color_sub_byte(dst, src1, src2);
      break;
    case IMB_BLEND_MUL:
      blend_color_mul_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LIGHTEN:
      blend_color_lighten_byte(dst, src1, src2);
      break;
    case IMB_BLEND_DARKEN:
      blend_color_darken_byte(dst, src1, src2);
      break;
    case IMB_BLEND_ERASE_ALPHA:
      blend_color_erase_alpha_byte(dst, src1, src2);
      break;
    case IMB_BLEND_ADD_ALPHA:
      blend_color_add_alpha_byte(dst, src1, src2);
      break;
    case IMB_BLEND_OVERLAY:
      blend_color_overlay_byte(dst, src1, src2);
      break;
    case IMB_BLEND_HARDLIGHT:
      blend_color_hardlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_COLORBURN:
      blend_color_burn_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARBURN:
      blend_color_linearburn_byte(dst, src1, src2);
      break;
    case IMB_BLEND_COLORDODGE:
      blend_color_dodge_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SCREEN:
      blend_color_screen_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SOFTLIGHT:
      blend_color_softlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_PINLIGHT:
      blend_color_pinlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARLIGHT:
      blend_color_linearlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_VIVIDLIGHT:
      blend_color_vividlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_DIFFERENCE:
      blend_color_difference_byte(dst, src1, src2);
      break;
    case IMB_BLEND_EXCLUSION:
      blend_color_exclusion_byte(dst, src1, src2);
      break;
    case IMB_BLEND_COLOR:
      blend_color_color_byte(dst, src1, src2);
      break;
    case IMB_BLEND_HUE:
      blend_color_hue_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SATURATION:
      blend_color_saturation_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LUMINOSITY:
      blend_color_luminosity_byte(dst, src1, src2);
      break;

    default:
      dst[0] = src1[0];
      dst[1] = src1[1];
      dst[2] = src1[2];
      dst[3] = src1[3];
      break;
  }
}

void IMB_blend_color_float(float dst[4],
                           const float src1[4],
                           const float src2[4],
                           IMB_BlendMode mode)
{
  switch (mode) {
    case IMB_BLEND_MIX:
      blend_color_mix_float(dst, src1, src2);
      break;
    case IMB_BLEND_ADD:
      blend_color_add_float(dst, src1, src2);
      break;
    case IMB_BLEND_SUB:
      blend_color_sub_float(dst, src1, src2);
      break;
    case IMB_BLEND_MUL:
      blend_color_mul_float(dst, src1, src2);
      break;
    case IMB_BLEND_LIGHTEN:
      blend_color_lighten_float(dst, src1, src2);
      break;
    case IMB_BLEND_DARKEN:
      blend_color_darken_float(dst, src1, src2);
      break;
    case IMB_BLEND_ERASE_ALPHA:
      blend_color_erase_alpha_float(dst, src1, src2);
      break;
    case IMB_BLEND_ADD_ALPHA:
      blend_color_add_alpha_float(dst, src1, src2);
      break;
    case IMB_BLEND_OVERLAY:
      blend_color_overlay_float(dst, src1, src2);
      break;
    case IMB_BLEND_HARDLIGHT:
      blend_color_hardlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_COLORBURN:
      blend_color_burn_float(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARBURN:
      blend_color_linearburn_float(dst, src1, src2);
      break;
    case IMB_BLEND_COLORDODGE:
      blend_color_dodge_float(dst, src1, src2);
      break;
    case IMB_BLEND_SCREEN:
      blend_color_screen_float(dst, src1, src2);
      break;
    case IMB_BLEND_SOFTLIGHT:
      blend_color_softlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_PINLIGHT:
      blend_color_pinlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARLIGHT:
      blend_color_linearlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_VIVIDLIGHT:
      blend_color_vividlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_DIFFERENCE:
      blend_color_difference_float(dst, src1, src2);
      break;
    case IMB_BLEND_EXCLUSION:
      blend_color_exclusion_float(dst, src1, src2);
      break;
    case IMB_BLEND_COLOR:
      blend_color_color_float(dst, src1, src2);
      break;
    case IMB_BLEND_HUE:
      blend_color_hue_float(dst, src1, src2);
      break;
    case IMB_BLEND_SATURATION:
      blend_color_saturation_float(dst, src1, src2);
      break;
    case IMB_BLEND_LUMINOSITY:
      blend_color_luminosity_float(dst, src1, src2);
      break;
    default:
      dst[0] = src1[0];
      dst[1] = src1[1];
      dst[2] = src1[2];
      dst[3] = src1[3];
      break;
  }
}

void IMB_blend_color_float(const MutableSpan<float4> dst,
                           const Span<float4> src1,
                           const Span<float4> src2,
                           const IMB_BlendMode mode)
{
  BLI_assert(dst.size() == src1.size());
  BLI_assert(dst.size() == src2.size());

  switch (mode) {
    case IMB_BLEND_MIX:
      for (const int i : dst.index_range()) {
        blend_color_mix_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_ADD:
      for (const int i : dst.index_range()) {
        blend_color_add_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_SUB:
      for (const int i : dst.index_range()) {
        blend_color_sub_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_MUL:
      for (const int i : dst.index_range()) {
        blend_color_mul_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_LIGHTEN:
      for (const int i : dst.index_range()) {
        blend_color_lighten_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_DARKEN:
      for (const int i : dst.index_range()) {
        blend_color_darken_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_ERASE_ALPHA:
      for (const int i : dst.index_range()) {
        blend_color_erase_alpha_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_ADD_ALPHA:
      for (const int i : dst.index_range()) {
        blend_color_add_alpha_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_OVERLAY:
      for (const int i : dst.index_range()) {
        blend_color_overlay_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_HARDLIGHT:
      for (const int i : dst.index_range()) {
        blend_color_hardlight_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_COLORBURN:
      for (const int i : dst.index_range()) {
        blend_color_burn_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_LINEARBURN:
      for (const int i : dst.index_range()) {
        blend_color_linearburn_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_COLORDODGE:
      for (const int i : dst.index_range()) {
        blend_color_dodge_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_SCREEN:
      for (const int i : dst.index_range()) {
        blend_color_screen_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_SOFTLIGHT:
      for (const int i : dst.index_range()) {
        blend_color_softlight_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_PINLIGHT:
      for (const int i : dst.index_range()) {
        blend_color_pinlight_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_LINEARLIGHT:
      for (const int i : dst.index_range()) {
        blend_color_linearlight_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_VIVIDLIGHT:
      for (const int i : dst.index_range()) {
        blend_color_vividlight_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_DIFFERENCE:
      for (const int i : dst.index_range()) {
        blend_color_difference_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_EXCLUSION:
      for (const int i : dst.index_range()) {
        blend_color_exclusion_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_COLOR:
      for (const int i : dst.index_range()) {
        blend_color_color_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_HUE:
      for (const int i : dst.index_range()) {
        blend_color_hue_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_SATURATION:
      for (const int i : dst.index_range()) {
        blend_color_saturation_float(dst[i], src1[i], src2[i]);
      }
      break;
    case IMB_BLEND_LUMINOSITY:
      for (const int i : dst.index_range()) {
        blend_color_luminosity_float(dst[i], src1[i], src2[i]);
      }
      break;
    default:
      for (const int i : dst.index_range()) {
        dst[i] = src1[i];
      }
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name Crop
 * \{ */

static void copy_to_cropped_bufffer(void *dst_void,
                                    const int2 &dst_size,
                                    const void *src_void,
                                    const int2 &src_size,
                                    const int stride,
                                    const int2 &src_rect_pos,
                                    const int2 &dst_rect_pos,
                                    const int2 &rect_size)
{
  BLI_assert(src_size.x > 0 && src_size.y > 0);
  BLI_assert(dst_size.x > 0 && dst_size.y > 0);
  BLI_assert(rect_size.x > 0 && rect_size.y > 0);
  BLI_assert(src_rect_pos.x >= 0 && src_rect_pos.y >= 0);
  BLI_assert(dst_rect_pos.x >= 0 && dst_rect_pos.y >= 0);
  BLI_assert((src_rect_pos.x + rect_size.x) <= src_size.x &&
             (src_rect_pos.y + rect_size.y) <= src_size.y);
  BLI_assert((dst_rect_pos.x + rect_size.x) <= dst_size.x &&
             (dst_rect_pos.y + rect_size.y) <= dst_size.y);
  auto *dst = static_cast<std::byte *>(dst_void);
  const auto *src = static_cast<const std::byte *>(src_void);
  for (const int rect_y : IndexRange(rect_size.y)) {
    const int src_y = src_rect_pos.y + rect_y;
    const int dst_y = dst_rect_pos.y + rect_y;
    const std::byte *row_src = src + size_t(src_size.x) * stride * src_y;
    std::byte *row_dst = dst + size_t(dst_size.x) * stride * dst_y;
    std::copy_n(row_src + size_t(src_rect_pos.x) * stride,
                size_t(rect_size.x) * stride,
                row_dst + size_t(dst_rect_pos.x) * stride);
  }
}

static void *create_cropped_buffer_impl(const void *src_void,
                                        const int2 &src_size,
                                        const int stride,
                                        const int2 &src_rect_pos,
                                        const int2 &rect_size)
{
  BLI_assert(rect_size.x > 0 && rect_size.y > 0);
  const size_t dst_buffer_size = size_t(rect_size.x) * size_t(rect_size.y);
  auto *dst = MEM_new_array_uninitialized<std::byte>(dst_buffer_size * stride, __func__);
  copy_to_cropped_bufffer(
      dst, rect_size, src_void, src_size, stride, src_rect_pos, int2(0, 0), rect_size);
  return dst;
}

static float *create_cropped_buffer(const float *src,
                                    const int2 &src_size,
                                    const int channels,
                                    const int2 &src_rect_pos,
                                    const int2 &rect_size)
{
  /* For some reason channels == 0 means 4-channel default. */
  const int stride = (channels == 0 ? 4 : channels) * sizeof(float);
  return static_cast<float *>(
      create_cropped_buffer_impl(src, src_size, stride, src_rect_pos, rect_size));
}

static uchar *create_cropped_buffer(const uchar *src,
                                    const int2 &src_size,
                                    const int2 &src_rect_pos,
                                    const int2 &rect_size)
{
  /* Byte buffers always have 4 channels. */
  const int stride = 4 * sizeof(uchar);
  return static_cast<uchar *>(
      create_cropped_buffer_impl(src, src_size, stride, src_rect_pos, rect_size));
}

void IMB_copy_rect(float *dst,
                   const int2 &dst_size,
                   const float *src,
                   const int2 &src_size,
                   const int channels,
                   const int2 &src_rect_pos,
                   const int2 &dst_rect_pos,
                   const int2 &rect_size)
{
  /* For some reason channels == 0 means 4-channel default. */
  const int stride = (channels == 0 ? 4 : channels) * sizeof(float);
  copy_to_cropped_bufffer(
      dst, dst_size, src, src_size, stride, src_rect_pos, dst_rect_pos, rect_size);
}

void IMB_copy_rect(uchar *dst,
                   const int2 &dst_size,
                   const uchar *src,
                   const int2 &src_size,
                   const int2 &src_rect_pos,
                   const int2 &dst_rect_pos,
                   const int2 &rect_size)
{
  /* Byte buffers always have 4 channels. */
  const int stride = 4 * sizeof(uchar);
  copy_to_cropped_bufffer(
      dst, dst_size, src, src_size, stride, src_rect_pos, dst_rect_pos, rect_size);
}

void IMB_copy_rect(ImBuf *dst,
                   const ImBuf *src,
                   const int2 &src_rect_pos,
                   const int2 &dst_rect_pos,
                   const int2 &rect_size)
{
  if (src->byte_data() && dst->byte_data()) {
    IMB_copy_rect(dst->byte_data_for_write(),
                  int2(dst->x, dst->y),
                  src->byte_data(),
                  int2(src->x, src->y),
                  src_rect_pos,
                  dst_rect_pos,
                  rect_size);
  }
  if (src->float_data() && dst->float_data()) {
    IMB_copy_rect(dst->float_data_for_write(),
                  int2(dst->x, dst->y),
                  src->float_data(),
                  int2(src->x, src->y),
                  src->channels,
                  src_rect_pos,
                  dst_rect_pos,
                  rect_size);
  }
}

void IMB_crop(ImBuf *ibuf, const int2 &rect_pos, const int2 &rect_size)
{
  const int2 src_size(ibuf->x, ibuf->y);
  if (src_size == rect_size) {
    return;
  }

  if (const uchar *byte_data = ibuf->byte_data()) {
    IMB_assign_byte_buffer(
        ibuf, create_cropped_buffer(byte_data, src_size, rect_pos, rect_size), IB_TAKE_OWNERSHIP);
  }
  if (const float *float_data = ibuf->float_data()) {
    IMB_assign_float_buffer(
        ibuf,
        create_cropped_buffer(float_data, src_size, ibuf->channels, rect_pos, rect_size),
        IB_TAKE_OWNERSHIP);
  }

  ibuf->x = rect_size.x;
  ibuf->y = rect_size.y;
}

/**
 * Re-allocate buffers at a new size.
 */
static void rect_realloc_4bytes(void **buf_p, const uint size[2])
{
  if (*buf_p == nullptr) {
    return;
  }
  MEM_delete_void(*buf_p);
  *buf_p = MEM_new_array_uninitialized<uint>(size_t(size[0]) * size_t(size[1]), __func__);
}

static void rect_realloc_16bytes(void **buf_p, const uint size[2])
{
  if (*buf_p == nullptr) {
    return;
  }
  MEM_delete_void(*buf_p);
  *buf_p = MEM_new_array_uninitialized<uint>(4 * size_t(size[0]) * size_t(size[1]), __func__);
}

void IMB_rect_size_set(ImBuf *ibuf, const uint size[2])
{
  BLI_assert(size[0] > 0 && size[1] > 0);
  if ((size[0] == ibuf->x) && (size[1] == ibuf->y)) {
    return;
  }

  /* TODO(sergey: Validate ownership. */
  rect_realloc_4bytes(reinterpret_cast<void **>(&ibuf->byte_buffer.data), size);
  rect_realloc_16bytes(reinterpret_cast<void **>(&ibuf->float_buffer.data), size);

  ibuf->x = size[0];
  ibuf->y = size[1];
}

/** \} */

/* clipping */

void IMB_rectclip(ImBuf *dbuf,
                  const ImBuf *sbuf,
                  int *destx,
                  int *desty,
                  int *srcx,
                  int *srcy,
                  int *width,
                  int *height)
{
  int tmp;

  if (dbuf == nullptr) {
    return;
  }

  if (*destx < 0) {
    *srcx -= *destx;
    *width += *destx;
    *destx = 0;
  }
  if (*srcx < 0) {
    *destx -= *srcx;
    *width += *srcx;
    *srcx = 0;
  }
  if (*desty < 0) {
    *srcy -= *desty;
    *height += *desty;
    *desty = 0;
  }
  if (*srcy < 0) {
    *desty -= *srcy;
    *height += *srcy;
    *srcy = 0;
  }

  tmp = dbuf->x - *destx;
  *width = std::min(*width, tmp);
  tmp = dbuf->y - *desty;
  *height = std::min(*height, tmp);

  if (sbuf) {
    tmp = sbuf->x - *srcx;
    *width = std::min(*width, tmp);
    tmp = sbuf->y - *srcy;
    *height = std::min(*height, tmp);
  }

  if ((*height <= 0) || (*width <= 0)) {
    *width = 0;
    *height = 0;
  }
}

static void imb_rectclip3(ImBuf *dbuf,
                          const ImBuf *obuf,
                          const ImBuf *sbuf,
                          int *destx,
                          int *desty,
                          int *origx,
                          int *origy,
                          int *srcx,
                          int *srcy,
                          int *width,
                          int *height)
{
  int tmp;

  if (dbuf == nullptr) {
    return;
  }

  if (*destx < 0) {
    *srcx -= *destx;
    *origx -= *destx;
    *width += *destx;
    *destx = 0;
  }
  if (*origx < 0) {
    *destx -= *origx;
    *srcx -= *origx;
    *width += *origx;
    *origx = 0;
  }
  if (*srcx < 0) {
    *destx -= *srcx;
    *origx -= *srcx;
    *width += *srcx;
    *srcx = 0;
  }

  if (*desty < 0) {
    *srcy -= *desty;
    *origy -= *desty;
    *height += *desty;
    *desty = 0;
  }
  if (*origy < 0) {
    *desty -= *origy;
    *srcy -= *origy;
    *height += *origy;
    *origy = 0;
  }
  if (*srcy < 0) {
    *desty -= *srcy;
    *origy -= *srcy;
    *height += *srcy;
    *srcy = 0;
  }

  tmp = dbuf->x - *destx;
  *width = std::min(*width, tmp);
  tmp = dbuf->y - *desty;
  *height = std::min(*height, tmp);

  if (obuf) {
    tmp = obuf->x - *origx;
    *width = std::min(*width, tmp);
    tmp = obuf->y - *origy;
    *height = std::min(*height, tmp);
  }

  if (sbuf) {
    tmp = sbuf->x - *srcx;
    *width = std::min(*width, tmp);
    tmp = sbuf->y - *srcy;
    *height = std::min(*height, tmp);
  }

  if ((*height <= 0) || (*width <= 0)) {
    *width = 0;
    *height = 0;
  }
}

using IMB_blend_func = void (*)(uchar *dst, const uchar *src1, const uchar *src2);
using IMB_blend_func_float = void (*)(float *dst, const float *src1, const float *src2);

void IMB_rectblend(ImBuf *dbuf,
                   const ImBuf *obuf,
                   const ImBuf *sbuf,
                   ushort *dmask,
                   const ushort *curvemask,
                   const ushort *texmask,
                   float mask_max,
                   int destx,
                   int desty,
                   int origx,
                   int origy,
                   int srcx,
                   int srcy,
                   int width,
                   int height,
                   IMB_BlendMode mode,
                   bool accumulate)
{
  uint *drect = nullptr;
  const uint *orect = nullptr;
  const uint *srect = nullptr;
  uint *dr;
  const uint *outr;
  const uint *sr;
  float *drectf = nullptr;
  const float *orectf = nullptr;
  const float *srectf = nullptr;
  float *drf;
  const float *orf;
  const float *srf;
  const ushort *cmaskrect = curvemask, *cmr;
  ushort *dmaskrect = dmask, *dmr;
  const ushort *texmaskrect = texmask, *tmr;
  int srcskip, destskip, origskip, x;
  IMB_blend_func func = nullptr;
  IMB_blend_func_float func_float = nullptr;

  if (dbuf == nullptr || obuf == nullptr) {
    return;
  }

  imb_rectclip3(dbuf, obuf, sbuf, &destx, &desty, &origx, &origy, &srcx, &srcy, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }
  if (sbuf && sbuf->channels != 4) {
    return;
  }
  if (dbuf->channels != 4) {
    return;
  }

  const bool do_char = (sbuf && sbuf->byte_data() && dbuf->byte_data() && obuf->byte_data());
  const bool do_float = (sbuf && sbuf->float_data() && dbuf->float_data() && obuf->float_data());

  if (do_char) {
    drect = reinterpret_cast<uint *>(dbuf->byte_data_for_write()) + size_t(desty) * dbuf->x +
            destx;
    orect = reinterpret_cast<const uint *>(obuf->byte_data()) + size_t(origy) * obuf->x + origx;
  }
  if (do_float) {
    drectf = dbuf->float_data_for_write() + (size_t(desty) * dbuf->x + destx) * 4;
    orectf = obuf->float_data() + (size_t(origy) * obuf->x + origx) * 4;
  }

  if (dmaskrect) {
    dmaskrect += size_t(origy) * obuf->x + origx;
  }

  destskip = dbuf->x;
  origskip = obuf->x;

  if (sbuf) {
    if (do_char) {
      srect = reinterpret_cast<const uint *>(sbuf->byte_data()) + size_t(srcy) * sbuf->x + srcx;
    }
    if (do_float) {
      srectf = sbuf->float_data() + (size_t(srcy) * sbuf->x + srcx) * 4;
    }
    srcskip = sbuf->x;

    if (cmaskrect) {
      cmaskrect += size_t(srcy) * sbuf->x + srcx;
    }

    if (texmaskrect) {
      texmaskrect += size_t(srcy) * sbuf->x + srcx;
    }
  }
  else {
    srect = drect;
    srectf = drectf;
    srcskip = destskip;
  }

  if (mode == IMB_BLEND_COPY_RGB) {
    /* copy rgb only */
    for (; height > 0; height--) {
      if (do_char) {
        dr = drect;
        sr = srect;
        for (x = width; x > 0; x--, dr++, sr++) {
          (reinterpret_cast<char *>(dr))[0] = (reinterpret_cast<const char *>(sr))[0];
          (reinterpret_cast<char *>(dr))[1] = (reinterpret_cast<const char *>(sr))[1];
          (reinterpret_cast<char *>(dr))[2] = (reinterpret_cast<const char *>(sr))[2];
        }
        drect += destskip;
        srect += srcskip;
      }

      if (do_float) {
        drf = drectf;
        srf = srectf;
        for (x = width; x > 0; x--, drf += 4, srf += 4) {
          float map_alpha = (srf[3] == 0.0f) ? drf[3] : drf[3] / srf[3];

          drf[0] = srf[0] * map_alpha;
          drf[1] = srf[1] * map_alpha;
          drf[2] = srf[2] * map_alpha;
        }
        drectf += destskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
  else if (mode == IMB_BLEND_COPY_ALPHA) {
    /* copy alpha only */
    for (; height > 0; height--) {
      if (do_char) {
        dr = drect;
        sr = srect;
        for (x = width; x > 0; x--, dr++, sr++) {
          (reinterpret_cast<char *>(dr))[3] = (reinterpret_cast<const char *>(sr))[3];
        }
        drect += destskip;
        srect += srcskip;
      }

      if (do_float) {
        drf = drectf;
        srf = srectf;
        for (x = width; x > 0; x--, drf += 4, srf += 4) {
          drf[3] = srf[3];
        }
        drectf += destskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
  else {
    switch (mode) {
      case IMB_BLEND_MIX:
      case IMB_BLEND_INTERPOLATE:
        func = blend_color_mix_byte;
        func_float = blend_color_mix_float;
        break;
      case IMB_BLEND_ADD:
        func = blend_color_add_byte;
        func_float = blend_color_add_float;
        break;
      case IMB_BLEND_SUB:
        func = blend_color_sub_byte;
        func_float = blend_color_sub_float;
        break;
      case IMB_BLEND_MUL:
        func = blend_color_mul_byte;
        func_float = blend_color_mul_float;
        break;
      case IMB_BLEND_LIGHTEN:
        func = blend_color_lighten_byte;
        func_float = blend_color_lighten_float;
        break;
      case IMB_BLEND_DARKEN:
        func = blend_color_darken_byte;
        func_float = blend_color_darken_float;
        break;
      case IMB_BLEND_ERASE_ALPHA:
        func = blend_color_erase_alpha_byte;
        func_float = blend_color_erase_alpha_float;
        break;
      case IMB_BLEND_ADD_ALPHA:
        func = blend_color_add_alpha_byte;
        func_float = blend_color_add_alpha_float;
        break;
      case IMB_BLEND_OVERLAY:
        func = blend_color_overlay_byte;
        func_float = blend_color_overlay_float;
        break;
      case IMB_BLEND_HARDLIGHT:
        func = blend_color_hardlight_byte;
        func_float = blend_color_hardlight_float;
        break;
      case IMB_BLEND_COLORBURN:
        func = blend_color_burn_byte;
        func_float = blend_color_burn_float;
        break;
      case IMB_BLEND_LINEARBURN:
        func = blend_color_linearburn_byte;
        func_float = blend_color_linearburn_float;
        break;
      case IMB_BLEND_COLORDODGE:
        func = blend_color_dodge_byte;
        func_float = blend_color_dodge_float;
        break;
      case IMB_BLEND_SCREEN:
        func = blend_color_screen_byte;
        func_float = blend_color_screen_float;
        break;
      case IMB_BLEND_SOFTLIGHT:
        func = blend_color_softlight_byte;
        func_float = blend_color_softlight_float;
        break;
      case IMB_BLEND_PINLIGHT:
        func = blend_color_pinlight_byte;
        func_float = blend_color_pinlight_float;
        break;
      case IMB_BLEND_LINEARLIGHT:
        func = blend_color_linearlight_byte;
        func_float = blend_color_linearlight_float;
        break;
      case IMB_BLEND_VIVIDLIGHT:
        func = blend_color_vividlight_byte;
        func_float = blend_color_vividlight_float;
        break;
      case IMB_BLEND_DIFFERENCE:
        func = blend_color_difference_byte;
        func_float = blend_color_difference_float;
        break;
      case IMB_BLEND_EXCLUSION:
        func = blend_color_exclusion_byte;
        func_float = blend_color_exclusion_float;
        break;
      case IMB_BLEND_COLOR:
        func = blend_color_color_byte;
        func_float = blend_color_color_float;
        break;
      case IMB_BLEND_HUE:
        func = blend_color_hue_byte;
        func_float = blend_color_hue_float;
        break;
      case IMB_BLEND_SATURATION:
        func = blend_color_saturation_byte;
        func_float = blend_color_saturation_float;
        break;
      case IMB_BLEND_LUMINOSITY:
        func = blend_color_luminosity_byte;
        func_float = blend_color_luminosity_float;
        break;
      default:
        break;
    }

    /* blend */
    for (; height > 0; height--) {
      if (do_char) {
        dr = drect;
        outr = orect;
        sr = srect;

        if (cmaskrect) {
          /* mask accumulation for painting */
          cmr = cmaskrect;
          tmr = texmaskrect;

          /* destination mask present, do max alpha masking */
          if (dmaskrect) {
            dmr = dmaskrect;
            for (x = width; x > 0; x--, dr++, outr++, sr++, dmr++, cmr++) {
              const uchar *src = reinterpret_cast<const uchar *>(sr);
              float mask_lim = mask_max * (*cmr);

              if (texmaskrect) {
                mask_lim *= ((*tmr++) / 65535.0f);
              }

              if (src[3] && mask_lim) {
                float mask;

                if (accumulate) {
                  mask = *dmr + mask_lim;
                }
                else {
                  mask = *dmr + mask_lim - (*dmr * (*cmr / 65535.0f));
                }

                mask = min_ff(mask, 65535.0);

                if (mask > *dmr) {
                  uchar mask_src[4];

                  *dmr = mask;

                  mask_src[0] = src[0];
                  mask_src[1] = src[1];
                  mask_src[2] = src[2];

                  if (mode == IMB_BLEND_INTERPOLATE) {
                    mask_src[3] = src[3];
                    blend_color_interpolate_byte(reinterpret_cast<uchar *>(dr),
                                                 reinterpret_cast<const uchar *>(outr),
                                                 mask_src,
                                                 mask / 65535.0f);
                  }
                  else {
                    mask_src[3] = divide_round_i(src[3] * mask, 65535);
                    func(reinterpret_cast<uchar *>(dr),
                         reinterpret_cast<const uchar *>(outr),
                         mask_src);
                  }
                }
              }
            }
            dmaskrect += origskip;
          }
          /* No destination mask buffer, do regular blend with mask-texture if present. */
          else {
            for (x = width; x > 0; x--, dr++, outr++, sr++, cmr++) {
              const uchar *src = reinterpret_cast<const uchar *>(sr);
              float mask = mask_max * float(*cmr);

              if (texmaskrect) {
                mask *= (float(*tmr++) / 65535.0f);
              }

              mask = min_ff(mask, 65535.0);

              if (src[3] && (mask > 0.0f)) {
                uchar mask_src[4];

                mask_src[0] = src[0];
                mask_src[1] = src[1];
                mask_src[2] = src[2];

                if (mode == IMB_BLEND_INTERPOLATE) {
                  mask_src[3] = src[3];
                  blend_color_interpolate_byte(reinterpret_cast<uchar *>(dr),
                                               reinterpret_cast<const uchar *>(outr),
                                               mask_src,
                                               mask / 65535.0f);
                }
                else {
                  mask_src[3] = divide_round_i(src[3] * mask, 65535);
                  func(reinterpret_cast<uchar *>(dr),
                       reinterpret_cast<const uchar *>(outr),
                       mask_src);
                }
              }
            }
          }

          cmaskrect += srcskip;
          if (texmaskrect) {
            texmaskrect += srcskip;
          }
        }
        else {
          /* regular blending */
          for (x = width; x > 0; x--, dr++, outr++, sr++) {
            if ((reinterpret_cast<const uchar *>(sr))[3]) {
              func(reinterpret_cast<uchar *>(dr),
                   reinterpret_cast<const uchar *>(outr),
                   reinterpret_cast<const uchar *>(sr));
            }
          }
        }

        drect += destskip;
        orect += origskip;
        srect += srcskip;
      }

      if (do_float) {
        drf = drectf;
        orf = orectf;
        srf = srectf;

        if (cmaskrect) {
          /* mask accumulation for painting */
          cmr = cmaskrect;
          tmr = texmaskrect;

          /* destination mask present, do max alpha masking */
          if (dmaskrect) {
            dmr = dmaskrect;
            for (x = width; x > 0; x--, drf += 4, orf += 4, srf += 4, dmr++, cmr++) {
              float mask_lim = mask_max * (*cmr);

              if (texmaskrect) {
                mask_lim *= ((*tmr++) / 65535.0f);
              }

              if (srf[3] && mask_lim) {
                float mask;

                if (accumulate) {
                  mask = min_ff(*dmr + mask_lim, 65535.0);
                }
                else {
                  mask = *dmr + mask_lim - (*dmr * (*cmr / 65535.0f));
                }

                mask = min_ff(mask, 65535.0);

                if (mask > *dmr) {
                  *dmr = mask;

                  if (mode == IMB_BLEND_INTERPOLATE) {
                    blend_color_interpolate_float(drf, orf, srf, mask / 65535.0f);
                  }
                  else {
                    float mask_srf[4];
                    mul_v4_v4fl(mask_srf, srf, mask / 65535.0f);
                    func_float(drf, orf, mask_srf);
                  }
                }
              }
            }
            dmaskrect += origskip;
          }
          /* No destination mask buffer, do regular blend with mask-texture if present. */
          else {
            for (x = width; x > 0; x--, drf += 4, orf += 4, srf += 4, cmr++) {
              float mask = mask_max * float(*cmr);

              if (texmaskrect) {
                mask *= (float(*tmr++) / 65535.0f);
              }

              mask = min_ff(mask, 65535.0);

              if (srf[3] && (mask > 0.0f)) {
                if (mode == IMB_BLEND_INTERPOLATE) {
                  blend_color_interpolate_float(drf, orf, srf, mask / 65535.0f);
                }
                else {
                  float mask_srf[4];
                  mul_v4_v4fl(mask_srf, srf, mask / 65535.0f);
                  func_float(drf, orf, mask_srf);
                }
              }
            }
          }

          cmaskrect += srcskip;
          if (texmaskrect) {
            texmaskrect += srcskip;
          }
        }
        else {
          /* regular blending */
          for (x = width; x > 0; x--, drf += 4, orf += 4, srf += 4) {
            if (srf[3] != 0) {
              func_float(drf, orf, srf);
            }
          }
        }

        drectf += destskip * 4;
        orectf += origskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
}

void IMB_rectblend_threaded(ImBuf *dbuf,
                            const ImBuf *obuf,
                            const ImBuf *sbuf,
                            ushort *dmask,
                            const ushort *curvemask,
                            const ushort *texmask,
                            float mask_max,
                            int destx,
                            int desty,
                            int origx,
                            int origy,
                            int srcx,
                            int srcy,
                            int width,
                            int height,
                            IMB_BlendMode mode,
                            bool accumulate)
{
  threading::parallel_for(IndexRange(height), 16, [&](const IndexRange y_range) {
    IMB_rectblend(dbuf,
                  obuf,
                  sbuf,
                  dmask,
                  curvemask,
                  texmask,
                  mask_max,
                  destx,
                  desty + y_range.first(),
                  origx,
                  origy + y_range.first(),
                  srcx,
                  srcy + y_range.first(),
                  width,
                  y_range.size(),
                  mode,
                  accumulate);
  });
}

void IMB_rectfill(ImBuf *drect, const float col[4])
{
  size_t num;

  if (drect->byte_data()) {
    uint *rrect = reinterpret_cast<uint *>(drect->byte_data_for_write());

    char ccol[4];
    unit_float_to_uchar_clamp_v4(ccol, col);

    num = IMB_get_pixel_count(drect);
    for (; num > 0; num--) {
      *rrect++ = *(reinterpret_cast<uint *>(ccol));
    }
  }

  if (drect->float_data()) {
    float *rrectf = drect->float_data_for_write();

    num = IMB_get_pixel_count(drect);
    for (; num > 0; num--) {
      *rrectf++ = col[0];
      *rrectf++ = col[1];
      *rrectf++ = col[2];
      *rrectf++ = col[3];
    }
  }
}

void IMB_rectfill_area(
    ImBuf *ibuf, const float scene_linear_color[4], int x1, int y1, int x2, int y2)
{
  if (!ibuf) {
    return;
  }

  uchar *rect = ibuf->byte_data_for_write();
  float *rectf = ibuf->float_data_for_write();
  const int width = ibuf->x;
  const int height = ibuf->y;

  if ((!rect && !rectf) || scene_linear_color[3] == 0.0f) {
    return;
  }

  /* sanity checks for coords */
  CLAMP(x1, 0, width);
  CLAMP(x2, 0, width);
  CLAMP(y1, 0, height);
  CLAMP(y2, 0, height);

  if (x1 > x2) {
    std::swap(x1, x2);
  }
  if (y1 > y2) {
    std::swap(y1, y2);
  }
  if (x1 == x2 || y1 == y2) {
    return;
  }
  const int x_span = x2 - x1;
  const int y_span = y2 - y1;

  /* Alpha. */
  const float a = scene_linear_color[3];
  /* Alpha inverted. */
  const float ai = 1 - a;
  /* Alpha, inverted, ai/255.0 - Convert char to float at the same time. */
  const float aich = ai / 255.0f;

  if (rect) {
    uchar *pixel;
    uchar chr = 0, chg = 0, chb = 0;
    float fr = 0, fg = 0, fb = 0;

    const int alphaint = unit_float_to_uchar_clamp(a);

    float col[3];
    copy_v3_v3(col, scene_linear_color);
    if (ibuf->byte_buffer.colorspace) {
      IMB_colormanagement_scene_linear_to_colorspace_v3(col, ibuf->byte_buffer.colorspace);
    }
    else {
      IMB_colormanagement_scene_linear_to_srgb_v3(col, scene_linear_color);
    }

    if (a == 1.0f) {
      chr = unit_float_to_uchar_clamp(col[0]);
      chg = unit_float_to_uchar_clamp(col[1]);
      chb = unit_float_to_uchar_clamp(col[2]);
    }
    else {
      fr = col[0] * a;
      fg = col[1] * a;
      fb = col[2] * a;
    }
    for (int j = 0; j < y_span; j++) {
      pixel = rect + (4 * (((size_t(y1) + size_t(j)) * size_t(width)) + size_t(x1)));
      for (int i = 0; i < x_span; i++) {
        BLI_assert(pixel >= rect && pixel < rect + (4 * (size_t(width) * size_t(height))));
        if (a == 1.0f) {
          pixel[0] = chr;
          pixel[1] = chg;
          pixel[2] = chb;
          pixel[3] = 255;
        }
        else {
          int alphatest;
          pixel[0] = char((fr + (float(pixel[0]) * aich)) * 255.0f);
          pixel[1] = char((fg + (float(pixel[1]) * aich)) * 255.0f);
          pixel[2] = char((fb + (float(pixel[2]) * aich)) * 255.0f);
          pixel[3] = char((alphatest = (int(pixel[3]) + alphaint)) < 255 ? alphatest : 255);
        }
        pixel += 4;
      }
    }
  }

  if (rectf) {
    float *pixel;

    for (int j = 0; j < y_span; j++) {
      pixel = rectf + (4 * (((size_t(y1) + j) * size_t(width)) + size_t(x1)));
      for (int i = 0; i < x_span; i++) {
        BLI_assert(pixel >= rectf && pixel < rectf + (4 * (size_t(width) * size_t(height))));
        if (a == 1.0f) {
          pixel[0] = scene_linear_color[0];
          pixel[1] = scene_linear_color[1];
          pixel[2] = scene_linear_color[2];
          pixel[3] = 1.0f;
        }
        else {
          float alphatest;
          pixel[0] = (scene_linear_color[0] * a) + (pixel[0] * ai);
          pixel[1] = (scene_linear_color[1] * a) + (pixel[1] * ai);
          pixel[2] = (scene_linear_color[2] * a) + (pixel[2] * ai);
          pixel[3] = (alphatest = (pixel[3] + a)) < 1.0f ? alphatest : 1.0f;
        }
        pixel += 4;
      }
    }
  }
}

void IMB_rectfill_alpha(ImBuf *ibuf, const float value)
{
  size_t i;

  if (ibuf->float_data() && (ibuf->channels == 4)) {
    float *fbuf = ibuf->float_data_for_write() + 3;
    for (i = IMB_get_pixel_count(ibuf); i > 0; i--, fbuf += 4) {
      *fbuf = value;
    }
  }

  if (uchar *byte_data = ibuf->byte_data_for_write()) {
    const uchar cvalue = value * 255;
    uchar *cbuf = byte_data + 3;
    for (i = IMB_get_pixel_count(ibuf); i > 0; i--, cbuf += 4) {
      *cbuf = cvalue;
    }
  }
}

}  // namespace blender
