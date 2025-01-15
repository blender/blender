/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_float4.h"
#include "util/types_int4.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
ccl_device_inline int4 operator+(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_add_epi32(a.m128, b.m128));
#  else
  return make_int4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
#  endif
}

ccl_device_inline int4 operator+=(int4 &a, const int4 b)
{
  return a = a + b;
}

ccl_device_inline int4 operator-(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_sub_epi32(a.m128, b.m128));
#  else
  return make_int4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
#  endif
}

ccl_device_inline int4 operator-=(int4 &a, const int4 b)
{
  return a = a - b;
}

ccl_device_inline int4 operator>>(const int4 a, const int i)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_srai_epi32(a.m128, i));
#  else
  return make_int4(a.x >> i, a.y >> i, a.z >> i, a.w >> i);
#  endif
}

ccl_device_inline int4 operator<<(const int4 a, const int i)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_slli_epi32(a.m128, i));
#  else
  return make_int4(a.x << i, a.y << i, a.z << i, a.w << i);
#  endif
}

ccl_device_inline int4 operator<(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_cmplt_epi32(a.m128, b.m128));
#  else
  return make_int4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
#  endif
}

ccl_device_inline int4 operator<(const int4 a, const int b)
{
  return a < make_int4(b);
}

ccl_device_inline int4 operator==(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_cmpeq_epi32(a.m128, b.m128));
#  else
  return make_int4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
#  endif
}

ccl_device_inline int4 operator==(const int4 a, const int b)
{
  return a == make_int4(b);
}

ccl_device_inline int4 operator>=(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_xor_si128(_mm_set1_epi32(0xffffffff), _mm_cmplt_epi32(a.m128, b.m128)));
#  else
  return make_int4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
#  endif
}

ccl_device_inline int4 operator>=(const int4 a, const int b)
{
  return a >= make_int4(b);
}

ccl_device_inline int4 operator&(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_and_si128(a.m128, b.m128));
#  else
  return make_int4(a.x & b.x, a.y & b.y, a.z & b.z, a.w & b.w);
#  endif
}

ccl_device_inline int4 operator|(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_or_si128(a.m128, b.m128));
#  else
  return make_int4(a.x | b.x, a.y | b.y, a.z | b.z, a.w | b.w);
#  endif
}

ccl_device_inline int4 operator^(const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_xor_si128(a.m128, b.m128));
#  else
  return make_int4(a.x ^ b.x, a.y ^ b.y, a.z ^ b.z, a.w ^ b.w);
#  endif
}

ccl_device_inline int4 operator&(const int32_t a, const int4 b)
{
  return make_int4(a) & b;
}

ccl_device_inline int4 operator&(const int4 a, const int32_t b)
{
  return a & make_int4(b);
}

ccl_device_inline int4 operator|(const int32_t a, const int4 b)
{
  return make_int4(a) | b;
}

ccl_device_inline int4 operator|(const int4 a, const int32_t b)
{
  return a | make_int4(b);
}

ccl_device_inline int4 operator^(const int32_t a, const int4 b)
{
  return make_int4(a) ^ b;
}

ccl_device_inline int4 operator^(const int4 a, const int32_t b)
{
  return a ^ make_int4(b);
}

ccl_device_inline int4 &operator&=(int4 &a, const int4 b)
{
  return a = a & b;
}
ccl_device_inline int4 &operator&=(int4 &a, const int32_t b)
{
  return a = a & b;
}

ccl_device_inline int4 &operator|=(int4 &a, const int4 b)
{
  return a = a | b;
}
ccl_device_inline int4 &operator|=(int4 &a, const int32_t b)
{
  return a = a | b;
}

ccl_device_inline int4 &operator^=(int4 &a, const int4 b)
{
  return a = a ^ b;
}
ccl_device_inline int4 &operator^=(int4 &a, const int32_t b)
{
  return a = a ^ b;
}

ccl_device_inline int4 &operator<<=(int4 &a, const int32_t b)
{
  return a = a << b;
}
ccl_device_inline int4 &operator>>=(int4 &a, const int32_t b)
{
  return a = a >> b;
}

#  ifdef __KERNEL_SSE__
ccl_device_forceinline int4 srl(const int4 a, const int32_t b)
{
  return int4(_mm_srli_epi32(a.m128, b));
}
#  endif

ccl_device_inline int4 min(const int4 a, const int4 b)
{
#  if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE42__)
  return int4(_mm_min_epi32(a.m128, b.m128));
#  else
  return make_int4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
#  endif
}

ccl_device_inline int4 max(const int4 a, const int4 b)
{
#  if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE42__)
  return int4(_mm_max_epi32(a.m128, b.m128));
#  else
  return make_int4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
#  endif
}

ccl_device_inline int4 clamp(const int4 a, const int4 mn, const int4 mx)
{
  return min(max(a, mn), mx);
}

ccl_device_inline int4 select(const int4 mask, const int4 a, const int4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b)));
#  else
  return make_int4(
      (mask.x) ? a.x : b.x, (mask.y) ? a.y : b.y, (mask.z) ? a.z : b.z, (mask.w) ? a.w : b.w);
#  endif
}

ccl_device_inline int4 load_int4(const int *v)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_loadu_si128((__m128i *)v));
#  else
  return make_int4(v[0], v[1], v[2], v[3]);
#  endif
}
#endif /* __KERNEL_GPU__ */

ccl_device_inline float4 cast(const int4 a)
{
#ifdef __KERNEL_SSE__
  return float4(_mm_castsi128_ps(a));
#else
  return make_float4(
      __int_as_float(a.x), __int_as_float(a.y), __int_as_float(a.z), __int_as_float(a.w));
#endif
}

#ifdef __KERNEL_SSE__
ccl_device_forceinline int4 andnot(const int4 a, const int4 b)
{
  return int4(_mm_andnot_si128(a.m128, b.m128));
}

template<size_t i0, const size_t i1, const size_t i2, const size_t i3>
ccl_device_forceinline int4 shuffle(const int4 a)
{
#  ifdef __KERNEL_NEON__
  int32x4_t result = shuffle_neon<int32x4_t, i0, i1, i2, i3>(vreinterpretq_s32_m128i(a));
  return int4(vreinterpretq_m128i_s32(result));
#  else
  return int4(_mm_shuffle_epi32(a, _MM_SHUFFLE(i3, i2, i1, i0)));
#  endif
}

template<size_t i0, const size_t i1, const size_t i2, const size_t i3>
ccl_device_forceinline int4 shuffle(const int4 a, const int4 b)
{
#  ifdef __KERNEL_NEON__
  int32x4_t result = shuffle_neon<int32x4_t, i0, i1, i2, i3>(vreinterpretq_s32_m128i(a),
                                                             vreinterpretq_s32_m128i(b));
  return int4(vreinterpretq_m128i_s32(result));
#  else
  return int4(_mm_castps_si128(
      _mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _MM_SHUFFLE(i3, i2, i1, i0))));
#  endif
}

template<size_t i0> ccl_device_forceinline int4 shuffle(const int4 b)
{
  return shuffle<i0, i0, i0, i0>(b);
}
#endif

CCL_NAMESPACE_END
