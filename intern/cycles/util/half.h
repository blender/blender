/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_HALF_H__
#define __UTIL_HALF_H__

#include "util/math.h"
#include "util/types.h"

#if !defined(__KERNEL_GPU__) && defined(__KERNEL_SSE2__)
#  include "util/simd.h"
#endif

CCL_NAMESPACE_BEGIN

/* Half Floats */

#if defined(__KERNEL_METAL__)

ccl_device_inline float half_to_float(half h_in)
{
  float f;
  union {
    half h;
    uint16_t s;
  } val;
  val.h = h_in;

  *((ccl_private int *)&f) = ((val.s & 0x8000) << 16) | (((val.s & 0x7c00) + 0x1C000) << 13) |
                             ((val.s & 0x03FF) << 13);

  return f;
}

#else

/* CUDA has its own half data type, no need to define then */
#  if !defined(__KERNEL_CUDA__) && !defined(__KERNEL_HIP__) && !defined(__KERNEL_ONEAPI__)
/* Implementing this as a class rather than a typedef so that the compiler can tell it apart from
 * unsigned shorts. */
class half {
 public:
  half() = default;
  half(const unsigned short &i) : v(i) {}
  operator unsigned short()
  {
    return v;
  }
  half &operator=(const unsigned short &i)
  {
    v = i;
    return *this;
  }

 private:
  unsigned short v;
};
#  endif

struct half4 {
  half x, y, z, w;
};
#endif

/* Conversion to/from half float for image textures
 *
 * Simplified float to half for fast sampling on processor without a native
 * instruction, and eliminating any NaN and inf values. */

ccl_device_inline half float_to_half_image(float f)
{
#if defined(__KERNEL_METAL__) || defined(__KERNEL_ONEAPI__)
  return half(min(f, 65504.0f));
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __float2half(min(f, 65504.0f));
#else
  const uint u = __float_as_uint(f);
  /* Sign bit, shifted to its position. */
  uint sign_bit = u & 0x80000000;
  sign_bit >>= 16;
  /* Exponent. */
  uint exponent_bits = u & 0x7f800000;
  /* Non-sign bits. */
  uint value_bits = u & 0x7fffffff;
  value_bits >>= 13;     /* Align mantissa on MSB. */
  value_bits -= 0x1c000; /* Adjust bias. */
  /* Flush-to-zero. */
  value_bits = (exponent_bits < 0x38800000) ? 0 : value_bits;
  /* Clamp-to-max. */
  value_bits = (exponent_bits > 0x47000000) ? 0x7bff : value_bits;
  /* Denormals-as-zero. */
  value_bits = (exponent_bits == 0 ? 0 : value_bits);
  /* Re-insert sign bit and return. */
  return (value_bits | sign_bit);
#endif
}

ccl_device_inline float half_to_float_image(half h)
{
#if defined(__KERNEL_METAL__)
  return half_to_float(h);
#elif defined(__KERNEL_ONEAPI__)
  return float(h);
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __half2float(h);
#else
  const int x = ((h & 0x8000) << 16) | (((h & 0x7c00) + 0x1C000) << 13) | ((h & 0x03FF) << 13);
  return __int_as_float(x);
#endif
}

ccl_device_inline float4 half4_to_float4_image(const half4 h)
{
  /* Unable to use because it gives different results half_to_float_image, can we
   * modify float_to_half_image so the conversion results are identical? */
#if 0 /* defined(__KERNEL_AVX2__) */
  /* CPU: AVX. */
  __m128i x = _mm_castpd_si128(_mm_load_sd((const double *)&h));
  return float4(_mm_cvtph_ps(x));
#endif

  const float4 f = make_float4(half_to_float_image(h.x),
                               half_to_float_image(h.y),
                               half_to_float_image(h.z),
                               half_to_float_image(h.w));
  return f;
}

/* Conversion to half float texture for display.
 *
 * Simplified float to half for fast display texture conversion on processors
 * without a native instruction. Assumes no negative, no NaN, no inf, and sets
 * denormal to 0. */

ccl_device_inline half float_to_half_display(const float f)
{
#if defined(__KERNEL_METAL__) || defined(__KERNEL_ONEAPI__)
  return half(min(f, 65504.0f));
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __float2half(min(f, 65504.0f));
#else
  const int x = __float_as_int((f > 0.0f) ? ((f < 65504.0f) ? f : 65504.0f) : 0.0f);
  const int absolute = x & 0x7FFFFFFF;
  const int Z = absolute + 0xC8000000;
  const int result = (absolute < 0x38800000) ? 0 : Z;
  const int rshift = (result >> 13);
  return (rshift & 0x7FFF);
#endif
}

ccl_device_inline half4 float4_to_half4_display(const float4 f)
{
#ifdef __KERNEL_SSE__
  /* CPU: SSE and AVX. */
  float4 x = min(max(f, make_float4(0.0f)), make_float4(65504.0f));
#  ifdef __KERNEL_AVX2__
  int4 rpack = int4(_mm_cvtps_ph(x, 0));
#  else
  int4 absolute = cast(x) & make_int4(0x7FFFFFFF);
  int4 Z = absolute + make_int4(0xC8000000);
  int4 result = andnot(absolute < make_int4(0x38800000), Z);
  int4 rshift = (result >> 13) & make_int4(0x7FFF);
  int4 rpack = int4(_mm_packs_epi32(rshift, rshift));
#  endif
  half4 h;
  _mm_storel_pi((__m64 *)&h, _mm_castsi128_ps(rpack));
  return h;
#else
  /* GPU and scalar fallback. */
  const half4 h = {float_to_half_display(f.x),
                   float_to_half_display(f.y),
                   float_to_half_display(f.z),
                   float_to_half_display(f.w)};
  return h;
#endif
}

CCL_NAMESPACE_END

#endif /* __UTIL_HALF_H__ */
