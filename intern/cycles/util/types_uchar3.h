/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uchar3 {
  uchar x, y, z;

#  ifndef __KERNEL_GPU__
  __forceinline uchar operator[](int i) const
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }

  __forceinline uchar &operator[](int i)
  {
    util_assert(i >= 0);
    util_assert(i < 3);
    return *(&x + i);
  }
#  endif
};

ccl_device_inline uchar3 make_uchar3(const uchar x, const uchar y, uchar z)
{
  uchar3 a = {x, y, z};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
