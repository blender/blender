/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device float invert(const float color, const float factor)
{
  return factor * (1.0f - color) + (1.0f - factor) * color;
}

ccl_device_noinline void svm_node_invert(ccl_private ShaderData *sd,
                                         ccl_private float *stack,
                                         const uint in_fac,
                                         const uint in_color,
                                         const uint out_color)
{
  const float factor = stack_load_float(stack, in_fac);
  float3 color = stack_load_float3(stack, in_color);

  color.x = invert(color.x, factor);
  color.y = invert(color.y, factor);
  color.z = invert(color.z, factor);

  if (stack_valid(out_color)) {
    stack_store_float3(stack, out_color, color);
  }
}

CCL_NAMESPACE_END
