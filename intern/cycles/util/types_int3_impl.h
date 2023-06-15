/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_SSE__
__forceinline int3::int3() {}

__forceinline int3::int3(const __m128i &a) : m128(a) {}

__forceinline int3::int3(const int3 &a) : m128(a.m128) {}

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

#  ifndef __KERNEL_GPU__
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

__forceinline int packed_int3::operator[](int i) const
{
  util_assert(i < 3);
  return *(&x + i);
}

__forceinline int &packed_int3::operator[](int i)
{
  util_assert(i < 3);
  return *(&x + i);
}
#  endif

ccl_device_inline int3 make_int3(int x, int y, int z)
{
#  if defined(__KERNEL_GPU__)
  return {x, y, z};
#  elif defined(__KERNEL_SSE__)
  return int3(_mm_set_epi32(0, z, y, x));
#  else
  return {x, y, z, 0};
#  endif
}

#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline int3 make_int3(int i)
{
#if defined(__KERNEL_GPU__)
  return make_int3(i, i, i);
#elif defined(__KERNEL_SSE__)
  return int3(_mm_set1_epi32(i));
#else
  return {i, i, i, i};
#endif
}

ccl_device_inline packed_int3 make_packed_int3(int x, int y, int z)
{
  packed_int3 a = {x, y, z};
  return a;
}

ccl_device_inline void print_int3(ccl_private const char *label, const int3 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %d %d %d\n", label, a.x, a.y, a.z);
#endif
}

CCL_NAMESPACE_END
