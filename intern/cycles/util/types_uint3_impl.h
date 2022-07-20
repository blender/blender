/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_UINT3_IMPL_H__
#define __UTIL_TYPES_UINT3_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
__forceinline uint uint3::operator[](uint i) const
{
  util_assert(i < 3);
  return *(&x + i);
}

__forceinline uint &uint3::operator[](uint i)
{
  util_assert(i < 3);
  return *(&x + i);
}

ccl_device_inline uint3 make_uint3(uint x, uint y, uint z)
{
  uint3 a = {x, y, z};
  return a;
}
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UINT3_IMPL_H__ */
