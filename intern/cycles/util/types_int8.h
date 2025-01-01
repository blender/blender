/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"
#include "util/types_int4.h"

CCL_NAMESPACE_BEGIN

struct vfloat8;

#ifdef __KERNEL_GPU__
struct vint8
#else
struct ccl_try_align(32) vint8
#endif
{
#ifdef __KERNEL_AVX__
  union {
    __m256i m256;
    struct {
      int a, b, c, d, e, f, g, h;
    };
  };

  __forceinline vint8() = default;
  __forceinline vint8(const vint8 &a) = default;
  __forceinline explicit vint8(const __m256i &a) : m256(a) {}

  __forceinline operator const __m256i &() const
  {
    return m256;
  }
  __forceinline operator __m256i &()
  {
    return m256;
  }

  __forceinline vint8 &operator=(const vint8 &a)
  {
    m256 = a.m256;
    return *this;
  }
#else  /* __KERNEL_AVX__ */
  int a, b, c, d, e, f, g, h;
#endif /* __KERNEL_AVX__ */

#ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 8);
    return *(&a + i);
  }
  __forceinline int &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 8);
    return *(&a + i);
  }
#endif
};

ccl_device_inline vint8
make_vint8(const int a, const int b, int c, const int d, int e, const int f, int g, const int h)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_set_epi32(h, g, f, e, d, c, b, a));
#else
  return {a, b, c, d, e, f, g, h};
#endif
}

ccl_device_inline vint8 make_vint8(const int i)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_set1_epi32(i));
#else
  return make_vint8(i, i, i, i, i, i, i, i);
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
