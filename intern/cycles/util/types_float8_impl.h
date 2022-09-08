/* SPDX-License-Identifier: BSD-3-Clause
 * Original code Copyright 2017, Intel Corporation
 * Modifications Copyright 2018-2022 Blender Foundation. */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_AVX2__
__forceinline float8_t::float8_t()
{
}

__forceinline float8_t::float8_t(const float8_t &f) : m256(f.m256)
{
}

__forceinline float8_t::float8_t(const __m256 &f) : m256(f)
{
}

__forceinline float8_t::operator const __m256 &() const
{
  return m256;
}

__forceinline float8_t::operator __m256 &()
{
  return m256;
}

__forceinline float8_t &float8_t::operator=(const float8_t &f)
{
  m256 = f.m256;
  return *this;
}
#endif /* __KERNEL_AVX2__ */

#ifndef __KERNEL_GPU__
__forceinline float float8_t::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}

__forceinline float &float8_t::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}
#endif

ccl_device_inline float8_t make_float8_t(float f)
{
#ifdef __KERNEL_AVX2__
  float8_t r(_mm256_set1_ps(f));
#else
  float8_t r = {f, f, f, f, f, f, f, f};
#endif
  return r;
}

ccl_device_inline float8_t
make_float8_t(float a, float b, float c, float d, float e, float f, float g, float h)
{
#ifdef __KERNEL_AVX2__
  float8_t r(_mm256_setr_ps(a, b, c, d, e, f, g, h));
#else
  float8_t r = {a, b, c, d, e, f, g, h};
#endif
  return r;
}

CCL_NAMESPACE_END
