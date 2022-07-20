/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_INT4_H__
#define __UTIL_TYPES_INT4_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)

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

  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
};

ccl_device_inline int4 make_int4(int i);
ccl_device_inline int4 make_int4(int x, int y, int z, int w);
ccl_device_inline int4 make_int4(const float3 &f);
ccl_device_inline int4 make_int4(const float4 &f);
ccl_device_inline void print_int4(const char *label, const int4 &a);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_INT4_H__ */
