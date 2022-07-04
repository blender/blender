/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_FLOAT2_H__
#define __UTIL_TYPES_FLOAT2_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
struct float2 {
  float x, y;

  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
};

ccl_device_inline float2 make_float2(float x, float y);
ccl_device_inline void print_float2(const char *label, const float2 &a);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_FLOAT2_H__ */
