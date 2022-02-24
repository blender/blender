/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_UINT2_H__
#define __UTIL_TYPES_UINT2_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
struct uint2 {
  uint x, y;

  __forceinline uint operator[](uint i) const;
  __forceinline uint &operator[](uint i);
};

ccl_device_inline uint2 make_uint2(uint x, uint y);
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UINT2_H__ */
