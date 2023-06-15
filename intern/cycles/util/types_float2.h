/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct float2 {
  float x, y;

#  ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
#  endif
};

ccl_device_inline float2 make_float2(float x, float y);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline void print_float2(ccl_private const char *label, const float2 a);

CCL_NAMESPACE_END
