/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uchar2 {
  uchar x, y;

#  ifndef __KERNEL_GPU__
  __forceinline uchar operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 2);
    return *(&x + i);
  }

  __forceinline uchar &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 2);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline uchar2 make_uchar2(const uchar x, const uchar y)
{
  uchar2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
