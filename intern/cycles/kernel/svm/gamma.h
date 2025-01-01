/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/math_util.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_gamma(ccl_private ShaderData *sd,
                                        ccl_private float *stack,
                                        const uint in_gamma,
                                        const uint in_color,
                                        const uint out_color)
{
  float3 color = stack_load_float3(stack, in_color);
  const float gamma = stack_load_float(stack, in_gamma);

  color = svm_math_gamma_color(color, gamma);

  if (stack_valid(out_color)) {
    stack_store_float3(stack, out_color, color);
  }
}

CCL_NAMESPACE_END
