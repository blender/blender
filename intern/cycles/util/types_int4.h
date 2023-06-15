/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__

struct float3;
struct float4;

struct ccl_try_align(16) int4
{
#  ifdef __KERNEL_SSE__
  union {
    __m128i m128;
    struct {
      int x, y, z, w;
    };
  };

  __forceinline int4();
  __forceinline int4(const int4 &a);
  __forceinline explicit int4(const __m128i &a);

  __forceinline operator const __m128i &() const;
  __forceinline operator __m128i &();

  __forceinline int4 &operator=(const int4 &a);
#  else  /* __KERNEL_SSE__ */
  int x, y, z, w;
#  endif /* __KERNEL_SSE__ */

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
#  endif
};

ccl_device_inline int4 make_int4(int x, int y, int z, int w);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

ccl_device_inline int4 make_int4(int i);
ccl_device_inline int4 make_int4(const float3 f);
ccl_device_inline int4 make_int4(const float4 f);
ccl_device_inline void print_int4(ccl_private const char *label, const int4 a);

CCL_NAMESPACE_END
