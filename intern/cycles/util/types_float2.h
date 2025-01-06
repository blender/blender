/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct float2 {
  float x, y;

#  ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
#  endif
};

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

ccl_device_inline float2 make_float2(const float x, const float y)
{
  float2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline void print_float2(const ccl_private char *label, const float2 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f\n", label, (double)a.x, (double)a.y);
#endif
}

CCL_NAMESPACE_END
