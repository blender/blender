/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

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

static void alloc_scale_dst_buffers(
    const ImBuf *ibuf, uint newx, uint newy, uchar4 **r_dst_byte, float **r_dst_float)
{
  *r_dst_byte = nullptr;
  if (ibuf->byte_buffer.data != nullptr) {
    *r_dst_byte = MEM_malloc_arrayN<uchar4>(size_t(newx) * size_t(newy), "scale_buf_byte");
    if (*r_dst_byte == nullptr) {
      return;
    }
  }
  *r_dst_float = nullptr;
  if (ibuf->float_buffer.data != nullptr) {
    *r_dst_float = MEM_malloc_arrayN<float>(size_t(ibuf->channels) * newx * newy,
                                            "scale_buf_float");
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
  return float4(ptr[0], 0.0f, 1.0f);
}
static inline float4 load_pixel(const float3 *ptr)
{
  return float4(ptr[0], 1.0f);
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
  memcpy(reinterpret_cast<void *>(ptr), &pix, sizeof(*ptr));
}
static inline void store_pixel(float4 pix, float3 *ptr)
{
  memcpy(reinterpret_cast<void *>(ptr), &pix, sizeof(*ptr));
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
        const T *src_ptr = src + (int64_t(y) * ibufx);
        T *dst_ptr = dst + (int64_t(y) * newx);
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
          const T *src_ptr = src + (int64_t(y) * ibufx);
          T *dst_ptr = dst + (int64_t(y) * newx);
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
        memcpy(reinterpret_cast<void *>(dst), src, sizeof(T) * ibufx);
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
    IMB_free_byte_pixels(ibuf);
    IMB_assign_byte_buffer(ibuf, reinterpret_cast<uint8_t *>(dst_byte), IB_TAKE_OWNERSHIP);
  }
  if (dst_float != nullptr) {
    IMB_free_float_pixels(ibuf);
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
    const ImBuf *ibuf, uint newx, uint newy, IMBScaleFilter filter, bool threaded)
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
  dst->channels = ibuf->channels;
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
