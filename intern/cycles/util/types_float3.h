/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifdef __KERNEL_ONEAPI__
/* Define float3 as packed for oneAPI. */
struct float3
#  else
struct ccl_try_align(16) float3
#  endif
{
#  ifdef __KERNEL_GPU__
  /* Compact structure for GPU. */
  float x, y, z;
#  else
  /* SIMD aligned structure for CPU. */
#    ifdef __KERNEL_SSE__
  union {
    __m128 m128;
    struct {
      float x, y, z, w;
    };
  };
#    else
  float x, y, z, w;
#    endif
#  endif

#  ifdef __KERNEL_SSE__
  /* Convenient constructors and operators for SIMD, otherwise default is enough. */
  __forceinline float3();
  __forceinline float3(const float3 &a);
  __forceinline explicit float3(const __m128 &a);

  __forceinline operator const __m128 &() const;
  __forceinline operator __m128 &();

  __forceinline float3 &operator=(const float3 &a);
#  endif

#  ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
#  endif
};

ccl_device_inline float3 make_float3(float x, float y, float z);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline float3 make_float3(float f);
ccl_device_inline void print_float3(ccl_private const char *label, const float3 a);

/* Smaller float3 for storage. For math operations this must be converted to float3, so that on the
 * CPU SIMD instructions can be used. */
#if defined(__KERNEL_METAL__)
/* Metal has native packed_float3. */
#elif defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)
/* CUDA and HIP float3 are already packed. */
typedef float3 packed_float3;
#else
struct packed_float3 {
  ccl_device_inline_method packed_float3(){};

  ccl_device_inline_method packed_float3(const float3 &a) : x(a.x), y(a.y), z(a.z) {}

  ccl_device_inline_method operator float3() const
  {
    return make_float3(x, y, z);
  }

  ccl_device_inline_method packed_float3 &operator=(const float3 &a)
  {
    x = a.x;
    y = a.y;
    z = a.z;
    return *this;
  }

  float x, y, z;
};
#endif

static_assert(sizeof(packed_float3) == 12, "packed_float3 expected to be exactly 12 bytes");

CCL_NAMESPACE_END
