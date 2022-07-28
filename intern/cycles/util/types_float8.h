/* SPDX-License-Identifier: BSD-3-Clause
 * Original code Copyright 2017, Intel Corporation
 * Modifications Copyright 2018-2022 Blender Foundation. */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/* float8 is a reserved type in Metal that has not been implemented. For
 * that reason this is named float8_t and not using native vector types. */

#ifdef __KERNEL_GPU__
struct float8_t
#else
struct ccl_try_align(32) float8_t
#endif
{
#ifdef __KERNEL_AVX2__
  union {
    __m256 m256;
    struct {
      float a, b, c, d, e, f, g, h;
    };
  };

  __forceinline float8_t();
  __forceinline float8_t(const float8_t &a);
  __forceinline explicit float8_t(const __m256 &a);

  __forceinline operator const __m256 &() const;
  __forceinline operator __m256 &();

  __forceinline float8_t &operator=(const float8_t &a);

#else  /* __KERNEL_AVX2__ */
  float a, b, c, d, e, f, g, h;
#endif /* __KERNEL_AVX2__ */

#ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
#endif
};

ccl_device_inline float8_t make_float8_t(float f);
ccl_device_inline float8_t
make_float8_t(float a, float b, float c, float d, float e, float f, float g, float h);

CCL_NAMESPACE_END
