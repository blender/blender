/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct int4;

struct ccl_try_align(16) float4
{
#  ifdef __KERNEL_SSE__
  union {
    __m128 m128;
    struct {
      float x, y, z, w;
    };
  };

  __forceinline float4();
  __forceinline explicit float4(const __m128 &a);

  __forceinline operator const __m128 &() const;
  __forceinline operator __m128 &();

  __forceinline float4 &operator=(const float4 &a);

#  else  /* __KERNEL_SSE__ */
  float x, y, z, w;
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
#  endif
};

ccl_device_inline float4 make_float4(float x, float y, float z, float w);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline float4 make_float4(float f);
ccl_device_inline float4 make_float4(const int4 i);
ccl_device_inline void print_float4(ccl_private const char *label, const float4 a);

CCL_NAMESPACE_END
