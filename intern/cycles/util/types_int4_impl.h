/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_INT4_IMPL_H__
#define __UTIL_TYPES_INT4_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
#  ifdef __KERNEL_SSE__
__forceinline int4::int4()
{
}

__forceinline int4::int4(const int4 &a) : m128(a.m128)
{
}

__forceinline int4::int4(const __m128i &a) : m128(a)
{
}

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

ccl_device_inline int4 make_int4(int i)
{
#  ifdef __KERNEL_SSE__
  int4 a(_mm_set1_epi32(i));
#  else
  int4 a = {i, i, i, i};
#  endif
  return a;
}

ccl_device_inline int4 make_int4(int x, int y, int z, int w)
{
#  ifdef __KERNEL_SSE__
  int4 a(_mm_set_epi32(w, z, y, x));
#  else
  int4 a = {x, y, z, w};
#  endif
  return a;
}

ccl_device_inline int4 make_int4(const float3 &f)
{
#  ifdef __KERNEL_SSE__
  int4 a(_mm_cvtps_epi32(f.m128));
#  elif defined(__KERNEL_ONEAPI__)
  int4 a = {(int)f.x, (int)f.y, (int)f.z, 0};
#  else
  int4 a = {(int)f.x, (int)f.y, (int)f.z, (int)f.w};
#  endif
  return a;
}

ccl_device_inline int4 make_int4(const float4 &f)
{
#  ifdef __KERNEL_SSE__
  int4 a(_mm_cvtps_epi32(f.m128));
#  else
  int4 a = {(int)f.x, (int)f.y, (int)f.z, (int)f.w};
#  endif
  return a;
}

ccl_device_inline void print_int4(const char *label, const int4 &a)
{
  printf("%s: %d %d %d %d\n", label, a.x, a.y, a.z, a.w);
}
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_INT4_IMPL_H__ */
