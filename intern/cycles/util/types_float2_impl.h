/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifndef __KERNEL_GPU__
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
#  endif

ccl_device_inline float2 make_float2(float x, float y)
{
  float2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline void print_float2(ccl_private const char *label, const float2 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f\n", label, (double)a.x, (double)a.y);
#endif
}

CCL_NAMESPACE_END
