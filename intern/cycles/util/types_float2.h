/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"
#include "util/types_int2.h"

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

ccl_device_inline float2 make_float2(const float f)
{
  return {f, f};
}

ccl_device_inline float2 make_float2(const int2 i)
{
  return make_float2((float)i.x, (float)i.y);
}

ccl_device_inline int2 make_int2(const float2 f)
{
  return make_int2((int)f.x, (int)f.y);
}

#if defined __METAL_PRINTF__
#  define print_float2(label, a) metal::os_log_default.log_debug(label ": %.8f %.8f", a.x, a.y)
#else
ccl_device_inline void print_float2(const ccl_private char *label, const float2 a)
{
#  ifdef __KERNEL_PRINTF__
  printf("%s: %.8f %.8f\n", label, (double)a.x, (double)a.y);
#  endif
}
#endif

CCL_NAMESPACE_END
