/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_NATIVE_VECTOR_TYPES__

struct ushort4 {
  uint16_t x, y, z, w;
};

#endif

CCL_NAMESPACE_END
