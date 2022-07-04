/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_UINT4_H__
#define __UTIL_TYPES_UINT4_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
struct uint4 {
  uint x, y, z, w;

  __forceinline uint operator[](uint i) const;
  __forceinline uint &operator[](uint i);
};

ccl_device_inline uint4 make_uint4(uint x, uint y, uint z, uint w);
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UINT4_H__ */
