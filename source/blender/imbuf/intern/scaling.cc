/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cmath>

#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "IMB_filter.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_interp.hh"
#include "IMB_metadata.hh"

#include "BLI_sys_types.h" /* for intptr_t support */

using blender::float2;
using blender::float3;
using blender::float4;
using blender::uchar4;

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

static void alloc_scale_dst_buffers(
    const ImBuf *ibuf, uint newx, uint newy, uchar4 **r_dst_byte, float **r_dst_float)
{
  *r_dst_byte = nullptr;
  if (ibuf->byte_buffer.data != nullptr) {
    *r_dst_byte = static_cast<uchar4 *>(
        MEM_mallocN(sizeof(uchar4) * newx * newy, "scale_buf_byte"));
    if (*r_dst_byte == nullptr) {
      return;
    }
  }
  *r_dst_float = nullptr;
  if (ibuf->float_buffer.data != nullptr) {
    *r_dst_float = static_cast<float *>(
        MEM_mallocN(sizeof(float) * ibuf->channels * newx * newy, "scale_buf_float"));
    if (*r_dst_float == nullptr) {
      if (*r_dst_byte) {
        MEM_freeN(*r_dst_byte);
      }
      return;
    }
  }
}

static inline float4 load_pixel(const uchar4 *ptr)
{
  return float4(ptr[0]);
}
static inline float4 load_pixel(const float *ptr)
{
  return float4(ptr[0]);
}
static inline float4 load_pixel(const float2 *ptr)
{
  return float4(ptr[0]);
}
static inline float4 load_pixel(const float3 *ptr)
{
  return float4(ptr[0]);
}
static inline float4 load_pixel(const float4 *ptr)
{
  return float4(ptr[0]);
}
static inline void store_pixel(float4 pix, uchar4 *ptr)
{
  *ptr = uchar4(blender::math::round(pix));
}
static inline void store_pixel(float4 pix, float *ptr)
{
  *ptr = pix.x;
}
static inline void store_pixel(float4 pix, float2 *ptr)
{
  memcpy(ptr, &pix, sizeof(*ptr));
}
static inline void store_pixel(float4 pix, float3 *ptr)
{
  memcpy(ptr, &pix, sizeof(*ptr));
}
static inline void store_pixel(float4 pix, float4 *ptr)
{
  *ptr = pix;
}

struct ScaleDownX {
  template<typename T>
  static void op(const T *src, T *dst, int ibufx, int ibufy, int newx, int /*newy*/, bool threaded)
  {
    using namespace blender;
    const float add = (ibufx - 0.01f) / newx;
    const float inv_add = 1.0f / add;

    const int grain_size = threaded ? 32 : ibufy;
    threading::parallel_for(IndexRange(ibufy), grain_size, [&](IndexRange range) {
      for (const int y : range) {
        const T *src_ptr = src + y * ibufx;
        T *dst_ptr = dst + y * newx;
        float sample = 0.0f;
        float4 val(0.0f);

        for (int x = 0; x < newx; x++) {
          float4 nval = -val * sample;
          sample += add;
          while (sample >= 1.0f) {
            sample -= 1.0f;
            nval += load_pixel(src_ptr);
            src_ptr++;
          }

          val = load_pixel(src_ptr);
          src_ptr++;

          float4 pix = (nval + sample * val) * inv_add;
          store_pixel(pix, dst_ptr);
          dst_ptr++;

          sample -= 1.0f;
        }
      }
    });
  }
};

struct ScaleDownY {
  template<typename T>
  static void op(const T *src, T *dst, int ibufx, int ibufy, int /*newx*/, int newy, bool threaded)
  {
    using namespace blender;
    const float add = (ibufy - 0.01f) / newy;
    const float inv_add = 1.0f / add;

    const int grain_size = threaded ? 32 : ibufx;
    threading::parallel_for(IndexRange(ibufx), grain_size, [&](IndexRange range) {
      for (const int x : range) {
        const T *src_ptr = src + x;
        T *dst_ptr = dst + x;
        float sample = 0.0f;
        float4 val(0.0f);

        for (int y = 0; y < newy; y++) {
          float4 nval = -val * sample;
          sample += add;
          while (sample >= 1.0f) {
            sample -= 1.0f;
            nval += load_pixel(src_ptr);
            src_ptr += ibufx;
          }

          val = load_pixel(src_ptr);
          src_ptr += ibufx;

          float4 pix = (nval + sample * val) * inv_add;
          store_pixel(pix, dst_ptr);
          dst_ptr += ibufx;

          sample -= 1.0f;
        }
      }
    });
  }
};

struct ScaleUpX {
  template<typename T>
  static void op(const T *src, T *dst, int ibufx, int ibufy, int newx, int /*newy*/, bool threaded)
  {
    using namespace blender;
    const float add = (ibufx - 0.001f) / newx;
    /* Special case: source is 1px wide (see #70356). */
    if (UNLIKELY(ibufx == 1)) {
      for (int y = ibufy; y > 0; y--) {
        for (int x = newx; x > 0; x--) {
          *dst = *src;
          dst++;
        }
        src++;
      }
    }
    else {
      const int grain_size = threaded ? 32 : ibufy;
      threading::parallel_for(IndexRange(ibufy), grain_size, [&](IndexRange range) {
        for (const int y : range) {
          float sample = -0.5f + add * 0.5f;
          int counter = 0;
          const T *src_ptr = src + y * ibufx;
          T *dst_ptr = dst + y * newx;
          float4 val = load_pixel(src_ptr);
          float4 nval = load_pixel(src_ptr + 1);
          float4 diff = nval - val;
          if (ibufx > 2) {
            src_ptr += 2;
            counter += 2;
          }
          for (int x = 0; x < newx; x++) {
            if (sample >= 1.0f) {
              sample -= 1.0f;
              val = nval;
              nval = load_pixel(src_ptr);
              diff = nval - val;
              if (counter + 1 < ibufx) {
                src_ptr++;
                counter++;
              }
            }
            float4 pix = val + blender::math::max(sample, 0.0f) * diff;
            store_pixel(pix, dst_ptr);
            dst_ptr++;
            sample += add;
          }
        }
      });
    }
  }
};

struct ScaleUpY {
  template<typename T>
  static void op(const T *src, T *dst, int ibufx, int ibufy, int /*newx*/, int newy, bool threaded)
  {
    using namespace blender;
    const float add = (ibufy - 0.001f) / newy;
    /* Special case: source is 1px high (see #70356). */
    if (UNLIKELY(ibufy == 1)) {
      for (int y = newy; y > 0; y--) {
        memcpy(dst, src, sizeof(T) * ibufx);
        dst += ibufx;
      }
    }
    else {
      const int grain_size = threaded ? 32 : ibufx;
      threading::parallel_for(IndexRange(ibufx), grain_size, [&](IndexRange range) {
        for (const int x : range) {
          float sample = -0.5f + add * 0.5f;
          int counter = 0;
          const T *src_ptr = src + x;
          T *dst_ptr = dst + x;

          float4 val = load_pixel(src_ptr);
          float4 nval = load_pixel(src_ptr + ibufx);
          float4 diff = nval - val;
          if (ibufy > 2) {
            src_ptr += ibufx * 2;
            counter += 2;
          }

          for (int y = 0; y < newy; y++) {
            if (sample >= 1.0f) {
              sample -= 1.0f;
              val = nval;
              nval = load_pixel(src_ptr);
              diff = nval - val;
              if (counter + 1 < ibufy) {
                src_ptr += ibufx;
                ++counter;
              }
            }
            float4 pix = val + blender::math::max(sample, 0.0f) * diff;
            store_pixel(pix, dst_ptr);
            dst_ptr += ibufx;
            sample += add;
          }
        }
      });
    }
  }
};

template<typename T>
static void instantiate_pixel_op(T & /*op*/,
                                 const ImBuf *ibuf,
                                 int newx,
                                 int newy,
                                 uchar4 *dst_byte,
                                 float *dst_float,
                                 bool threaded)
{
  if (dst_byte != nullptr) {
    const uchar4 *src = (const uchar4 *)ibuf->byte_buffer.data;
    T::op(src, dst_byte, ibuf->x, ibuf->y, newx, newy, threaded);
  }
  if (dst_float != nullptr) {
    if (ibuf->channels == 1) {
      T::op(ibuf->float_buffer.data, dst_float, ibuf->x, ibuf->y, newx, newy, threaded);
    }
    else if (ibuf->channels == 2) {
      const float2 *src = (const float2 *)ibuf->float_buffer.data;
      T::op(src, (float2 *)dst_float, ibuf->x, ibuf->y, newx, newy, threaded);
    }
    else if (ibuf->channels == 3) {
      const float3 *src = (const float3 *)ibuf->float_buffer.data;
      T::op(src, (float3 *)dst_float, ibuf->x, ibuf->y, newx, newy, threaded);
    }
    else if (ibuf->channels == 4) {
      const float4 *src = (const float4 *)ibuf->float_buffer.data;
      T::op(src, (float4 *)dst_float, ibuf->x, ibuf->y, newx, newy, threaded);
    }
  }
}

static void scale_down_x_func(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded)
{
  ScaleDownX op;
  instantiate_pixel_op(op, ibuf, newx, newy, dst_byte, dst_float, threaded);
}

static void scale_down_y_func(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded)
{
  ScaleDownY op;
  instantiate_pixel_op(op, ibuf, newx, newy, dst_byte, dst_float, threaded);
}

static void scale_up_x_func(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded)
{
  ScaleUpX op;
  instantiate_pixel_op(op, ibuf, newx, newy, dst_byte, dst_float, threaded);
}

static void scale_up_y_func(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded)
{
  ScaleUpY op;
  instantiate_pixel_op(op, ibuf, newx, newy, dst_byte, dst_float, threaded);
}

using ScaleFunction = void (*)(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded);

static void scale_with_function(ImBuf *ibuf, int newx, int newy, ScaleFunction func, bool threaded)
{
  /* Allocate destination buffers. */
  uchar4 *dst_byte = nullptr;
  float *dst_float = nullptr;
  alloc_scale_dst_buffers(ibuf, newx, newy, &dst_byte, &dst_float);
  if (dst_byte == nullptr && dst_float == nullptr) {
    return;
  }

  /* Do actual processing. */
  func(ibuf, newx, newy, dst_byte, dst_float, threaded);

  /* Modify image to point to new destination. */
  if (dst_byte != nullptr) {
    imb_freerectImBuf(ibuf);
    IMB_assign_byte_buffer(ibuf, reinterpret_cast<uint8_t *>(dst_byte), IB_TAKE_OWNERSHIP);
  }
  if (dst_float != nullptr) {
    imb_freerectfloatImBuf(ibuf);
    IMB_assign_float_buffer(ibuf, dst_float, IB_TAKE_OWNERSHIP);
  }
  ibuf->x = newx;
  ibuf->y = newy;
}

static void imb_scale_box(ImBuf *ibuf, uint newx, uint newy, bool threaded)
{
  if (newx != 0 && (newx < ibuf->x)) {
    scale_with_function(ibuf, newx, ibuf->y, scale_down_x_func, threaded);
  }
  if (newy != 0 && (newy < ibuf->y)) {
    scale_with_function(ibuf, ibuf->x, newy, scale_down_y_func, threaded);
  }
  if (newx != 0 && (newx > ibuf->x)) {
    scale_with_function(ibuf, newx, ibuf->y, scale_up_x_func, threaded);
  }
  if (newy != 0 && (newy > ibuf->y)) {
    scale_with_function(ibuf, ibuf->x, newy, scale_up_y_func, threaded);
  }
}

template<typename T>
static void scale_nearest(
    const T *src, T *dst, int ibufx, int ibufy, int newx, int newy, blender::IndexRange y_range)
{
  /* Nearest sample scaling. Step through pixels in fixed point coordinates. */
  constexpr int FRAC_BITS = 16;
  int64_t stepx = ((int64_t(ibufx) << FRAC_BITS) + newx / 2) / newx;
  int64_t stepy = ((int64_t(ibufy) << FRAC_BITS) + newy / 2) / newy;
  int64_t posy = y_range.first() * stepy;
  dst += y_range.first() * newx;
  for (const int y : y_range) {
    UNUSED_VARS(y);
    const T *row = src + (posy >> FRAC_BITS) * ibufx;
    int64_t posx = 0;
    for (int x = 0; x < newx; x++, posx += stepx) {
      *dst = row[posx >> FRAC_BITS];
      dst++;
    }
    posy += stepy;
  }
}

static void scale_nearest_func(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded)
{
  using namespace blender;

  const int grain_size = threaded ? 64 : newy;
  threading::parallel_for(IndexRange(newy), grain_size, [&](IndexRange y_range) {
    /* Byte pixels. */
    if (dst_byte != nullptr) {
      const uchar4 *src = (const uchar4 *)ibuf->byte_buffer.data;
      scale_nearest(src, dst_byte, ibuf->x, ibuf->y, newx, newy, y_range);
    }
    /* Float pixels. */
    if (dst_float != nullptr) {
      if (ibuf->channels == 1) {
        scale_nearest(ibuf->float_buffer.data, dst_float, ibuf->x, ibuf->y, newx, newy, y_range);
      }
      else if (ibuf->channels == 2) {
        const float2 *src = (const float2 *)ibuf->float_buffer.data;
        scale_nearest(src, (float2 *)dst_float, ibuf->x, ibuf->y, newx, newy, y_range);
      }
      else if (ibuf->channels == 3) {
        const float3 *src = (const float3 *)ibuf->float_buffer.data;
        scale_nearest(src, (float3 *)dst_float, ibuf->x, ibuf->y, newx, newy, y_range);
      }
      else if (ibuf->channels == 4) {
        const float4 *src = (const float4 *)ibuf->float_buffer.data;
        scale_nearest(src, (float4 *)dst_float, ibuf->x, ibuf->y, newx, newy, y_range);
      }
    }
  });
}

static void scale_bilinear_func(
    const ImBuf *ibuf, int newx, int newy, uchar4 *dst_byte, float *dst_float, bool threaded)
{
  using namespace blender;
  using namespace blender::imbuf;

  const int grain_size = threaded ? 32 : newy;
  threading::parallel_for(IndexRange(newy), grain_size, [&](IndexRange y_range) {
    float factor_x = float(ibuf->x) / newx;
    float factor_y = float(ibuf->y) / newy;

    for (const int y : y_range) {
      float v = (float(y) + 0.5f) * factor_y - 0.5f;
      for (int x = 0; x < newx; x++) {
        float u = (float(x) + 0.5f) * factor_x - 0.5f;
        int64_t offset = int64_t(y) * newx + x;
        if (dst_byte) {
          interpolate_bilinear_byte(ibuf, (uchar *)(dst_byte + offset), u, v);
        }
        if (dst_float) {
          float *pixel = dst_float + ibuf->channels * offset;
          math::interpolate_bilinear_fl(
              ibuf->float_buffer.data, pixel, ibuf->x, ibuf->y, ibuf->channels, u, v);
        }
      }
    }
  });
}

bool IMB_scale(ImBuf *ibuf, uint newx, uint newy, IMBScaleFilter filter, bool threaded)
{
  BLI_assert_msg(newx > 0 && newy > 0, "Images must be at least 1 on both dimensions!");
  if (ibuf == nullptr) {
    return false;
  }
  if (newx == ibuf->x && newy == ibuf->y) {
    return false;
  }

  switch (filter) {
    case IMBScaleFilter::Nearest:
      scale_with_function(ibuf, newx, newy, scale_nearest_func, threaded);
      break;
    case IMBScaleFilter::Bilinear:
      scale_with_function(ibuf, newx, newy, scale_bilinear_func, threaded);
      break;
    case IMBScaleFilter::Box:
      imb_scale_box(ibuf, newx, newy, threaded);
      break;
  }
  return true;
}

ImBuf *IMB_scale_into_new(
    const ImBuf *ibuf, unsigned int newx, unsigned int newy, IMBScaleFilter filter, bool threaded)
{
  BLI_assert_msg(newx > 0 && newy > 0, "Images must be at least 1 on both dimensions!");
  if (ibuf == nullptr) {
    return nullptr;
  }
  /* Size same as source: just copy source image. */
  if (newx == ibuf->x && newy == ibuf->y) {
    ImBuf *dst = IMB_dupImBuf(ibuf);
    IMB_metadata_copy(dst, ibuf);
    return dst;
  }

  /* Allocate destination buffers. */
  uchar4 *dst_byte = nullptr;
  float *dst_float = nullptr;
  alloc_scale_dst_buffers(ibuf, newx, newy, &dst_byte, &dst_float);
  if (dst_byte == nullptr && dst_float == nullptr) {
    return nullptr;
  }

  switch (filter) {
    case IMBScaleFilter::Nearest:
      scale_nearest_func(ibuf, newx, newy, dst_byte, dst_float, threaded);
      break;
    case IMBScaleFilter::Bilinear:
      scale_bilinear_func(ibuf, newx, newy, dst_byte, dst_float, threaded);
      break;
    case IMBScaleFilter::Box: {
      /* Horizontal scale. */
      uchar4 *tmp_byte = nullptr;
      float *tmp_float = nullptr;
      alloc_scale_dst_buffers(ibuf, newx, ibuf->y, &tmp_byte, &tmp_float);
      if (tmp_byte == nullptr && tmp_float == nullptr) {
        if (dst_byte != nullptr) {
          MEM_freeN(dst_byte);
        }
        if (dst_byte != nullptr) {
          MEM_freeN(dst_float);
        }
        return nullptr;
      }
      if (newx < ibuf->x) {
        scale_down_x_func(ibuf, newx, ibuf->y, tmp_byte, tmp_float, threaded);
      }
      else {
        scale_up_x_func(ibuf, newx, ibuf->y, tmp_byte, tmp_float, threaded);
      }

      /* Vertical scale. */
      ImBuf tmpbuf;
      IMB_initImBuf(&tmpbuf, newx, ibuf->y, ibuf->planes, 0);
      if (tmp_byte != nullptr) {
        IMB_assign_byte_buffer(
            &tmpbuf, reinterpret_cast<uint8_t *>(tmp_byte), IB_DO_NOT_TAKE_OWNERSHIP);
      }
      if (tmp_float != nullptr) {
        IMB_assign_float_buffer(&tmpbuf, tmp_float, IB_DO_NOT_TAKE_OWNERSHIP);
      }
      if (newy < ibuf->y) {
        scale_down_y_func(&tmpbuf, newx, newy, dst_byte, dst_float, threaded);
      }
      else {
        scale_up_y_func(&tmpbuf, newx, newy, dst_byte, dst_float, threaded);
      }

      if (tmp_byte != nullptr) {
        MEM_freeN(tmp_byte);
      }
      if (tmp_float != nullptr) {
        MEM_freeN(tmp_float);
      }
    } break;
  }

  /* Create result image. */
  ImBuf *dst = IMB_allocImBuf(newx, newy, ibuf->planes, IB_uninitialized_pixels);
  IMB_metadata_copy(dst, ibuf);
  dst->colormanage_flag = ibuf->colormanage_flag;
  if (dst_byte != nullptr) {
    IMB_assign_byte_buffer(dst, reinterpret_cast<uint8_t *>(dst_byte), IB_TAKE_OWNERSHIP);
    dst->byte_buffer.colorspace = ibuf->byte_buffer.colorspace;
  }
  if (dst_float != nullptr) {
    IMB_assign_float_buffer(dst, dst_float, IB_TAKE_OWNERSHIP);
    dst->float_buffer.colorspace = ibuf->float_buffer.colorspace;
  }
  return dst;
}
