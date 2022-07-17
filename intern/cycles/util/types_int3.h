/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_INT3_H__
#define __UTIL_TYPES_INT3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
struct ccl_try_align(16) int3
{
#  ifdef __KERNEL_SSE__
  union {
    __m128i m128;
    struct {
      int x, y, z, w;
    };
  };

  __forceinline int3();
  __forceinline int3(const int3 &a);
  __forceinline explicit int3(const __m128i &a);

  __forceinline operator const __m128i &() const;
  __forceinline operator __m128i &();

  __forceinline int3 &operator=(const int3 &a);
#  else  /* __KERNEL_SSE__ */
  int x, y, z, w;
#  endif /* __KERNEL_SSE__ */

  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
};

ccl_device_inline int3 make_int3(int i);
ccl_device_inline int3 make_int3(int x, int y, int z);
ccl_device_inline void print_int3(const char *label, const int3 &a);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_INT3_H__ */
