/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_SSE__
__forceinline int4::int4() {}

__forceinline int4::int4(const int4 &a) : m128(a.m128) {}

__forceinline int4::int4(const __m128i &a) : m128(a) {}

__forceinline int4::operator const __m128i &() const
{
  return m128;
}

__forceinline int4::operator __m128i &()
{
  return m128;
}

__forceinline int4 &int4::operator=(const int4 &a)
{
  m128 = a.m128;
  return *this;
}
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
__forceinline int int4::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 4);
  return *(&x + i);
}

__forceinline int &int4::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 4);
  return *(&x + i);
}
#  endif

ccl_device_inline int4 make_int4(int x, int y, int z, int w)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_set_epi32(w, z, y, x));
#  else
  return {x, y, z, w};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline int4 make_int4(int i)
{
#ifdef __KERNEL_SSE__
  return int4(_mm_set1_epi32(i));
#else
  return make_int4(i, i, i, i);
#endif
}

ccl_device_inline int4 make_int4(const float3 f)
{
#if defined(__KERNEL_GPU__)
  return make_int4((int)f.x, (int)f.y, (int)f.z, 0);
#elif defined(__KERNEL_SSE__)
  return int4(_mm_cvtps_epi32(f.m128));
#else
  return make_int4((int)f.x, (int)f.y, (int)f.z, (int)f.w);
#endif
}

ccl_device_inline int4 make_int4(const float4 f)
{
#ifdef __KERNEL_SSE__
  return int4(_mm_cvtps_epi32(f.m128));
#else
  return make_int4((int)f.x, (int)f.y, (int)f.z, (int)f.w);
#endif
}

ccl_device_inline void print_int4(ccl_private const char *label, const int4 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %d %d %d %d\n", label, a.x, a.y, a.z, a.w);
#endif
}

CCL_NAMESPACE_END
