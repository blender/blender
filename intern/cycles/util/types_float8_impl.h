/* SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileCopyrightText: 2018-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Originally by Intel Corporation, modified by the Blender Foundation. */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_AVX__
__forceinline vfloat8::vfloat8() {}

__forceinline vfloat8::vfloat8(const vfloat8 &f) : m256(f.m256) {}

__forceinline vfloat8::vfloat8(const __m256 &f) : m256(f) {}

__forceinline vfloat8::operator const __m256 &() const
{
  return m256;
}

__forceinline vfloat8::operator __m256 &()
{
  return m256;
}

__forceinline vfloat8 &vfloat8::operator=(const vfloat8 &f)
{
  m256 = f.m256;
  return *this;
}
#endif /* __KERNEL_AVX__ */

#ifndef __KERNEL_GPU__
__forceinline float vfloat8::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}

__forceinline float &vfloat8::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 8);
  return *(&a + i);
}
#endif

ccl_device_inline vfloat8 make_vfloat8(float f)
{
#ifdef __KERNEL_AVX__
  vfloat8 r(_mm256_set1_ps(f));
#else
  vfloat8 r = {f, f, f, f, f, f, f, f};
#endif
  return r;
}

ccl_device_inline vfloat8
make_vfloat8(float a, float b, float c, float d, float e, float f, float g, float h)
{
#ifdef __KERNEL_AVX__
  vfloat8 r(_mm256_setr_ps(a, b, c, d, e, f, g, h));
#else
  vfloat8 r = {a, b, c, d, e, f, g, h};
#endif
  return r;
}

ccl_device_inline vfloat8 make_vfloat8(const float4 a, const float4 b)
{
#ifdef __KERNEL_AVX__
  return vfloat8(_mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1));
#else
  return make_vfloat8(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
#endif
}

ccl_device_inline void print_vfloat8(ccl_private const char *label, const vfloat8 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f\n",
         label,
         (double)a.a,
         (double)a.b,
         (double)a.c,
         (double)a.d,
         (double)a.e,
         (double)a.f,
         (double)a.g,
         (double)a.h);
#endif
}

CCL_NAMESPACE_END
