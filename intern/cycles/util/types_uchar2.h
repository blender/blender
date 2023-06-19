/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct uchar2 {
  uchar x, y;

#  ifndef __KERNEL_GPU__
  __forceinline uchar operator[](int i) const;
  __forceinline uchar &operator[](int i);
#  endif
};

ccl_device_inline uchar2 make_uchar2(uchar x, uchar y);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
