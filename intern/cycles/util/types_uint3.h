/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uint3 {
  uint x, y, z;

#  ifndef __KERNEL_GPU__
  __forceinline uint operator[](uint i) const;
  __forceinline uint &operator[](uint i);
#  endif
};

ccl_device_inline uint3 make_uint3(uint x, uint y, uint z);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
