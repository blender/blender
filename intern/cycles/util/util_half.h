/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_HALF_H__
#define __UTIL_HALF_H__

#include "util/util_math.h"
#include "util/util_types.h"

#if !defined(__KERNEL_GPU__) && defined(__KERNEL_SSE2__)
#  include "util/util_simd.h"
#endif

CCL_NAMESPACE_BEGIN

/* Half Floats */

/* CUDA has its own half data type, no need to define then */
#if !defined(__KERNEL_CUDA__) && !defined(__KERNEL_HIP__)
/* Implementing this as a class rather than a typedef so that the compiler can tell it apart from
 * unsigned shorts. */
class half {
 public:
  half() : v(0)
  {
  }
  half(const unsigned short &i) : v(i)
  {
  }
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
#endif

struct half4 {
  half x, y, z, w;
};

/* Conversion to/from half float for image textures
 *
 * Simplified float to half for fast sampling on processor without a native
 * instruction, and eliminating any NaN and inf values. */

ccl_device_inline half float_to_half_image(float f)
{
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __float2half(f);
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
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
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
#if defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
  return __float2half(f);
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
#ifdef __KERNEL_SSE2__
  /* CPU: SSE and AVX. */
  ssef x = min(max(load4f(f), 0.0f), 65504.0f);
#  ifdef __KERNEL_AVX2__
  ssei rpack = _mm_cvtps_ph(x, 0);
#  else
  ssei absolute = cast(x) & 0x7FFFFFFF;
  ssei Z = absolute + 0xC8000000;
  ssei result = andnot(absolute < 0x38800000, Z);
  ssei rshift = (result >> 13) & 0x7FFF;
  ssei rpack = _mm_packs_epi32(rshift, rshift);
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
