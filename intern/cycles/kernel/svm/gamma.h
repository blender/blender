/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_gamma(ccl_private ShaderData *sd,
                                        ccl_private SVMState *svm,
                                        uint in_gamma,
                                        uint in_color,
                                        uint out_color)
{
  float3 color = stack_load_float3(svm, in_color);
  float gamma = stack_load_float(svm, in_gamma);

  color = svm_math_gamma_color(color, gamma);

  if (stack_valid(out_color)) {
    stack_store_float3(svm, out_color, color);
  }
}

CCL_NAMESPACE_END
