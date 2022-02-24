/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_INT2_H__
#define __UTIL_TYPES_INT2_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
struct int2 {
  int x, y;

  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
};

ccl_device_inline int2 make_int2(int x, int y);
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_INT2_H__ */
