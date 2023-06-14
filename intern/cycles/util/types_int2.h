/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__
struct int2 {
  int x, y;

#  ifndef __KERNEL_GPU__
  __forceinline int operator[](int i) const;
  __forceinline int &operator[](int i);
#  endif
};

ccl_device_inline int2 make_int2(int x, int y);
#endif /* __KERNEL_NATIVE_VECTOR_TYPES__ */

CCL_NAMESPACE_END
