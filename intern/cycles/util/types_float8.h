/* SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileCopyrightText: 2018-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Originally by Intel Corporation, modified by the Blender Foundation. */

#pragma once

#include "util/types_base.h"
#include "util/types_float4.h"
#include "util/types_int8.h"

CCL_NAMESPACE_BEGIN

/* float8 is a reserved type in Metal that has not been implemented. For
 * that reason this is named vfloat8 and not using native vector types. */

#ifdef __KERNEL_GPU__
struct vfloat8
#else
struct ccl_try_align(32) vfloat8
#endif
{
#ifdef __KERNEL_AVX__
  union {
    __m256 m256;
    struct {
      float a, b, c, d, e, f, g, h;
    };
  };

  __forceinline vfloat8() = default;
  __forceinline vfloat8(const vfloat8 &a) = default;
  __forceinline explicit vfloat8(const __m256 &a) : m256(a) {}

  __forceinline operator const __m256 &() const
  {
    return m256;
  }
  __forceinline operator __m256 &()
  {
    return m256;
  }

  __forceinline vfloat8 &operator=(const vfloat8 &a)
  {
    m256 = a.m256;
    return *this;
  }

#else  /* __KERNEL_AVX__ */
  float a, b, c, d, e, f, g, h;
#endif /* __KERNEL_AVX__ */

#ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 8);
    return *(&a + i);
  }
  __forceinline float &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 8);
    return *(&a + i);
  }
#endif
};

ccl_device_inline vfloat8 make_vfloat8(const float f)
{
#ifdef __KERNEL_AVX__
  vfloat8 r(_mm256_set1_ps(f));
#else
  vfloat8 r = {f, f, f, f, f, f, f, f};
#endif
  return r;
}

ccl_device_inline vfloat8 make_vfloat8(const float a,
                                       const float b,
                                       float c,
                                       const float d,
                                       float e,
                                       const float f,
                                       float g,
                                       const float h)
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

ccl_device_inline void print_vfloat8(const ccl_private char *label, const vfloat8 a)
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

ccl_device_inline vint8 make_vint8(const vfloat8 f)
{
#ifdef __KERNEL_AVX__
  return vint8(_mm256_cvtps_epi32(f.m256));
#else
  return make_vint8(
      (int)f.a, (int)f.b, (int)f.c, (int)f.d, (int)f.e, (int)f.f, (int)f.g, (int)f.h);
#endif
}

CCL_NAMESPACE_END
