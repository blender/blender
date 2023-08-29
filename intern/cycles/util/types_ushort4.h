/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TYPES_USHORT4_H__
#define __UTIL_TYPES_USHORT4_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__

struct ushort4 {
  uint16_t x, y, z, w;
};

#endif

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_USHORT4_H__ */
