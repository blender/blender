/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_UCHAR3_H__
#define __UTIL_TYPES_UCHAR3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
struct uchar3 {
  uchar x, y, z;

  __forceinline uchar operator[](int i) const;
  __forceinline uchar &operator[](int i);
};

ccl_device_inline uchar3 make_uchar3(uchar x, uchar y, uchar z);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UCHAR3_H__ */
