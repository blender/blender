/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

struct Quaternion {
  float w, x, y, z;
};

ccl_device_inline Quaternion make_quaternion(const float w,
                                             const float x,
                                             const float y,
                                             const float z)
{
  return {w, x, y, z};
}

ccl_device_inline Quaternion identity_quaternion()
{
  return make_quaternion(1.0f, 0.0f, 0.0f, 0.0f);
}

CCL_NAMESPACE_END
