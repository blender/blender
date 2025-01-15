/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/types_int2.h"

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_METAL__)
ccl_device_inline bool operator==(const int2 a, const int2 b)
{
  return (a.x == b.x && a.y == b.y);
}

ccl_device_inline int2 operator+(const int2 &a, const int2 &b)
{
  return make_int2(a.x + b.x, a.y + b.y);
}

ccl_device_inline int2 operator+=(int2 &a, const int2 &b)
{
  return a = a + b;
}

ccl_device_inline int2 operator-(const int2 &a, const int2 &b)
{
  return make_int2(a.x - b.x, a.y - b.y);
}

ccl_device_inline int2 operator*(const int2 &a, const int2 &b)
{
  return make_int2(a.x * b.x, a.y * b.y);
}

ccl_device_inline int2 operator/(const int2 &a, const int2 &b)
{
  return make_int2(a.x / b.x, a.y / b.y);
}
#endif /* !__KERNEL_METAL__ */

CCL_NAMESPACE_END
