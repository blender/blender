/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_FLOAT3_H__
#define __UTIL_TYPES_FLOAT3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__)
struct ccl_try_align(16) float3
{
#  ifdef __KERNEL_SSE__
  union {
    __m128 m128;
    struct {
      float x, y, z, w;
    };
  };

  __forceinline float3();
  __forceinline float3(const float3 &a);
  __forceinline explicit float3(const __m128 &a);

  __forceinline operator const __m128 &() const;
  __forceinline operator __m128 &();

  __forceinline float3 &operator=(const float3 &a);
#  else  /* __KERNEL_SSE__ */
  float x, y, z, w;
#  endif /* __KERNEL_SSE__ */

  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
};

ccl_device_inline float3 make_float3(float f);
ccl_device_inline float3 make_float3(float x, float y, float z);
ccl_device_inline void print_float3(const char *label, const float3 &a);
#endif /* !defined(__KERNEL_GPU__) */

/* Smaller float3 for storage. For math operations this must be converted to float3, so that on the
 * CPU SIMD instructions can be used. */
#if defined(__KERNEL_METAL__)
/* Metal has native packed_float3. */
#elif defined(__KERNEL_CUDA__)
/* CUDA float3 is already packed. */
typedef float3 packed_float3;
#else
/* HIP float3 is not packed (https://github.com/ROCm-Developer-Tools/HIP/issues/706). */
struct packed_float3 {
  ccl_device_inline_method packed_float3(){};

  ccl_device_inline_method packed_float3(const float3 &a) : x(a.x), y(a.y), z(a.z)
  {
  }

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

#endif /* __UTIL_TYPES_FLOAT3_H__ */
