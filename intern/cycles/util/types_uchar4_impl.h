/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TYPES_UCHAR4_IMPL_H__
#define __UTIL_TYPES_UCHAR4_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
#  ifndef __KERNEL_GPU__
uchar uchar4::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 4);
  return *(&x + i);
}

uchar &uchar4::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 4);
  return *(&x + i);
}
#  endif

ccl_device_inline uchar4 make_uchar4(uchar x, uchar y, uchar z, uchar w)
{
  uchar4 a = {x, y, z, w};
  return a;
}
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UCHAR4_IMPL_H__ */
