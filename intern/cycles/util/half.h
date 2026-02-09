/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/math_base.h"
#include "util/math_float4.h"
#include "util/math_int4.h"
#include "util/types_base.h"
#include "util/types_float4.h"
#include "util/types_int4.h"
#include "util/types_uint4.h"

#if !defined(__KERNEL_GPU__) && defined(__KERNEL_SSE2__)
#  include "util/optimization.h"  // IWYU pragma: keep
#  include "util/simd.h"          // IWYU pragma: keep
#endif

CCL_NAMESPACE_BEGIN

/* Half Floats */

#if !defined(__KERNEL_GPU__)
/* GPUs have native support for this type.
 * Implementing this as a class rather than a typedef so that the compiler can tell it apart from
 * uint16_ts. */
class half {
 public:
  half() = default;
  half(const uint16_t &i) : v(i) {}
  operator uint16_t() const
  {
    return v;
  }
  half &operator=(const uint16_t &i)
  {
    v = i;
    return *this;
  }

 private:
  uint16_t v;
};
#endif

#if !defined(__KERNEL_METAL__)
struct half3 {
  half x, y, z;
};

struct half4 {
  half x, y, z, w;
};
#endif

#if !defined(__KERNEL_GPU__)
/* Optimized fallback implementations with fast path for normal and denormal numbers, assuming
 * no Infs or NaNs. Based on public domain functions from.
 *
 * https://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/
 * https://gist.github.com/rygorous/2144712
 * https://gist.github.com/rygorous/2156668
 * https://gist.github.com/rygorous/4d9e9e88cab13c703773dc767a23575f
 */
ccl_device_inline float fallback_half_to_float(const half h)
{
  const uint32_t bits = uint16_t(h);
  const uint32_t s = (bits & 0x8000) << 16;
  const uint32_t em = (bits & 0x7fff) << 13;
  const float f = __int_as_float(em) * __int_as_float(0x77800000 /* 2^112 */);
  return __int_as_float(__float_as_uint(f) | s);
}

ccl_device_inline float4 fallback_half4_to_float4(const half4 h)
{
  const int4 i = make_int4(uint16_t(h.x), uint16_t(h.y), uint16_t(h.z), uint16_t(h.w));
  const int4 s = (i & 0x8000) << 16;
  const int4 em = (i & 0x7fff) << 13;
  const float4 f = cast(em) * __int_as_float(0x77800000 /* 2^112 */);
  return cast(cast(f) | s);
}

ccl_device_inline float3 fallback_half3_to_float3(const half3 h)
{
  return make_float3(fallback_half4_to_float4({h.x, h.y, h.z, 0}));
}

ccl_device_inline half fallback_float_to_half(const float f)
{
  const int c_f16max = (127 + 16) << 23;
  const int c_infty_as_fp16 = 0x7c00;
  const int c_min_normal = (127 - 14) << 23;
  const int c_denorm_magic = ((127 - 15) + (23 - 10) + 1) << 23;
  const int c_normal_bias = 0xfff - ((127 - 15) << 23);

  const uint f_i = __float_as_uint(f);
  const uint sign_i = f_i & 0x80000000u;
  const int abs_i = int(f_i ^ sign_i);

  uint16_t res;

  if (abs_i >= c_f16max) {
    /* Overflows to infinity. */
    res = uint16_t(c_infty_as_fp16);
  }
  else if (abs_i < c_min_normal) {
    /* Denormal. */
    float denorm_f = __uint_as_float(uint(abs_i));
    denorm_f += __int_as_float(c_denorm_magic);
    const int denorm_i = int(__float_as_uint(denorm_f)) - c_denorm_magic;
    res = uint16_t(denorm_i);
  }
  else {
    /* Normal. */
    const int mant_odd = int(uint(abs_i) >> 13) & 1;
    res = uint16_t((abs_i + c_normal_bias + mant_odd) >> 13);
  }

  return half(res | uint16_t(sign_i >> 16));
}

ccl_device_inline half4 fallback_float4_to_half4(const float4 f)
{
  const int4 c_f16max = make_int4((127 + 16) << 23);
  const int4 c_infty_as_fp16 = make_int4(0x7c00);
  const int4 c_min_normal = make_int4((127 - 14) << 23);
  const int4 c_denorm_magic = make_int4(((127 - 15) + (23 - 10) + 1) << 23);
  const int4 c_normal_bias = make_int4(0xfff - ((127 - 15) << 23));

  const float4 abs_f = fabs(f);
  const int4 abs_i = __float4_as_int4(abs_f);

  const int4 b_isregular = c_f16max > abs_i;
  const int4 b_isdenorm = c_min_normal > abs_i;

  /* Denormal. */
  const float4 denorm_f = abs_f + __int4_as_float4(c_denorm_magic);
  const int4 denorm_i = __float4_as_int4(denorm_f) - c_denorm_magic;

  /* Normal. */
  const int4 mant_odd = (abs_i << (31 - 13)) >> 31;
  const int4 normal = srl(abs_i + c_normal_bias - mant_odd, 13);

  /* Combined normal and denormal. */
  const int4 nonspecial = select(b_isdenorm, denorm_i, normal);

  /* Combine overflow to infinity. */
  const int4 combined = select(b_isregular, nonspecial, c_infty_as_fp16);

  const int4 sign_i = __float4_as_int4(f ^ abs_f);
  const int4 res = combined | (sign_i >> 16);

  return {
      half(uint16_t(res.x)), half(uint16_t(res.y)), half(uint16_t(res.z)), half(uint16_t(res.w))};
}

ccl_device_inline half3 fallback_float3_to_half3(const float3 f)
{
  const half4 h = fallback_float4_to_half4(make_float4(f));
  return {h.x, h.y, h.z};
}
#endif

ccl_device_inline float half_to_float(half h)
{
#if defined(__KERNEL_METAL__) || defined(__KERNEL_ONEAPI__)
  return float(h);
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __half2float(h);
/* We assume half instructions are always supported when there is ARM Neon,
 * which implies ARMv8.2-A+. There is no official Blender minimum, but is
 * already assumed elsewhere in Blender and not that recent. */
#elif defined(__ARM_NEON) || defined(_M_ARM64)
  uint16x4_t v = vdup_n_u16(uint16_t(h));
  return vgetq_lane_f32(vcvt_f32_f16(vreinterpret_f16_u16(v)), 0);
#elif defined(__F16C__)
  return _cvtsh_ss(uint16_t(h));
#else
  /* The fallback is fast so don't bother with native instructions. */
  return fallback_half_to_float(h);
#endif
}

ccl_device_inline half float_to_half(const float f)
{
#if defined(__KERNEL_METAL__) || defined(__KERNEL_ONEAPI__)
  return half(f);
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __float2half(f);
#elif defined(__ARM_NEON) || defined(_M_ARM64)
  return half(vget_lane_u16(vreinterpret_u16_f16(vcvt_f16_f32(vdupq_n_f32(f))), 0));
#elif defined(__F16C__)
  return half((uint16_t)_cvtss_sh(f, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
#else
  return fallback_float_to_half(f);
#endif
}

ccl_device_inline half4 float4_to_half4(const float4 f)
{
#if defined(__KERNEL_METAL__)
  return {half(f.x), half(f.y), half(f.z), half(f.w)};
#elif defined(__KERNEL_ONEAPI__)
  return {half(f.x), half(f.y), half(f.z), half(f.w)};
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return {__float2half(f.x), __float2half(f.y), __float2half(f.z), __float2half(f.w)};
#elif defined(__ARM_NEON) || defined(_M_ARM64)
  half4 r;
  vst1_u16(reinterpret_cast<uint16_t *>(&r),
           vreinterpret_u16_f16(vcvt_f16_f32(float32x4_t{f.x, f.y, f.z, f.w})));
  return r;
#elif defined(__KERNEL_SSE__) && defined(__F16C__)
  half4 r;
  const __m128i h = _mm_cvtps_ph(_mm_loadu_ps(&f.x),
                                 _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
  _mm_storel_epi64(reinterpret_cast<__m128i *>(&r), h);
  return r;
#else
  return fallback_float4_to_half4(f);
#endif
}

ccl_device_inline float4 half4_to_float4(const half4 h)
{
#if defined(__KERNEL_METAL__)
  return {float(h.x), float(h.y), float(h.z), float(h.w)};
#elif defined(__KERNEL_ONEAPI__)
  return {float(h.x), float(h.y), float(h.z), float(h.w)};
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return {__half2float(h.x), __half2float(h.y), __half2float(h.z), __half2float(h.w)};
#elif defined(__ARM_NEON) || defined(_M_ARM64)
  float4 r;
  vst1q_f32(&r.x,
            vcvt_f32_f16(vreinterpret_f16_u16(vld1_u16(reinterpret_cast<const uint16_t *>(&h)))));
  return r;
#elif defined(__KERNEL_SSE__) && defined(__F16C__)
  float4 r;
  _mm_storeu_ps(&r.x, _mm_cvtph_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&h))));
  return r;
#else
  return fallback_half4_to_float4(h);
#endif
}

ccl_device_inline half3 float3_to_half3(const float3 f)
{
#if defined(__KERNEL_GPU__)
  return {float_to_half(f.x), float_to_half(f.y), float_to_half(f.z)};
#elif defined(__ARM_NEON) || defined(_M_ARM64)
  const uint16x4_t h = vreinterpret_u16_f16(vcvt_f16_f32(float32x4_t{f.x, f.y, f.z, 0.0f}));
  return {half(vget_lane_u16(h, 0)), half(vget_lane_u16(h, 1)), half(vget_lane_u16(h, 2))};
#elif defined(__KERNEL_SSE__) && defined(__F16C__)
  const __m128i h = _mm_cvtps_ph(_mm_set_ps(0.0f, f.z, f.y, f.x),
                                 _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
  return {half(uint16_t(_mm_extract_epi16(h, 0))),
          half(uint16_t(_mm_extract_epi16(h, 1))),
          half(uint16_t(_mm_extract_epi16(h, 2)))};
#else
  half4 h = fallback_float4_to_half4(make_float4(f));
  return {h.x, h.y, h.z};
#endif
}

ccl_device_inline float3 half3_to_float3(const half3 h)
{
#if defined(__KERNEL_GPU__)
  return make_float3(half_to_float(h.x), half_to_float(h.y), half_to_float(h.z));
#elif defined(__ARM_NEON) || defined(_M_ARM64)
  const float32x4_t f = vcvt_f32_f16(
      vreinterpret_f16_u16(uint16x4_t{uint16_t(h.x), uint16_t(h.y), uint16_t(h.z), 0}));
  return make_float3(vgetq_lane_f32(f, 0), vgetq_lane_f32(f, 1), vgetq_lane_f32(f, 2));
#elif defined(__KERNEL_SSE__) && defined(__F16C__)
  const __m128i v = _mm_set_epi16(0, 0, 0, 0, 0, uint16_t(h.z), uint16_t(h.y), uint16_t(h.x));
  const __m128 f = _mm_cvtph_ps(v);
  return make_float3(float4(f));
#else
  return make_float3(fallback_half4_to_float4({h.x, h.y, h.z, 0}));
#endif
}

/* For image textutes */
ccl_device_inline half float_to_half_image(const float f)
{
  return float_to_half(clamp(f, -65504.0f, 65504.0f));
}

ccl_device_inline float half_to_float_image(half h)
{
  return half_to_float(h);
}

ccl_device_inline float4 half4_to_float4_image(const half4 h)
{
  return half4_to_float4(h);
}

/* For render display */
ccl_device_inline half float_to_half_display(const float f)
{
  return float_to_half(clamp(f, 0.0f, 65504.0f));
}

ccl_device_inline half4 float4_to_half4_display(const float4 f)
{
  return float4_to_half4(clamp(f, make_float4(0.0f), make_float4(65504.0f)));
}

#ifndef __KERNEL_GPU__
ccl_device_inline float half_is_finite(const half h)
{
  const int exponent = (uint16_t(h) >> 10) & 0x001f;
  return exponent < 31;
}
#endif

CCL_NAMESPACE_END
