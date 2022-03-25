/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_VECTOR3_H__
#define __UTIL_TYPES_VECTOR3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
template<typename T> class vector3 {
 public:
  T x, y, z;

  __forceinline vector3();
  __forceinline vector3(const T &a);
  __forceinline vector3(const T &x, const T &y, const T &z);
};
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_VECTOR3_H__ */
