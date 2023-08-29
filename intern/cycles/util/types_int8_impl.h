/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_AVX__
__forceinline vint8::vint8() {}

__forceinline vint8::vint8(const vint8 &a) : m256(a.m256) {}

__forceinline vint8::vint8(const __m256i &a) : m256(a) {}

__forceinline vint8::operator const __m256i &() const
{
  return m256;
}

__forceinline vint8::operator __m256i &()
{
  return m256;
}

__forceinline vint8 &vint8::operator=(const vint8 &a)
{
  m256 = a.m256;
  return *this;
}
#endif /* __KERNEL_AVX__ */

#ifndef __KERNEL_GPU__
__forceinline int vint8::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}

__forceinline int &vint8::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}
#endif

ccl_device_inline vint8 make_vint8(int a, int b, int c, int d, int e, int f, int g, int h)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_set_epi32(h, g, f, e, d, c, b, a));
#else
  return {a, b, c, d, e, f, g, h};
#endif
}

ccl_device_inline vint8 make_vint8(int i)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_set1_epi32(i));
#else
  return make_vint8(i, i, i, i, i, i, i, i);
#endif
}

ccl_device_inline vint8 make_vint8(const vfloat8 f)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_cvtps_epi32(f.m256));
#else
  return make_vint8(
      (int)f.a, (int)f.b, (int)f.c, (int)f.d, (int)f.e, (int)f.f, (int)f.g, (int)f.h);
#endif
}

ccl_device_inline vint8 make_vint8(const int4 a, const int4 b)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_insertf128_si256(_mm256_castsi128_si256(a.m128), b.m128, 1));
#else
  return make_vint8(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
#endif
}

CCL_NAMESPACE_END
