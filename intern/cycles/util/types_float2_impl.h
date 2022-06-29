/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_FLOAT2_IMPL_H__
#define __UTIL_TYPES_FLOAT2_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

#ifndef __KERNEL_GPU__
#  include <cstdio>
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
__forceinline float float2::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 2);
  return *(&x + i);
}

__forceinline float &float2::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 2);
  return *(&x + i);
}

ccl_device_inline float2 make_float2(float x, float y)
{
  float2 a = {x, y};
  return a;
}

ccl_device_inline void print_float2(const char *label, const float2 &a)
{
  printf("%s: %.8f %.8f\n", label, (double)a.x, (double)a.y);
}
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_FLOAT2_IMPL_H__ */
