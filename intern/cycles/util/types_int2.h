/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct int2 {
  int x, y;

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 2);
    return *(&x + i);
  }

  __forceinline int &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 2);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline int2 make_int2(const int x, const int y)
{
  int2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
