/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TYPES_UCHAR3_IMPL_H__
#define __UTIL_TYPES_UCHAR3_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#if !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__)
uchar uchar3::operator[](int i) const
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

uchar &uchar3::operator[](int i)
{
  util_assert(i >= 0);
  util_assert(i < 3);
  return *(&x + i);
}

ccl_device_inline uchar3 make_uchar3(uchar x, uchar y, uchar z)
{
  uchar3 a = {x, y, z};
  return a;
}
#endif /* !defined(__KERNEL_GPU__) || defined(__KERNEL_ONEAPI__) */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UCHAR3_IMPL_H__ */
