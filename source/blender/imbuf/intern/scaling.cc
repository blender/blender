/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <math.h>

#include "BLI_math_color.h"
#include "BLI_math_interp.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "imbuf.h"

#include "IMB_filter.h"

#include "BLI_sys_types.h" /* for intptr_t support */

static void imb_half_x_no_alloc(ImBuf *ibuf2, ImBuf *ibuf1)
{
  uchar *p1, *_p1, *dest;
  short a, r, g, b;
  int x, y;
  float af, rf, gf, bf, *p1f, *_p1f, *destf;
  bool do_rect, do_float;

  do_rect = (ibuf1->byte_buffer.data != nullptr);
  do_float = (ibuf1->float_buffer.data != nullptr && ibuf2->float_buffer.data != nullptr);

  _p1 = ibuf1->byte_buffer.data;
  dest = ibuf2->byte_buffer.data;

  _p1f = ibuf1->float_buffer.data;
  destf = ibuf2->float_buffer.data;

  for (y = ibuf2->y; y > 0; y--) {
    p1 = _p1;
    p1f = _p1f;
    for (x = ibuf2->x; x > 0; x--) {
      if (do_rect) {
        a = *(p1++);
        b = *(p1++);
        g = *(p1++);
        r = *(p1++);
        a += *(p1++);
        b += *(p1++);
        g += *(p1++);
        r += *(p1++);
        *(dest++) = a >> 1;
        *(dest++) = b >> 1;
        *(dest++) = g >> 1;
        *(dest++) = r >> 1;
      }
      if (do_float) {
        af = *(p1f++);
        bf = *(p1f++);
        gf = *(p1f++);
        rf = *(p1f++);
        af += *(p1f++);
        bf += *(p1f++);
        gf += *(p1f++);
        rf += *(p1f++);
        *(destf++) = 0.5f * af;
        *(destf++) = 0.5f * bf;
        *(destf++) = 0.5f * gf;
        *(destf++) = 0.5f * rf;
      }
    }
    if (do_rect) {
      _p1 += (ibuf1->x << 2);
    }
    if (do_float) {
      _p1f += (ibuf1->x << 2);
    }
  }
}

ImBuf *IMB_half_x(ImBuf *ibuf1)
{
  ImBuf *ibuf2;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr && ibuf1->float_buffer.data == nullptr) {
    return nullptr;
  }

  if (ibuf1->x <= 1) {
    return IMB_dupImBuf(ibuf1);
  }

  ibuf2 = IMB_allocImBuf((ibuf1->x) / 2, ibuf1->y, ibuf1->planes, ibuf1->flags);
  if (ibuf2 == nullptr) {
    return nullptr;
  }

  imb_half_x_no_alloc(ibuf2, ibuf1);

  return ibuf2;
}

ImBuf *IMB_double_fast_x(ImBuf *ibuf1)
{
  ImBuf *ibuf2;
  int *p1, *dest, i, col, do_rect, do_float;
  float *p1f, *destf;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr && ibuf1->float_buffer.data == nullptr) {
    return nullptr;
  }

  do_rect = (ibuf1->byte_buffer.data != nullptr);
  do_float = (ibuf1->float_buffer.data != nullptr);

  ibuf2 = IMB_allocImBuf(2 * ibuf1->x, ibuf1->y, ibuf1->planes, ibuf1->flags);
  if (ibuf2 == nullptr) {
    return nullptr;
  }

  p1 = (int *)ibuf1->byte_buffer.data;
  dest = (int *)ibuf2->byte_buffer.data;
  p1f = (float *)ibuf1->float_buffer.data;
  destf = (float *)ibuf2->float_buffer.data;

  for (i = ibuf1->y * ibuf1->x; i > 0; i--) {
    if (do_rect) {
      col = *p1++;
      *dest++ = col;
      *dest++ = col;
    }
    if (do_float) {
      destf[0] = destf[4] = p1f[0];
      destf[1] = destf[5] = p1f[1];
      destf[2] = destf[6] = p1f[2];
      destf[3] = destf[7] = p1f[3];
      destf += 8;
      p1f += 4;
    }
  }

  return ibuf2;
}

ImBuf *IMB_double_x(ImBuf *ibuf1)
{
  ImBuf *ibuf2;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr && ibuf1->float_buffer.data == nullptr) {
    return nullptr;
  }

  ibuf2 = IMB_double_fast_x(ibuf1);

  imb_filterx(ibuf2);
  return ibuf2;
}

static void imb_half_y_no_alloc(ImBuf *ibuf2, ImBuf *ibuf1)
{
  uchar *p1, *p2, *_p1, *dest;
  short a, r, g, b;
  int x, y;
  float af, rf, gf, bf, *p1f, *p2f, *_p1f, *destf;

  p1 = p2 = nullptr;
  p1f = p2f = nullptr;

  const bool do_rect = (ibuf1->byte_buffer.data != nullptr);
  const bool do_float = (ibuf1->float_buffer.data != nullptr &&
                         ibuf2->float_buffer.data != nullptr);

  _p1 = ibuf1->byte_buffer.data;
  dest = ibuf2->byte_buffer.data;
  _p1f = (float *)ibuf1->float_buffer.data;
  destf = (float *)ibuf2->float_buffer.data;

  for (y = ibuf2->y; y > 0; y--) {
    if (do_rect) {
      p1 = _p1;
      p2 = _p1 + (ibuf1->x << 2);
    }
    if (do_float) {
      p1f = _p1f;
      p2f = _p1f + (ibuf1->x << 2);
    }
    for (x = ibuf2->x; x > 0; x--) {
      if (do_rect) {
        a = *(p1++);
        b = *(p1++);
        g = *(p1++);
        r = *(p1++);
        a += *(p2++);
        b += *(p2++);
        g += *(p2++);
        r += *(p2++);
        *(dest++) = a >> 1;
        *(dest++) = b >> 1;
        *(dest++) = g >> 1;
        *(dest++) = r >> 1;
      }
      if (do_float) {
        af = *(p1f++);
        bf = *(p1f++);
        gf = *(p1f++);
        rf = *(p1f++);
        af += *(p2f++);
        bf += *(p2f++);
        gf += *(p2f++);
        rf += *(p2f++);
        *(destf++) = 0.5f * af;
        *(destf++) = 0.5f * bf;
        *(destf++) = 0.5f * gf;
        *(destf++) = 0.5f * rf;
      }
    }
    if (do_rect) {
      _p1 += (ibuf1->x << 3);
    }
    if (do_float) {
      _p1f += (ibuf1->x << 3);
    }
  }
}

ImBuf *IMB_half_y(ImBuf *ibuf1)
{
  ImBuf *ibuf2;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr && ibuf1->float_buffer.data == nullptr) {
    return nullptr;
  }

  if (ibuf1->y <= 1) {
    return IMB_dupImBuf(ibuf1);
  }

  ibuf2 = IMB_allocImBuf(ibuf1->x, (ibuf1->y) / 2, ibuf1->planes, ibuf1->flags);
  if (ibuf2 == nullptr) {
    return nullptr;
  }

  imb_half_y_no_alloc(ibuf2, ibuf1);

  return ibuf2;
}

ImBuf *IMB_double_fast_y(ImBuf *ibuf1)
{
  ImBuf *ibuf2;
  int *p1, *dest1, *dest2;
  float *p1f, *dest1f, *dest2f;
  int x, y;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr && ibuf1->float_buffer.data == nullptr) {
    return nullptr;
  }

  const bool do_rect = (ibuf1->byte_buffer.data != nullptr);
  const bool do_float = (ibuf1->float_buffer.data != nullptr);

  ibuf2 = IMB_allocImBuf(ibuf1->x, 2 * ibuf1->y, ibuf1->planes, ibuf1->flags);
  if (ibuf2 == nullptr) {
    return nullptr;
  }

  p1 = (int *)ibuf1->byte_buffer.data;
  dest1 = (int *)ibuf2->byte_buffer.data;
  p1f = (float *)ibuf1->float_buffer.data;
  dest1f = (float *)ibuf2->float_buffer.data;

  for (y = ibuf1->y; y > 0; y--) {
    if (do_rect) {
      dest2 = dest1 + ibuf2->x;
      for (x = ibuf2->x; x > 0; x--) {
        *dest1++ = *dest2++ = *p1++;
      }
      dest1 = dest2;
    }
    if (do_float) {
      dest2f = dest1f + (4 * ibuf2->x);
      for (x = ibuf2->x * 4; x > 0; x--) {
        *dest1f++ = *dest2f++ = *p1f++;
      }
      dest1f = dest2f;
    }
  }

  return ibuf2;
}

ImBuf *IMB_double_y(ImBuf *ibuf1)
{
  ImBuf *ibuf2;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr) {
    return nullptr;
  }

  ibuf2 = IMB_double_fast_y(ibuf1);

  IMB_filtery(ibuf2);
  return ibuf2;
}

/* pretty much specific functions which converts uchar <-> ushort but assumes
 * ushort range of 255*255 which is more convenient here
 */
MINLINE void straight_uchar_to_premul_ushort(ushort result[4], const uchar color[4])
{
  ushort alpha = color[3];

  result[0] = color[0] * alpha;
  result[1] = color[1] * alpha;
  result[2] = color[2] * alpha;
  result[3] = alpha * 256;
}

MINLINE void premul_ushort_to_straight_uchar(uchar *result, const ushort color[4])
{
  if (color[3] <= 255) {
    result[0] = unit_ushort_to_uchar(color[0]);
    result[1] = unit_ushort_to_uchar(color[1]);
    result[2] = unit_ushort_to_uchar(color[2]);
    result[3] = unit_ushort_to_uchar(color[3]);
  }
  else {
    ushort alpha = color[3] / 256;

    result[0] = unit_ushort_to_uchar(ushort(color[0] / alpha * 256));
    result[1] = unit_ushort_to_uchar(ushort(color[1] / alpha * 256));
    result[2] = unit_ushort_to_uchar(ushort(color[2] / alpha * 256));
    result[3] = unit_ushort_to_uchar(color[3]);
  }
}

void imb_onehalf_no_alloc(ImBuf *ibuf2, ImBuf *ibuf1)
{
  int x, y;
  const bool do_rect = (ibuf1->byte_buffer.data != nullptr);
  const bool do_float = (ibuf1->float_buffer.data != nullptr) &&
                        (ibuf2->float_buffer.data != nullptr);

  if (do_rect && (ibuf2->byte_buffer.data == nullptr)) {
    imb_addrectImBuf(ibuf2);
  }

  if (ibuf1->x <= 1) {
    imb_half_y_no_alloc(ibuf2, ibuf1);
    return;
  }
  if (ibuf1->y <= 1) {
    imb_half_x_no_alloc(ibuf2, ibuf1);
    return;
  }

  if (do_rect) {
    uchar *cp1, *cp2, *dest;

    cp1 = ibuf1->byte_buffer.data;
    dest = ibuf2->byte_buffer.data;

    for (y = ibuf2->y; y > 0; y--) {
      cp2 = cp1 + (ibuf1->x << 2);
      for (x = ibuf2->x; x > 0; x--) {
        ushort p1i[8], p2i[8], desti[4];

        straight_uchar_to_premul_ushort(p1i, cp1);
        straight_uchar_to_premul_ushort(p2i, cp2);
        straight_uchar_to_premul_ushort(p1i + 4, cp1 + 4);
        straight_uchar_to_premul_ushort(p2i + 4, cp2 + 4);

        desti[0] = (uint(p1i[0]) + p2i[0] + p1i[4] + p2i[4]) >> 2;
        desti[1] = (uint(p1i[1]) + p2i[1] + p1i[5] + p2i[5]) >> 2;
        desti[2] = (uint(p1i[2]) + p2i[2] + p1i[6] + p2i[6]) >> 2;
        desti[3] = (uint(p1i[3]) + p2i[3] + p1i[7] + p2i[7]) >> 2;

        premul_ushort_to_straight_uchar(dest, desti);

        cp1 += 8;
        cp2 += 8;
        dest += 4;
      }
      cp1 = cp2;
      if (ibuf1->x & 1) {
        cp1 += 4;
      }
    }
  }

  if (do_float) {
    float *p1f, *p2f, *destf;

    p1f = ibuf1->float_buffer.data;
    destf = ibuf2->float_buffer.data;
    for (y = ibuf2->y; y > 0; y--) {
      p2f = p1f + (ibuf1->x << 2);
      for (x = ibuf2->x; x > 0; x--) {
        destf[0] = 0.25f * (p1f[0] + p2f[0] + p1f[4] + p2f[4]);
        destf[1] = 0.25f * (p1f[1] + p2f[1] + p1f[5] + p2f[5]);
        destf[2] = 0.25f * (p1f[2] + p2f[2] + p1f[6] + p2f[6]);
        destf[3] = 0.25f * (p1f[3] + p2f[3] + p1f[7] + p2f[7]);
        p1f += 8;
        p2f += 8;
        destf += 4;
      }
      p1f = p2f;
      if (ibuf1->x & 1) {
        p1f += 4;
      }
    }
  }
}

ImBuf *IMB_onehalf(ImBuf *ibuf1)
{
  ImBuf *ibuf2;

  if (ibuf1 == nullptr) {
    return nullptr;
  }
  if (ibuf1->byte_buffer.data == nullptr && ibuf1->float_buffer.data == nullptr) {
    return nullptr;
  }

  if (ibuf1->x <= 1) {
    return IMB_half_y(ibuf1);
  }
  if (ibuf1->y <= 1) {
    return IMB_half_x(ibuf1);
  }

  ibuf2 = IMB_allocImBuf((ibuf1->x) / 2, (ibuf1->y) / 2, ibuf1->planes, ibuf1->flags);
  if (ibuf2 == nullptr) {
    return nullptr;
  }

  imb_onehalf_no_alloc(ibuf2, ibuf1);

  return ibuf2;
}

/* q_scale_linear_interpolation helper functions */

static void enlarge_picture_byte(
    uchar *src, uchar *dst, int src_width, int src_height, int dst_width, int dst_height)
{
  double ratiox = double(dst_width - 1.0) / double(src_width - 1.001);
  double ratioy = double(dst_height - 1.0) / double(src_height - 1.001);
  uintptr_t x_src, dx_src, x_dst;
  uintptr_t y_src, dy_src, y_dst;

  dx_src = 65536.0 / ratiox;
  dy_src = 65536.0 / ratioy;

  y_src = 0;
  for (y_dst = 0; y_dst < dst_height; y_dst++) {
    uchar *line1 = src + (y_src >> 16) * 4 * src_width;
    uchar *line2 = line1 + 4 * src_width;
    uintptr_t weight1y = 65536 - (y_src & 0xffff);
    uintptr_t weight2y = 65536 - weight1y;

    if ((y_src >> 16) == src_height - 1) {
      line2 = line1;
    }

    x_src = 0;
    for (x_dst = 0; x_dst < dst_width; x_dst++) {
      uintptr_t weight1x = 65536 - (x_src & 0xffff);
      uintptr_t weight2x = 65536 - weight1x;

      ulong x = (x_src >> 16) * 4;

      *dst++ = ((((line1[x] * weight1y) >> 16) * weight1x) >> 16) +
               ((((line2[x] * weight2y) >> 16) * weight1x) >> 16) +
               ((((line1[4 + x] * weight1y) >> 16) * weight2x) >> 16) +
               ((((line2[4 + x] * weight2y) >> 16) * weight2x) >> 16);

      *dst++ = ((((line1[x + 1] * weight1y) >> 16) * weight1x) >> 16) +
               ((((line2[x + 1] * weight2y) >> 16) * weight1x) >> 16) +
               ((((line1[4 + x + 1] * weight1y) >> 16) * weight2x) >> 16) +
               ((((line2[4 + x + 1] * weight2y) >> 16) * weight2x) >> 16);

      *dst++ = ((((line1[x + 2] * weight1y) >> 16) * weight1x) >> 16) +
               ((((line2[x + 2] * weight2y) >> 16) * weight1x) >> 16) +
               ((((line1[4 + x + 2] * weight1y) >> 16) * weight2x) >> 16) +
               ((((line2[4 + x + 2] * weight2y) >> 16) * weight2x) >> 16);

      *dst++ = ((((line1[x + 3] * weight1y) >> 16) * weight1x) >> 16) +
               ((((line2[x + 3] * weight2y) >> 16) * weight1x) >> 16) +
               ((((line1[4 + x + 3] * weight1y) >> 16) * weight2x) >> 16) +
               ((((line2[4 + x + 3] * weight2y) >> 16) * weight2x) >> 16);

      x_src += dx_src;
    }
    y_src += dy_src;
  }
}

struct scale_outpix_byte {
  uintptr_t r;
  uintptr_t g;
  uintptr_t b;
  uintptr_t a;

  uintptr_t weight;
};

static void shrink_picture_byte(
    uchar *src, uchar *dst, int src_width, int src_height, int dst_width, int dst_height)
{
  double ratiox = double(dst_width) / double(src_width);
  double ratioy = double(dst_height) / double(src_height);
  uintptr_t x_src, dx_dst, x_dst;
  uintptr_t y_src, dy_dst, y_dst;
  intptr_t y_counter;
  uchar *dst_begin = dst;

  scale_outpix_byte *dst_line1 = nullptr;
  scale_outpix_byte *dst_line2 = nullptr;

  dst_line1 = (scale_outpix_byte *)MEM_callocN((dst_width + 1) * sizeof(scale_outpix_byte),
                                               "shrink_picture_byte 1");
  dst_line2 = (scale_outpix_byte *)MEM_callocN((dst_width + 1) * sizeof(scale_outpix_byte),
                                               "shrink_picture_byte 2");

  dx_dst = 65536.0 * ratiox;
  dy_dst = 65536.0 * ratioy;

  y_dst = 0;
  y_counter = 65536;
  for (y_src = 0; y_src < src_height; y_src++) {
    uchar *line = src + y_src * 4 * src_width;
    uintptr_t weight1y = 65535 - (y_dst & 0xffff);
    uintptr_t weight2y = 65535 - weight1y;
    x_dst = 0;
    for (x_src = 0; x_src < src_width; x_src++) {
      uintptr_t weight1x = 65535 - (x_dst & 0xffff);
      uintptr_t weight2x = 65535 - weight1x;

      uintptr_t x = x_dst >> 16;

      uintptr_t w;

      w = (weight1y * weight1x) >> 16;

      /* Ensure correct rounding, without this you get ugly banding,
       * or too low color values (ton). */
      dst_line1[x].r += (line[0] * w + 32767) >> 16;
      dst_line1[x].g += (line[1] * w + 32767) >> 16;
      dst_line1[x].b += (line[2] * w + 32767) >> 16;
      dst_line1[x].a += (line[3] * w + 32767) >> 16;
      dst_line1[x].weight += w;

      w = (weight2y * weight1x) >> 16;

      dst_line2[x].r += (line[0] * w + 32767) >> 16;
      dst_line2[x].g += (line[1] * w + 32767) >> 16;
      dst_line2[x].b += (line[2] * w + 32767) >> 16;
      dst_line2[x].a += (line[3] * w + 32767) >> 16;
      dst_line2[x].weight += w;

      w = (weight1y * weight2x) >> 16;

      dst_line1[x + 1].r += (line[0] * w + 32767) >> 16;
      dst_line1[x + 1].g += (line[1] * w + 32767) >> 16;
      dst_line1[x + 1].b += (line[2] * w + 32767) >> 16;
      dst_line1[x + 1].a += (line[3] * w + 32767) >> 16;
      dst_line1[x + 1].weight += w;

      w = (weight2y * weight2x) >> 16;

      dst_line2[x + 1].r += (line[0] * w + 32767) >> 16;
      dst_line2[x + 1].g += (line[1] * w + 32767) >> 16;
      dst_line2[x + 1].b += (line[2] * w + 32767) >> 16;
      dst_line2[x + 1].a += (line[3] * w + 32767) >> 16;
      dst_line2[x + 1].weight += w;

      x_dst += dx_dst;
      line += 4;
    }

    y_dst += dy_dst;
    y_counter -= dy_dst;
    if (y_counter < 0) {
      int val;
      uintptr_t x;
      scale_outpix_byte *temp;

      y_counter += 65536;

      for (x = 0; x < dst_width; x++) {
        uintptr_t f = 0x80000000UL / dst_line1[x].weight;
        *dst++ = (val = (dst_line1[x].r * f) >> 15) > 255 ? 255 : val;
        *dst++ = (val = (dst_line1[x].g * f) >> 15) > 255 ? 255 : val;
        *dst++ = (val = (dst_line1[x].b * f) >> 15) > 255 ? 255 : val;
        *dst++ = (val = (dst_line1[x].a * f) >> 15) > 255 ? 255 : val;
      }
      memset(dst_line1, 0, dst_width * sizeof(scale_outpix_byte));
      temp = dst_line1;
      dst_line1 = dst_line2;
      dst_line2 = temp;
    }
  }
  if (dst - dst_begin < dst_width * dst_height * 4) {
    int val;
    uintptr_t x;
    for (x = 0; x < dst_width; x++) {
      uintptr_t f = 0x80000000UL / dst_line1[x].weight;
      *dst++ = (val = (dst_line1[x].r * f) >> 15) > 255 ? 255 : val;
      *dst++ = (val = (dst_line1[x].g * f) >> 15) > 255 ? 255 : val;
      *dst++ = (val = (dst_line1[x].b * f) >> 15) > 255 ? 255 : val;
      *dst++ = (val = (dst_line1[x].a * f) >> 15) > 255 ? 255 : val;
    }
  }
  MEM_freeN(dst_line1);
  MEM_freeN(dst_line2);
}

static void q_scale_byte(
    uchar *in, uchar *out, int in_width, int in_height, int dst_width, int dst_height)
{
  if (dst_width > in_width && dst_height > in_height) {
    enlarge_picture_byte(in, out, in_width, in_height, dst_width, dst_height);
  }
  else if (dst_width < in_width && dst_height < in_height) {
    shrink_picture_byte(in, out, in_width, in_height, dst_width, dst_height);
  }
}

static void enlarge_picture_float(
    float *src, float *dst, int src_width, int src_height, int dst_width, int dst_height)
{
  double ratiox = double(dst_width - 1.0) / double(src_width - 1.001);
  double ratioy = double(dst_height - 1.0) / double(src_height - 1.001);
  uintptr_t x_dst;
  uintptr_t y_dst;
  double x_src, dx_src;
  double y_src, dy_src;

  dx_src = 1.0 / ratiox;
  dy_src = 1.0 / ratioy;

  y_src = 0;
  for (y_dst = 0; y_dst < dst_height; y_dst++) {
    float *line1 = src + int(y_src) * 4 * src_width;
    const float *line2 = line1 + 4 * src_width;
    const float weight1y = float(1.0 - (y_src - int(y_src)));
    const float weight2y = 1.0f - weight1y;

    if (int(y_src) == src_height - 1) {
      line2 = line1;
    }

    x_src = 0;
    for (x_dst = 0; x_dst < dst_width; x_dst++) {
      const float weight1x = float(1.0 - (x_src - int(x_src)));
      const float weight2x = float(1.0f - weight1x);

      const float w11 = weight1y * weight1x;
      const float w21 = weight2y * weight1x;
      const float w12 = weight1y * weight2x;
      const float w22 = weight2y * weight2x;

      uintptr_t x = int(x_src) * 4;

      *dst++ = line1[x] * w11 + line2[x] * w21 + line1[4 + x] * w12 + line2[4 + x] * w22;

      *dst++ = line1[x + 1] * w11 + line2[x + 1] * w21 + line1[4 + x + 1] * w12 +
               line2[4 + x + 1] * w22;

      *dst++ = line1[x + 2] * w11 + line2[x + 2] * w21 + line1[4 + x + 2] * w12 +
               line2[4 + x + 2] * w22;

      *dst++ = line1[x + 3] * w11 + line2[x + 3] * w21 + line1[4 + x + 3] * w12 +
               line2[4 + x + 3] * w22;

      x_src += dx_src;
    }
    y_src += dy_src;
  }
}

struct scale_outpix_float {
  float r;
  float g;
  float b;
  float a;

  float weight;
};

static void shrink_picture_float(
    const float *src, float *dst, int src_width, int src_height, int dst_width, int dst_height)
{
  double ratiox = double(dst_width) / double(src_width);
  double ratioy = double(dst_height) / double(src_height);
  uintptr_t x_src;
  uintptr_t y_src;
  float dx_dst, x_dst;
  float dy_dst, y_dst;
  float y_counter;
  const float *dst_begin = dst;

  scale_outpix_float *dst_line1;
  scale_outpix_float *dst_line2;

  dst_line1 = (scale_outpix_float *)MEM_callocN((dst_width + 1) * sizeof(scale_outpix_float),
                                                "shrink_picture_float 1");
  dst_line2 = (scale_outpix_float *)MEM_callocN((dst_width + 1) * sizeof(scale_outpix_float),
                                                "shrink_picture_float 2");

  dx_dst = ratiox;
  dy_dst = ratioy;

  y_dst = 0;
  y_counter = 1.0;
  for (y_src = 0; y_src < src_height; y_src++) {
    const float *line = src + y_src * 4 * src_width;
    uintptr_t weight1y = 1.0f - (y_dst - int(y_dst));
    uintptr_t weight2y = 1.0f - weight1y;
    x_dst = 0;
    for (x_src = 0; x_src < src_width; x_src++) {
      uintptr_t weight1x = 1.0f - (x_dst - int(x_dst));
      uintptr_t weight2x = 1.0f - weight1x;

      uintptr_t x = int(x_dst);

      float w;

      w = weight1y * weight1x;

      dst_line1[x].r += line[0] * w;
      dst_line1[x].g += line[1] * w;
      dst_line1[x].b += line[2] * w;
      dst_line1[x].a += line[3] * w;
      dst_line1[x].weight += w;

      w = weight2y * weight1x;

      dst_line2[x].r += line[0] * w;
      dst_line2[x].g += line[1] * w;
      dst_line2[x].b += line[2] * w;
      dst_line2[x].a += line[3] * w;
      dst_line2[x].weight += w;

      w = weight1y * weight2x;

      dst_line1[x + 1].r += line[0] * w;
      dst_line1[x + 1].g += line[1] * w;
      dst_line1[x + 1].b += line[2] * w;
      dst_line1[x + 1].a += line[3] * w;
      dst_line1[x + 1].weight += w;

      w = weight2y * weight2x;

      dst_line2[x + 1].r += line[0] * w;
      dst_line2[x + 1].g += line[1] * w;
      dst_line2[x + 1].b += line[2] * w;
      dst_line2[x + 1].a += line[3] * w;
      dst_line2[x + 1].weight += w;

      x_dst += dx_dst;
      line += 4;
    }

    y_dst += dy_dst;
    y_counter -= dy_dst;
    if (y_counter < 0) {
      uintptr_t x;
      scale_outpix_float *temp;

      y_counter += 1.0f;

      for (x = 0; x < dst_width; x++) {
        float f = 1.0f / dst_line1[x].weight;
        *dst++ = dst_line1[x].r * f;
        *dst++ = dst_line1[x].g * f;
        *dst++ = dst_line1[x].b * f;
        *dst++ = dst_line1[x].a * f;
      }
      memset(dst_line1, 0, dst_width * sizeof(scale_outpix_float));
      temp = dst_line1;
      dst_line1 = dst_line2;
      dst_line2 = temp;
    }
  }
  if (dst - dst_begin < dst_width * dst_height * 4) {
    uintptr_t x;
    for (x = 0; x < dst_width; x++) {
      float f = 1.0f / dst_line1[x].weight;
      *dst++ = dst_line1[x].r * f;
      *dst++ = dst_line1[x].g * f;
      *dst++ = dst_line1[x].b * f;
      *dst++ = dst_line1[x].a * f;
    }
  }
  MEM_freeN(dst_line1);
  MEM_freeN(dst_line2);
}

static void q_scale_float(
    float *in, float *out, int in_width, int in_height, int dst_width, int dst_height)
{
  if (dst_width > in_width && dst_height > in_height) {
    enlarge_picture_float(in, out, in_width, in_height, dst_width, dst_height);
  }
  else if (dst_width < in_width && dst_height < in_height) {
    shrink_picture_float(in, out, in_width, in_height, dst_width, dst_height);
  }
}

/**
 * q_scale_linear_interpolation (derived from `ppmqscale`, http://libdv.sf.net)
 *
 * q stands for quick _and_ quality :)
 *
 * only handles common cases when we either
 *
 * scale both, x and y or
 * shrink both, x and y
 *
 * but that is pretty fast:
 * - does only blit once instead of two passes like the old code
 *   (fewer cache misses)
 * - uses fixed point integer arithmetic for byte buffers
 * - doesn't branch in tight loops
 *
 * Should be comparable in speed to the ImBuf ..._fast functions at least
 * for byte-buffers.
 *
 * NOTE: disabled, due to unacceptable inaccuracy and quality loss, see bug #18609 (ton)
 */
static bool q_scale_linear_interpolation(ImBuf *ibuf, int newx, int newy)
{
  if ((newx >= ibuf->x && newy <= ibuf->y) || (newx <= ibuf->x && newy >= ibuf->y)) {
    return false;
  }

  if (ibuf->byte_buffer.data) {
    uchar *newrect = static_cast<uchar *>(MEM_mallocN(sizeof(int) * newx * newy, "q_scale rect"));
    q_scale_byte(ibuf->byte_buffer.data, newrect, ibuf->x, ibuf->y, newx, newy);

    IMB_assign_byte_buffer(ibuf, newrect, IB_TAKE_OWNERSHIP);
  }
  if (ibuf->float_buffer.data) {
    float *newrect = static_cast<float *>(
        MEM_mallocN(sizeof(float[4]) * newx * newy, "q_scale rectfloat"));
    q_scale_float(ibuf->float_buffer.data, newrect, ibuf->x, ibuf->y, newx, newy);

    IMB_assign_float_buffer(ibuf, newrect, IB_TAKE_OWNERSHIP);
  }

  ibuf->x = newx;
  ibuf->y = newy;

  return true;
}

static ImBuf *scaledownx(ImBuf *ibuf, int newx)
{
  const bool do_rect = (ibuf->byte_buffer.data != nullptr);
  const bool do_float = (ibuf->float_buffer.data != nullptr);
  const size_t rect_size = IMB_get_rect_len(ibuf) * 4;

  uchar *rect, *_newrect, *newrect;
  float *rectf, *_newrectf, *newrectf;
  float sample, add, val[4], nval[4], valf[4], nvalf[4];
  int x, y;

  rectf = _newrectf = newrectf = nullptr;
  rect = _newrect = newrect = nullptr;
  nval[0] = nval[1] = nval[2] = nval[3] = 0.0f;
  nvalf[0] = nvalf[1] = nvalf[2] = nvalf[3] = 0.0f;

  if (!do_rect && !do_float) {
    return ibuf;
  }

  if (do_rect) {
    _newrect = static_cast<uchar *>(MEM_mallocN(sizeof(uchar[4]) * newx * ibuf->y, "scaledownx"));
    if (_newrect == nullptr) {
      return ibuf;
    }
  }
  if (do_float) {
    _newrectf = static_cast<float *>(
        MEM_mallocN(sizeof(float[4]) * newx * ibuf->y, "scaledownxf"));
    if (_newrectf == nullptr) {
      if (_newrect) {
        MEM_freeN(_newrect);
      }
      return ibuf;
    }
  }

  add = (ibuf->x - 0.01) / newx;

  if (do_rect) {
    rect = ibuf->byte_buffer.data;
    newrect = _newrect;
  }
  if (do_float) {
    rectf = ibuf->float_buffer.data;
    newrectf = _newrectf;
  }

  for (y = ibuf->y; y > 0; y--) {
    sample = 0.0f;
    val[0] = val[1] = val[2] = val[3] = 0.0f;
    valf[0] = valf[1] = valf[2] = valf[3] = 0.0f;

    for (x = newx; x > 0; x--) {
      if (do_rect) {
        nval[0] = -val[0] * sample;
        nval[1] = -val[1] * sample;
        nval[2] = -val[2] * sample;
        nval[3] = -val[3] * sample;
      }
      if (do_float) {
        nvalf[0] = -valf[0] * sample;
        nvalf[1] = -valf[1] * sample;
        nvalf[2] = -valf[2] * sample;
        nvalf[3] = -valf[3] * sample;
      }

      sample += add;

      while (sample >= 1.0f) {
        sample -= 1.0f;

        if (do_rect) {
          nval[0] += rect[0];
          nval[1] += rect[1];
          nval[2] += rect[2];
          nval[3] += rect[3];
          rect += 4;
        }
        if (do_float) {
          nvalf[0] += rectf[0];
          nvalf[1] += rectf[1];
          nvalf[2] += rectf[2];
          nvalf[3] += rectf[3];
          rectf += 4;
        }
      }

      if (do_rect) {
        val[0] = rect[0];
        val[1] = rect[1];
        val[2] = rect[2];
        val[3] = rect[3];
        rect += 4;

        newrect[0] = roundf((nval[0] + sample * val[0]) / add);
        newrect[1] = roundf((nval[1] + sample * val[1]) / add);
        newrect[2] = roundf((nval[2] + sample * val[2]) / add);
        newrect[3] = roundf((nval[3] + sample * val[3]) / add);

        newrect += 4;
      }
      if (do_float) {

        valf[0] = rectf[0];
        valf[1] = rectf[1];
        valf[2] = rectf[2];
        valf[3] = rectf[3];
        rectf += 4;

        newrectf[0] = ((nvalf[0] + sample * valf[0]) / add);
        newrectf[1] = ((nvalf[1] + sample * valf[1]) / add);
        newrectf[2] = ((nvalf[2] + sample * valf[2]) / add);
        newrectf[3] = ((nvalf[3] + sample * valf[3]) / add);

        newrectf += 4;
      }

      sample -= 1.0f;
    }
  }

  if (do_rect) {
    // printf("%ld %ld\n", (uchar *)rect - ibuf->byte_buffer.data, rect_size);
    BLI_assert((uchar *)rect - ibuf->byte_buffer.data == rect_size); /* see bug #26502. */

    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, _newrect, IB_TAKE_OWNERSHIP);
  }
  if (do_float) {
    // printf("%ld %ld\n", rectf - ibuf->float_buffer.data, rect_size);
    BLI_assert((rectf - ibuf->float_buffer.data) == rect_size); /* see bug #26502. */

    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, _newrectf, IB_TAKE_OWNERSHIP);
  }

  (void)rect_size; /* UNUSED in release builds */

  ibuf->x = newx;
  return ibuf;
}

static ImBuf *scaledowny(ImBuf *ibuf, int newy)
{
  const bool do_rect = (ibuf->byte_buffer.data != nullptr);
  const bool do_float = (ibuf->float_buffer.data != nullptr);
  const size_t rect_size = IMB_get_rect_len(ibuf) * 4;

  uchar *rect, *_newrect, *newrect;
  float *rectf, *_newrectf, *newrectf;
  float sample, add, val[4], nval[4], valf[4], nvalf[4];
  int x, y, skipx;

  rectf = _newrectf = newrectf = nullptr;
  rect = _newrect = newrect = nullptr;
  nval[0] = nval[1] = nval[2] = nval[3] = 0.0f;
  nvalf[0] = nvalf[1] = nvalf[2] = nvalf[3] = 0.0f;

  if (!do_rect && !do_float) {
    return ibuf;
  }

  if (do_rect) {
    _newrect = static_cast<uchar *>(MEM_mallocN(sizeof(uchar[4]) * newy * ibuf->x, "scaledowny"));
    if (_newrect == nullptr) {
      return ibuf;
    }
  }
  if (do_float) {
    _newrectf = static_cast<float *>(
        MEM_mallocN(sizeof(float[4]) * newy * ibuf->x, "scaledownyf"));
    if (_newrectf == nullptr) {
      if (_newrect) {
        MEM_freeN(_newrect);
      }
      return ibuf;
    }
  }

  add = (ibuf->y - 0.01) / newy;
  skipx = 4 * ibuf->x;

  for (x = skipx - 4; x >= 0; x -= 4) {
    if (do_rect) {
      rect = ibuf->byte_buffer.data + x;
      newrect = _newrect + x;
    }
    if (do_float) {
      rectf = ibuf->float_buffer.data + x;
      newrectf = _newrectf + x;
    }

    sample = 0.0f;
    val[0] = val[1] = val[2] = val[3] = 0.0f;
    valf[0] = valf[1] = valf[2] = valf[3] = 0.0f;

    for (y = newy; y > 0; y--) {
      if (do_rect) {
        nval[0] = -val[0] * sample;
        nval[1] = -val[1] * sample;
        nval[2] = -val[2] * sample;
        nval[3] = -val[3] * sample;
      }
      if (do_float) {
        nvalf[0] = -valf[0] * sample;
        nvalf[1] = -valf[1] * sample;
        nvalf[2] = -valf[2] * sample;
        nvalf[3] = -valf[3] * sample;
      }

      sample += add;

      while (sample >= 1.0f) {
        sample -= 1.0f;

        if (do_rect) {
          nval[0] += rect[0];
          nval[1] += rect[1];
          nval[2] += rect[2];
          nval[3] += rect[3];
          rect += skipx;
        }
        if (do_float) {
          nvalf[0] += rectf[0];
          nvalf[1] += rectf[1];
          nvalf[2] += rectf[2];
          nvalf[3] += rectf[3];
          rectf += skipx;
        }
      }

      if (do_rect) {
        val[0] = rect[0];
        val[1] = rect[1];
        val[2] = rect[2];
        val[3] = rect[3];
        rect += skipx;

        newrect[0] = roundf((nval[0] + sample * val[0]) / add);
        newrect[1] = roundf((nval[1] + sample * val[1]) / add);
        newrect[2] = roundf((nval[2] + sample * val[2]) / add);
        newrect[3] = roundf((nval[3] + sample * val[3]) / add);

        newrect += skipx;
      }
      if (do_float) {

        valf[0] = rectf[0];
        valf[1] = rectf[1];
        valf[2] = rectf[2];
        valf[3] = rectf[3];
        rectf += skipx;

        newrectf[0] = ((nvalf[0] + sample * valf[0]) / add);
        newrectf[1] = ((nvalf[1] + sample * valf[1]) / add);
        newrectf[2] = ((nvalf[2] + sample * valf[2]) / add);
        newrectf[3] = ((nvalf[3] + sample * valf[3]) / add);

        newrectf += skipx;
      }

      sample -= 1.0f;
    }
  }

  if (do_rect) {
    // printf("%ld %ld\n", (uchar *)rect - byte_buffer.data, rect_size);
    BLI_assert((uchar *)rect - ibuf->byte_buffer.data == rect_size); /* see bug #26502. */

    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, _newrect, IB_TAKE_OWNERSHIP);
  }
  if (do_float) {
    // printf("%ld %ld\n", rectf - ibuf->float_buffer.data, rect_size);
    BLI_assert((rectf - ibuf->float_buffer.data) == rect_size); /* see bug #26502. */

    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, _newrectf, IB_TAKE_OWNERSHIP);
  }

  (void)rect_size; /* UNUSED in release builds */

  ibuf->y = newy;
  return ibuf;
}

static ImBuf *scaleupx(ImBuf *ibuf, int newx)
{
  uchar *rect, *_newrect = nullptr, *newrect;
  float *rectf, *_newrectf = nullptr, *newrectf;
  int x, y;
  bool do_rect = false, do_float = false;

  if (ibuf == nullptr) {
    return nullptr;
  }
  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    return ibuf;
  }

  if (ibuf->byte_buffer.data) {
    do_rect = true;
    _newrect = static_cast<uchar *>(MEM_mallocN(newx * ibuf->y * sizeof(int), "scaleupx"));
    if (_newrect == nullptr) {
      return ibuf;
    }
  }
  if (ibuf->float_buffer.data) {
    do_float = true;
    _newrectf = static_cast<float *>(MEM_mallocN(sizeof(float[4]) * newx * ibuf->y, "scaleupxf"));
    if (_newrectf == nullptr) {
      if (_newrect) {
        MEM_freeN(_newrect);
      }
      return ibuf;
    }
  }

  rect = ibuf->byte_buffer.data;
  rectf = ibuf->float_buffer.data;
  newrect = _newrect;
  newrectf = _newrectf;

  /* Special case, copy all columns, needed since the scaling logic assumes there is at least
   * two rows to interpolate between causing out of bounds read for 1px images, see #70356. */
  if (UNLIKELY(ibuf->x == 1)) {
    if (do_rect) {
      for (y = ibuf->y; y > 0; y--) {
        for (x = newx; x > 0; x--) {
          memcpy(newrect, rect, sizeof(char[4]));
          newrect += 4;
        }
        rect += 4;
      }
    }
    if (do_float) {
      for (y = ibuf->y; y > 0; y--) {
        for (x = newx; x > 0; x--) {
          memcpy(newrectf, rectf, sizeof(float[4]));
          newrectf += 4;
        }
        rectf += 4;
      }
    }
  }
  else {
    const float add = (ibuf->x - 1.001) / (newx - 1.0);
    float sample;

    float val_a, nval_a, diff_a;
    float val_b, nval_b, diff_b;
    float val_g, nval_g, diff_g;
    float val_r, nval_r, diff_r;
    float val_af, nval_af, diff_af;
    float val_bf, nval_bf, diff_bf;
    float val_gf, nval_gf, diff_gf;
    float val_rf, nval_rf, diff_rf;

    val_a = nval_a = diff_a = val_b = nval_b = diff_b = 0;
    val_g = nval_g = diff_g = val_r = nval_r = diff_r = 0;
    val_af = nval_af = diff_af = val_bf = nval_bf = diff_bf = 0;
    val_gf = nval_gf = diff_gf = val_rf = nval_rf = diff_rf = 0;

    for (y = ibuf->y; y > 0; y--) {

      sample = 0;

      if (do_rect) {
        val_a = rect[0];
        nval_a = rect[4];
        diff_a = nval_a - val_a;
        val_a += 0.5f;

        val_b = rect[1];
        nval_b = rect[5];
        diff_b = nval_b - val_b;
        val_b += 0.5f;

        val_g = rect[2];
        nval_g = rect[6];
        diff_g = nval_g - val_g;
        val_g += 0.5f;

        val_r = rect[3];
        nval_r = rect[7];
        diff_r = nval_r - val_r;
        val_r += 0.5f;

        rect += 8;
      }
      if (do_float) {
        val_af = rectf[0];
        nval_af = rectf[4];
        diff_af = nval_af - val_af;

        val_bf = rectf[1];
        nval_bf = rectf[5];
        diff_bf = nval_bf - val_bf;

        val_gf = rectf[2];
        nval_gf = rectf[6];
        diff_gf = nval_gf - val_gf;

        val_rf = rectf[3];
        nval_rf = rectf[7];
        diff_rf = nval_rf - val_rf;

        rectf += 8;
      }
      for (x = newx; x > 0; x--) {
        if (sample >= 1.0f) {
          sample -= 1.0f;

          if (do_rect) {
            val_a = nval_a;
            nval_a = rect[0];
            diff_a = nval_a - val_a;
            val_a += 0.5f;

            val_b = nval_b;
            nval_b = rect[1];
            diff_b = nval_b - val_b;
            val_b += 0.5f;

            val_g = nval_g;
            nval_g = rect[2];
            diff_g = nval_g - val_g;
            val_g += 0.5f;

            val_r = nval_r;
            nval_r = rect[3];
            diff_r = nval_r - val_r;
            val_r += 0.5f;
            rect += 4;
          }
          if (do_float) {
            val_af = nval_af;
            nval_af = rectf[0];
            diff_af = nval_af - val_af;

            val_bf = nval_bf;
            nval_bf = rectf[1];
            diff_bf = nval_bf - val_bf;

            val_gf = nval_gf;
            nval_gf = rectf[2];
            diff_gf = nval_gf - val_gf;

            val_rf = nval_rf;
            nval_rf = rectf[3];
            diff_rf = nval_rf - val_rf;
            rectf += 4;
          }
        }
        if (do_rect) {
          newrect[0] = val_a + sample * diff_a;
          newrect[1] = val_b + sample * diff_b;
          newrect[2] = val_g + sample * diff_g;
          newrect[3] = val_r + sample * diff_r;
          newrect += 4;
        }
        if (do_float) {
          newrectf[0] = val_af + sample * diff_af;
          newrectf[1] = val_bf + sample * diff_bf;
          newrectf[2] = val_gf + sample * diff_gf;
          newrectf[3] = val_rf + sample * diff_rf;
          newrectf += 4;
        }
        sample += add;
      }
    }
  }

  if (do_rect) {
    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, _newrect, IB_TAKE_OWNERSHIP);
  }
  if (do_float) {
    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, _newrectf, IB_TAKE_OWNERSHIP);
  }

  ibuf->x = newx;
  return ibuf;
}

static ImBuf *scaleupy(ImBuf *ibuf, int newy)
{
  uchar *rect, *_newrect = nullptr, *newrect;
  float *rectf, *_newrectf = nullptr, *newrectf;
  int x, y, skipx;
  bool do_rect = false, do_float = false;

  if (ibuf == nullptr) {
    return nullptr;
  }
  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    return ibuf;
  }

  if (ibuf->byte_buffer.data) {
    do_rect = true;
    _newrect = static_cast<uchar *>(MEM_mallocN(ibuf->x * newy * sizeof(int), "scaleupy"));
    if (_newrect == nullptr) {
      return ibuf;
    }
  }
  if (ibuf->float_buffer.data) {
    do_float = true;
    _newrectf = static_cast<float *>(MEM_mallocN(sizeof(float[4]) * ibuf->x * newy, "scaleupyf"));
    if (_newrectf == nullptr) {
      if (_newrect) {
        MEM_freeN(_newrect);
      }
      return ibuf;
    }
  }

  rect = ibuf->byte_buffer.data;
  rectf = ibuf->float_buffer.data;
  newrect = _newrect;
  newrectf = _newrectf;

  skipx = 4 * ibuf->x;

  /* Special case, copy all rows, needed since the scaling logic assumes there is at least
   * two rows to interpolate between causing out of bounds read for 1px images, see #70356. */
  if (UNLIKELY(ibuf->y == 1)) {
    if (do_rect) {
      for (y = newy; y > 0; y--) {
        memcpy(newrect, rect, sizeof(char) * skipx);
        newrect += skipx;
      }
    }
    if (do_float) {
      for (y = newy; y > 0; y--) {
        memcpy(newrectf, rectf, sizeof(float) * skipx);
        newrectf += skipx;
      }
    }
  }
  else {
    const float add = (ibuf->y - 1.001) / (newy - 1.0);
    float sample;

    float val_a, nval_a, diff_a;
    float val_b, nval_b, diff_b;
    float val_g, nval_g, diff_g;
    float val_r, nval_r, diff_r;
    float val_af, nval_af, diff_af;
    float val_bf, nval_bf, diff_bf;
    float val_gf, nval_gf, diff_gf;
    float val_rf, nval_rf, diff_rf;

    val_a = nval_a = diff_a = val_b = nval_b = diff_b = 0;
    val_g = nval_g = diff_g = val_r = nval_r = diff_r = 0;
    val_af = nval_af = diff_af = val_bf = nval_bf = diff_bf = 0;
    val_gf = nval_gf = diff_gf = val_rf = nval_rf = diff_rf = 0;

    for (x = ibuf->x; x > 0; x--) {
      sample = 0;
      if (do_rect) {
        rect = ibuf->byte_buffer.data + 4 * (x - 1);
        newrect = _newrect + 4 * (x - 1);

        val_a = rect[0];
        nval_a = rect[skipx];
        diff_a = nval_a - val_a;
        val_a += 0.5f;

        val_b = rect[1];
        nval_b = rect[skipx + 1];
        diff_b = nval_b - val_b;
        val_b += 0.5f;

        val_g = rect[2];
        nval_g = rect[skipx + 2];
        diff_g = nval_g - val_g;
        val_g += 0.5f;

        val_r = rect[3];
        nval_r = rect[skipx + 3];
        diff_r = nval_r - val_r;
        val_r += 0.5f;

        rect += 2 * skipx;
      }
      if (do_float) {
        rectf = ibuf->float_buffer.data + 4 * (x - 1);
        newrectf = _newrectf + 4 * (x - 1);

        val_af = rectf[0];
        nval_af = rectf[skipx];
        diff_af = nval_af - val_af;

        val_bf = rectf[1];
        nval_bf = rectf[skipx + 1];
        diff_bf = nval_bf - val_bf;

        val_gf = rectf[2];
        nval_gf = rectf[skipx + 2];
        diff_gf = nval_gf - val_gf;

        val_rf = rectf[3];
        nval_rf = rectf[skipx + 3];
        diff_rf = nval_rf - val_rf;

        rectf += 2 * skipx;
      }

      for (y = newy; y > 0; y--) {
        if (sample >= 1.0f) {
          sample -= 1.0f;

          if (do_rect) {
            val_a = nval_a;
            nval_a = rect[0];
            diff_a = nval_a - val_a;
            val_a += 0.5f;

            val_b = nval_b;
            nval_b = rect[1];
            diff_b = nval_b - val_b;
            val_b += 0.5f;

            val_g = nval_g;
            nval_g = rect[2];
            diff_g = nval_g - val_g;
            val_g += 0.5f;

            val_r = nval_r;
            nval_r = rect[3];
            diff_r = nval_r - val_r;
            val_r += 0.5f;
            rect += skipx;
          }
          if (do_float) {
            val_af = nval_af;
            nval_af = rectf[0];
            diff_af = nval_af - val_af;

            val_bf = nval_bf;
            nval_bf = rectf[1];
            diff_bf = nval_bf - val_bf;

            val_gf = nval_gf;
            nval_gf = rectf[2];
            diff_gf = nval_gf - val_gf;

            val_rf = nval_rf;
            nval_rf = rectf[3];
            diff_rf = nval_rf - val_rf;
            rectf += skipx;
          }
        }
        if (do_rect) {
          newrect[0] = val_a + sample * diff_a;
          newrect[1] = val_b + sample * diff_b;
          newrect[2] = val_g + sample * diff_g;
          newrect[3] = val_r + sample * diff_r;
          newrect += skipx;
        }
        if (do_float) {
          newrectf[0] = val_af + sample * diff_af;
          newrectf[1] = val_bf + sample * diff_bf;
          newrectf[2] = val_gf + sample * diff_gf;
          newrectf[3] = val_rf + sample * diff_rf;
          newrectf += skipx;
        }
        sample += add;
      }
    }
  }

  if (do_rect) {
    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, _newrect, IB_TAKE_OWNERSHIP);
  }
  if (do_float) {
    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, _newrectf, IB_TAKE_OWNERSHIP);
  }

  ibuf->y = newy;
  return ibuf;
}

bool IMB_scaleImBuf(ImBuf *ibuf, uint newx, uint newy)
{
  BLI_assert_msg(newx > 0 && newy > 0, "Images must be at least 1 on both dimensions!");

  if (ibuf == nullptr) {
    return false;
  }
  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    return false;
  }

  if (newx == ibuf->x && newy == ibuf->y) {
    return false;
  }

  /* try to scale common cases in a fast way */
  /* disabled, quality loss is unacceptable, see report #18609  (ton) */
  if (0 && q_scale_linear_interpolation(ibuf, newx, newy)) {
    return true;
  }

  if (newx && (newx < ibuf->x)) {
    scaledownx(ibuf, newx);
  }
  if (newy && (newy < ibuf->y)) {
    scaledowny(ibuf, newy);
  }
  if (newx && (newx > ibuf->x)) {
    scaleupx(ibuf, newx);
  }
  if (newy && (newy > ibuf->y)) {
    scaleupy(ibuf, newy);
  }

  return true;
}

struct imbufRGBA {
  float r, g, b, a;
};

bool IMB_scalefastImBuf(ImBuf *ibuf, uint newx, uint newy)
{
  BLI_assert_msg(newx > 0 && newy > 0, "Images must be at least 1 on both dimensions!");

  uint *rect, *_newrect, *newrect;
  imbufRGBA *rectf, *_newrectf, *newrectf;
  int x, y;
  bool do_float = false, do_rect = false;
  size_t ofsx, ofsy, stepx, stepy;

  rect = nullptr;
  _newrect = nullptr;
  newrect = nullptr;
  rectf = nullptr;
  _newrectf = nullptr;
  newrectf = nullptr;

  if (ibuf == nullptr) {
    return false;
  }
  if (ibuf->byte_buffer.data) {
    do_rect = true;
  }
  if (ibuf->float_buffer.data) {
    do_float = true;
  }
  if (do_rect == false && do_float == false) {
    return false;
  }

  if (newx == ibuf->x && newy == ibuf->y) {
    return false;
  }

  if (do_rect) {
    _newrect = static_cast<uint *>(MEM_mallocN(newx * newy * sizeof(int), "scalefastimbuf"));
    if (_newrect == nullptr) {
      return false;
    }
    newrect = _newrect;
  }

  if (do_float) {
    _newrectf = static_cast<imbufRGBA *>(
        MEM_mallocN(sizeof(float[4]) * newx * newy, "scalefastimbuf f"));
    if (_newrectf == nullptr) {
      if (_newrect) {
        MEM_freeN(_newrect);
      }
      return false;
    }
    newrectf = _newrectf;
  }

  stepx = round(65536.0 * (ibuf->x - 1.0) / (newx - 1.0));
  stepy = round(65536.0 * (ibuf->y - 1.0) / (newy - 1.0));
  ofsy = 32768;

  for (y = newy; y > 0; y--, ofsy += stepy) {
    if (do_rect) {
      rect = (uint *)ibuf->byte_buffer.data;
      rect += (ofsy >> 16) * ibuf->x;
      ofsx = 32768;

      for (x = newx; x > 0; x--, ofsx += stepx) {
        *newrect++ = rect[ofsx >> 16];
      }
    }

    if (do_float) {
      rectf = (imbufRGBA *)ibuf->float_buffer.data;
      rectf += (ofsy >> 16) * ibuf->x;
      ofsx = 32768;

      for (x = newx; x > 0; x--, ofsx += stepx) {
        *newrectf++ = rectf[ofsx >> 16];
      }
    }
  }

  if (do_rect) {
    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, reinterpret_cast<uint8_t *>(_newrect), IB_TAKE_OWNERSHIP);
  }

  if (do_float) {
    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, reinterpret_cast<float *>(_newrectf), IB_TAKE_OWNERSHIP);
  }

  ibuf->x = newx;
  ibuf->y = newy;
  return true;
}

/* ******** threaded scaling ******** */

struct ScaleTreadInitData {
  ImBuf *ibuf;

  uint newx;
  uint newy;

  uchar *byte_buffer;
  float *float_buffer;
};

struct ScaleThreadData {
  ImBuf *ibuf;

  uint newx;
  uint newy;

  int start_line;
  int tot_line;

  uchar *byte_buffer;
  float *float_buffer;
};

static void scale_thread_init(void *data_v, int start_line, int tot_line, void *init_data_v)
{
  ScaleThreadData *data = (ScaleThreadData *)data_v;
  ScaleTreadInitData *init_data = (ScaleTreadInitData *)init_data_v;

  data->ibuf = init_data->ibuf;

  data->newx = init_data->newx;
  data->newy = init_data->newy;

  data->start_line = start_line;
  data->tot_line = tot_line;

  data->byte_buffer = init_data->byte_buffer;
  data->float_buffer = init_data->float_buffer;
}

static void *do_scale_thread(void *data_v)
{
  ScaleThreadData *data = (ScaleThreadData *)data_v;
  ImBuf *ibuf = data->ibuf;
  int i;
  float factor_x = float(ibuf->x) / data->newx;
  float factor_y = float(ibuf->y) / data->newy;

  for (i = 0; i < data->tot_line; i++) {
    int y = data->start_line + i;
    int x;

    for (x = 0; x < data->newx; x++) {
      float u = float(x) * factor_x;
      float v = float(y) * factor_y;
      int offset = y * data->newx + x;

      if (data->byte_buffer) {
        uchar *pixel = data->byte_buffer + 4 * offset;
        BLI_bilinear_interpolation_char(ibuf->byte_buffer.data, pixel, ibuf->x, ibuf->y, 4, u, v);
      }

      if (data->float_buffer) {
        float *pixel = data->float_buffer + ibuf->channels * offset;
        BLI_bilinear_interpolation_fl(
            ibuf->float_buffer.data, pixel, ibuf->x, ibuf->y, ibuf->channels, u, v);
      }
    }
  }

  return nullptr;
}

void IMB_scaleImBuf_threaded(ImBuf *ibuf, uint newx, uint newy)
{
  BLI_assert_msg(newx > 0 && newy > 0, "Images must be at least 1 on both dimensions!");

  ScaleTreadInitData init_data = {nullptr};

  /* prepare initialization data */
  init_data.ibuf = ibuf;

  init_data.newx = newx;
  init_data.newy = newy;

  if (ibuf->byte_buffer.data) {
    init_data.byte_buffer = static_cast<uchar *>(
        MEM_mallocN(4 * newx * newy * sizeof(char), "threaded scale byte buffer"));
  }

  if (ibuf->float_buffer.data) {
    init_data.float_buffer = static_cast<float *>(
        MEM_mallocN(ibuf->channels * newx * newy * sizeof(float), "threaded scale float buffer"));
  }

  /* actual scaling threads */
  IMB_processor_apply_threaded(
      newy, sizeof(ScaleThreadData), &init_data, scale_thread_init, do_scale_thread);

  /* alter image buffer */
  ibuf->x = newx;
  ibuf->y = newy;

  if (ibuf->byte_buffer.data) {
    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, init_data.byte_buffer, IB_TAKE_OWNERSHIP);
  }

  if (ibuf->float_buffer.data) {
    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, init_data.float_buffer, IB_TAKE_OWNERSHIP);
  }
}
