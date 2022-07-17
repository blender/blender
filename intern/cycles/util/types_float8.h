/* SPDX-License-Identifier: BSD-3-Clause
 * Original code Copyright 2017, Intel Corporation
 * Modifications Copyright 2018-2022 Blender Foundation. */

#ifndef __UTIL_TYPES_FLOAT8_H__
#define __UTIL_TYPES_FLOAT8_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)

struct ccl_try_align(32) float8
{
#  ifdef __KERNEL_AVX2__
  union {
    __m256 m256;
    struct {
      float a, b, c, d, e, f, g, h;
    };
  };

  __forceinline float8();
  __forceinline float8(const float8 &a);
  __forceinline explicit float8(const __m256 &a);

  __forceinline operator const __m256 &() const;
  __forceinline operator __m256 &();

  __forceinline float8 &operator=(const float8 &a);

#  else  /* __KERNEL_AVX2__ */
  float a, b, c, d, e, f, g, h;
#  endif /* __KERNEL_AVX2__ */

  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
};

ccl_device_inline float8 make_float8(float f);
ccl_device_inline float8
make_float8(float a, float b, float c, float d, float e, float f, float g, float h);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_FLOAT8_H__ */
