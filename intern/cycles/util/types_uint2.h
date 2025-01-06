/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uint2 {
  uint x, y;

#  ifndef __KERNEL_GPU__
  __forceinline uint operator[](uint i) const
  {
    util_assert(i < 2);
    return *(&x + i);
  }

  __forceinline uint &operator[](uint i)
  {
    util_assert(i < 2);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline uint2 make_uint2(const uint x, const uint y)
{
  uint2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
