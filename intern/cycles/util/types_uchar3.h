/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TYPES_UCHAR3_H__
#define __UTIL_TYPES_UCHAR3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uchar3 {
  uchar x, y, z;

#  ifndef __KERNEL_GPU__
  __forceinline uchar operator[](int i) const;
  __forceinline uchar &operator[](int i);
#  endif
};

ccl_device_inline uchar3 make_uchar3(uchar x, uchar y, uchar z);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UCHAR3_H__ */
