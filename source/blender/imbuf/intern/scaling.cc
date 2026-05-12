/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_math_interp.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "IMB_filter.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"

#include "BLI_sys_types.h" /* for intptr_t support */

namespace blender {

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
  *ptr = uchar4(math::round(pix));
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

template<typename BufferT, typename Fn>
static void to_static_pixel_type(const BufferT *src_buffer,
                                 const int channels,
                                 BufferT *dst_buffer,
                                 const Fn &fn)
{
  if constexpr (std::is_same_v<BufferT, uchar>) {
    fn(reinterpret_cast<const uchar4 *>(src_buffer), reinterpret_cast<uchar4 *>(dst_buffer));
  }
  else {
    if (channels == 1) {
      fn(src_buffer, dst_buffer);
    }
    else if (channels == 2) {
      const float2 *src = reinterpret_cast<const float2 *>(src_buffer);
      fn(src, reinterpret_cast<float2 *>(dst_buffer));
    }
    else if (channels == 3) {
      const float3 *src = reinterpret_cast<const float3 *>(src_buffer);
      fn(src, reinterpret_cast<float3 *>(dst_buffer));
    }
    else if (channels == 4) {
      const float4 *src = reinterpret_cast<const float4 *>(src_buffer);
      fn(src, reinterpret_cast<float4 *>(dst_buffer));
    }
  }
}

template<typename BufferT>
static void scale_down_x_func(const BufferT *src_buffer,
                              const int2 src_size,
                              const int channels,
                              BufferT *dst_buffer,
                              const int2 dst_size,
                              bool threaded)
{
  const int newx = dst_size.x;
  const int ibufx = src_size.x;
  const int ibufy = src_size.y;
  to_static_pixel_type(src_buffer, channels, dst_buffer, [&]<typename T>(const T *src, T *dst) {
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
  });
}

template<typename BufferT>
static void scale_down_y_func(const BufferT *src_buffer,
                              const int2 src_size,
                              const int channels,
                              BufferT *dst_buffer,
                              const int2 dst_size,
                              bool threaded)
{
  const int newy = dst_size.y;
  const int ibufx = src_size.x;
  const int ibufy = src_size.y;
  to_static_pixel_type(src_buffer, channels, dst_buffer, [&]<typename T>(const T *src, T *dst) {
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
  });
}

template<typename BufferT>
static void scale_up_x_func(const BufferT *src_buffer,
                            const int2 src_size,
                            const int channels,
                            BufferT *dst_buffer,
                            const int2 dst_size,
                            bool threaded)
{
  const int newx = dst_size.x;
  const int ibufx = src_size.x;
  const int ibufy = src_size.y;
  to_static_pixel_type(src_buffer, channels, dst_buffer, [&]<typename T>(const T *src, T *dst) {
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
            float4 pix = val + math::max(sample, 0.0f) * diff;
            store_pixel(pix, dst_ptr);
            dst_ptr++;
            sample += add;
          }
        }
      });
    }
  });
}

template<typename BufferT>
static void scale_up_y_func(const BufferT *src_buffer,
                            const int2 src_size,
                            const int channels,
                            BufferT *dst_buffer,
                            const int2 dst_size,
                            bool threaded)
{
  const int newy = dst_size.y;
  const int ibufx = src_size.x;
  const int ibufy = src_size.y;
  to_static_pixel_type(src_buffer, channels, dst_buffer, [&]<typename T>(const T *src, T *dst) {
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
            float4 pix = val + math::max(sample, 0.0f) * diff;
            store_pixel(pix, dst_ptr);
            dst_ptr += ibufx;
            sample += add;
          }
        }
      });
    }
  });
}

template<typename BufferT>
static void imb_scale_box(const BufferT *src_buffer,
                          const int2 src_size,
                          const int channels,
                          BufferT *dst_buffer,
                          const int2 dst_size,
                          const bool threaded)
{
  BufferT *tmp_buffer = MEM_new_array_uninitialized<BufferT>(
      int64_t(channels) * dst_size.x * src_size.y, __func__);
  if (dst_size.x < src_size.x) {
    scale_down_x_func(
        src_buffer, src_size, channels, tmp_buffer, int2(dst_size.x, src_size.y), threaded);
  }
  else {
    scale_up_x_func(
        src_buffer, src_size, channels, tmp_buffer, int2(dst_size.x, src_size.y), threaded);
  }

  if (dst_size.y < src_size.y) {
    scale_down_y_func(
        tmp_buffer, int2(dst_size.x, src_size.y), channels, dst_buffer, dst_size, threaded);
  }
  else {
    scale_up_y_func(
        tmp_buffer, int2(dst_size.x, src_size.y), channels, dst_buffer, dst_size, threaded);
  }

  MEM_delete(tmp_buffer);
}

template<typename T>
static void scale_nearest(
    const T *src, T *dst, const int2 src_size, const int2 dst_size, IndexRange y_range)
{
  const int ibufx = src_size.x;
  const int ibufy = src_size.y;
  const int newx = dst_size.x;
  const int newy = dst_size.y;
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

template<typename BufferT>
static void scale_nearest_func(const BufferT *src_buffer,
                               const int2 src_size,
                               const int channels,
                               BufferT *dst_buffer,
                               const int2 dst_size,
                               bool threaded)
{
  const int grain_size = threaded ? 64 : dst_size.y;
  threading::parallel_for(IndexRange(dst_size.y), grain_size, [&](IndexRange y_range) {
    if constexpr (std::is_same_v<BufferT, uchar>) {
      const uchar4 *src = reinterpret_cast<const uchar4 *>(src_buffer);
      scale_nearest(src, reinterpret_cast<uchar4 *>(dst_buffer), src_size, dst_size, y_range);
    }
    else {
      if (channels == 1) {
        scale_nearest(src_buffer, dst_buffer, src_size, dst_size, y_range);
      }
      else if (channels == 2) {
        const float2 *src = reinterpret_cast<const float2 *>(src_buffer);
        scale_nearest(src, reinterpret_cast<float2 *>(dst_buffer), src_size, dst_size, y_range);
      }
      else if (channels == 3) {
        const float3 *src = reinterpret_cast<const float3 *>(src_buffer);
        scale_nearest(src, reinterpret_cast<float3 *>(dst_buffer), src_size, dst_size, y_range);
      }
      else if (channels == 4) {
        const float4 *src = reinterpret_cast<const float4 *>(src_buffer);
        scale_nearest(src, reinterpret_cast<float4 *>(dst_buffer), src_size, dst_size, y_range);
      }
    }
  });
}

template<typename BufferT>
static void scale_bilinear(const BufferT *src_buffer,
                           const int2 src_size,
                           const int channels,
                           BufferT *dst_buffer,
                           const int2 dst_size,
                           bool threaded)
{
  const int newx = dst_size.x;
  const int newy = dst_size.y;

  const int grain_size = threaded ? 32 : newy;
  threading::parallel_for(IndexRange(newy), grain_size, [&](IndexRange y_range) {
    float factor_x = float(src_size.x) / newx;
    float factor_y = float(src_size.y) / newy;

    for (const int y : y_range) {
      float v = (float(y) + 0.5f) * factor_y - 0.5f;
      for (int x = 0; x < newx; x++) {
        float u = (float(x) + 0.5f) * factor_x - 0.5f;
        int64_t offset = int64_t(y) * newx + x;
        if constexpr (std::is_same_v<BufferT, uchar>) {
          *reinterpret_cast<uchar4 *>(dst_buffer + offset * 4) = math::interpolate_bilinear_byte(
              src_buffer, src_size.x, src_size.y, u, v);
        }
        else {
          float *pixel = dst_buffer + channels * offset;
          math::interpolate_bilinear_fl(src_buffer, pixel, src_size.x, src_size.y, channels, u, v);
        }
      }
    }
  });
}

bool IMB_scale(ImBuf *ibuf, const int2 new_size, IMBScaleFilter filter, bool threaded)
{
  BLI_assert_msg(new_size.x > 0 && new_size.y > 0,
                 "Images must be at least 1 on both dimensions!");
  if (ibuf == nullptr) {
    return false;
  }
  const int2 src_size = int2(ibuf->x, ibuf->y);
  if (src_size == new_size) {
    return false;
  }

  switch (filter) {
    case IMBScaleFilter::Nearest: {
      if (const float *src = ibuf->float_data()) {
        float *dst = MEM_new_array_uninitialized<float>(
            size_t(ibuf->channels) * new_size.x * new_size.y, __func__);
        scale_nearest_func(src, src_size, ibuf->channels, dst, new_size, threaded);
        IMB_assign_float_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
      }
      if (const uchar *src = ibuf->byte_data()) {
        uchar *dst = MEM_new_array_uninitialized<uchar>(size_t(new_size.x) * new_size.y * 4,
                                                        __func__);
        scale_nearest_func(src, src_size, 4, dst, new_size, threaded);
        IMB_assign_byte_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
      }
      break;
    }
    case IMBScaleFilter::Bilinear: {
      if (const float *src = ibuf->float_data()) {
        float *dst = MEM_new_array_uninitialized<float>(
            size_t(ibuf->channels) * new_size.x * new_size.y, __func__);
        scale_bilinear(src, src_size, ibuf->channels, dst, new_size, threaded);
        IMB_assign_float_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
      }
      if (const uchar *src = ibuf->byte_data()) {
        uchar *dst = MEM_new_array_uninitialized<uchar>(size_t(new_size.x) * new_size.y * 4,
                                                        __func__);
        scale_bilinear(src, src_size, 4, dst, new_size, threaded);
        IMB_assign_byte_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
      }
      break;
    }
    case IMBScaleFilter::Box: {
      if (const float *src = ibuf->float_data()) {
        float *dst = MEM_new_array_uninitialized<float>(
            size_t(ibuf->channels) * new_size.x * new_size.y, __func__);
        imb_scale_box(src, src_size, ibuf->channels, dst, new_size, threaded);
        IMB_assign_float_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
      }
      if (const uchar *src = ibuf->byte_data()) {
        uchar *dst = MEM_new_array_uninitialized<uchar>(size_t(new_size.x) * new_size.y * 4,
                                                        __func__);
        imb_scale_box(src, src_size, 4, dst, new_size, threaded);
        IMB_assign_byte_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
      }
      break;
    }
  }
  ibuf->x = new_size.x;
  ibuf->y = new_size.y;
  return true;
}

ImBuf *IMB_scale_into_new(const ImBuf *ibuf,
                          const int2 new_size,
                          IMBScaleFilter filter,
                          bool threaded)
{
  BLI_assert_msg(new_size.x > 0 && new_size.y > 0,
                 "Images must be at least 1 on both dimensions!");
  if (ibuf == nullptr) {
    return nullptr;
  }
  /* Size same as source: just copy source image. */
  const int2 src_size = int2(ibuf->x, ibuf->y);
  if (src_size == new_size) {
    ImBuf *dst = IMB_dupImBuf(ibuf);
    IMB_metadata_copy(dst, ibuf);
    return dst;
  }

  /* Allocate destination buffers. */
  eImBufFlags flags = IB_uninitialized_pixels;
  if (ibuf->byte_data()) {
    flags |= IB_byte_data;
  }
  if (ibuf->float_data()) {
    flags |= IB_float_data;
  }
  ImBuf *dst = IMB_allocImBuf(new_size.x, new_size.y, ibuf->planes, flags);
  dst->channels = ibuf->channels;
  IMB_metadata_copy(dst, ibuf);
  dst->colormanage_flag = ibuf->colormanage_flag;
  uchar *dst_byte = dst->byte_data_for_write();
  float *dst_float = dst->float_data_for_write();
  if (dst_byte == nullptr && dst_float == nullptr) {
    IMB_freeImBuf(dst);
    return nullptr;
  }

  switch (filter) {
    case IMBScaleFilter::Nearest: {
      if (const float *src = ibuf->float_data()) {
        scale_nearest_func(src, src_size, ibuf->channels, dst_float, new_size, threaded);
      }
      if (const uchar *src = ibuf->byte_data()) {
        scale_nearest_func(src, src_size, 4, dst_byte, new_size, threaded);
      }
      break;
    }
    case IMBScaleFilter::Bilinear: {
      if (const float *src = ibuf->float_data()) {
        scale_bilinear(src, src_size, ibuf->channels, dst_float, new_size, threaded);
      }
      if (const uchar *src = ibuf->byte_data()) {
        scale_bilinear(src, src_size, 4, dst_byte, new_size, threaded);
      }
      break;
    }
    case IMBScaleFilter::Box: {
      if (const float *src = ibuf->float_data()) {
        imb_scale_box(src, src_size, ibuf->channels, dst_float, new_size, threaded);
      }
      if (const uchar *src = ibuf->byte_data()) {
        imb_scale_box(src, src_size, 4, dst_byte, new_size, threaded);
      }
      break;
    }
  }
  return dst;
}

}  // namespace blender
