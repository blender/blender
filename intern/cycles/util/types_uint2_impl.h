/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifndef __KERNEL_GPU__
__forceinline uint uint2::operator[](uint i) const
{
  util_assert(i < 2);
  return *(&x + i);
}

__forceinline uint &uint2::operator[](uint i)
{
  util_assert(i < 2);
  return *(&x + i);
}
#  endif

ccl_device_inline uint2 make_uint2(uint x, uint y)
{
  uint2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
