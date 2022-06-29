/* SPDX-License-Identifier: BSD-3-Clause
 * Original code Copyright 2017, Intel Corporation
 * Modifications Copyright 2018-2022 Blender Foundation. */

#ifndef __UTIL_TYPES_FLOAT8_IMPL_H__
#define __UTIL_TYPES_FLOAT8_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
#  ifdef __KERNEL_AVX2__
__forceinline float8::float8()
{
}

__forceinline float8::float8(const float8 &f) : m256(f.m256)
{
}

__forceinline float8::float8(const __m256 &f) : m256(f)
{
}

__forceinline float8::operator const __m256 &() const
{
  return m256;
}

__forceinline float8::operator __m256 &()
{
  return m256;
}

__forceinline float8 &float8::operator=(const float8 &f)
{
  m256 = f.m256;
  return *this;
}
#  endif /* __KERNEL_AVX2__ */

__forceinline float float8::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}

__forceinline float &float8::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}

ccl_device_inline float8 make_float8(float f)
{
#  ifdef __KERNEL_AVX2__
  float8 r(_mm256_set1_ps(f));
#  else
  float8 r = {f, f, f, f, f, f, f, f};
#  endif
  return r;
}

ccl_device_inline float8
make_float8(float a, float b, float c, float d, float e, float f, float g, float h)
{
#  ifdef __KERNEL_AVX2__
  float8 r(_mm256_set_ps(a, b, c, d, e, f, g, h));
#  else
  float8 r = {a, b, c, d, e, f, g, h};
#  endif
  return r;
}

#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_FLOAT8_IMPL_H__ */
