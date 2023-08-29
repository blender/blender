/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

struct vfloat8;

#ifdef __KERNEL_GPU__
struct vint8
#else
struct ccl_try_align(32) vint8
#endif
{
#ifdef __KERNEL_AVX__
  union {
    __m256i m256;
    struct {
      int a, b, c, d, e, f, g, h;
    };
  };

  __forceinline vint8();
  __forceinline vint8(const vint8 &a);
  __forceinline explicit vint8(const __m256i &a);

  __forceinline operator const __m256i &() const;
  __forceinline operator __m256i &();

  __forceinline vint8 &operator=(const vint8 &a);
#else  /* __KERNEL_AVX__ */
  int a, b, c, d, e, f, g, h;
#endif /* __KERNEL_AVX__ */

#ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
#endif
};

ccl_device_inline vint8 make_vint8(int a, int b, int c, int d, int e, int f, int g, int h);
ccl_device_inline vint8 make_vint8(int i);
ccl_device_inline vint8 make_vint8(const vfloat8 f);
ccl_device_inline vint8 make_vint8(const int4 a, const int4 b);

CCL_NAMESPACE_END
