/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_INT3_IMPL_H__
#define __UTIL_TYPES_INT3_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
#  ifdef __KERNEL_SSE__
__forceinline int3::int3()
{
}

__forceinline int3::int3(const __m128i &a) : m128(a)
{
}

__forceinline int3::int3(const int3 &a) : m128(a.m128)
{
}

__forceinline int3::operator const __m128i &() const
{
  return m128;
}

__forceinline int3::operator __m128i &()
{
  return m128;
}

__forceinline int3 &int3::operator=(const int3 &a)
{
  m128 = a.m128;
  return *this;
}
#  endif /* __KERNEL_SSE__ */

__forceinline int int3::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

__forceinline int &int3::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

ccl_device_inline int3 make_int3(int i)
{
#  ifdef __KERNEL_SSE__
  int3 a(_mm_set1_epi32(i));
#  else
  int3 a = {i, i, i, i};
#  endif
  return a;
}

ccl_device_inline int3 make_int3(int x, int y, int z)
{
#  ifdef __KERNEL_SSE__
  int3 a(_mm_set_epi32(0, z, y, x));
#  else
  int3 a = {x, y, z, 0};
#  endif

  return a;
}

ccl_device_inline void print_int3(const char *label, const int3 &a)
{
  printf("%s: %d %d %d\n", label, a.x, a.y, a.z);
}
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_INT3_IMPL_H__ */
