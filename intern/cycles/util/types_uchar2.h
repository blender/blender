/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_UCHAR2_H__
#define __UTIL_TYPES_UCHAR2_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
struct uchar2 {
  uchar x, y;

  __forceinline uchar operator[](int i) const;
  __forceinline uchar &operator[](int i);
};

ccl_device_inline uchar2 make_uchar2(uchar x, uchar y);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UCHAR2_H__ */
