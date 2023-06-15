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
int int2::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 2);
  return *(&x + i);
}

int &int2::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 2);
  return *(&x + i);
}
#  endif

ccl_device_inline int2 make_int2(int x, int y)
{
  int2 a = {x, y};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
