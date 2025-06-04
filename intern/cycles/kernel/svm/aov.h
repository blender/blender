/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/aov_passes.h"

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline bool svm_node_aov_check(const uint32_t path_flag,
                                          const ccl_global float *render_buffer)
{
  const bool is_primary = (path_flag & PATH_RAY_TRANSPARENT_BACKGROUND) &&
                          (!(path_flag & PATH_RAY_SINGLE_PASS_DONE));

  return ((render_buffer != nullptr) && is_primary);
}

template<uint node_feature_mask, typename ConstIntegratorGenericState>
ccl_device void svm_node_aov_color(KernelGlobals kg,
                                   ConstIntegratorGenericState state,
                                   ccl_private ShaderData *sd,
                                   ccl_private float *stack,
                                   const uint4 node,
                                   ccl_global float *render_buffer)
{
  IF_KERNEL_NODES_FEATURE(AOV)
  {
    const float3 val = stack_load_float3(stack, node.y);
    film_write_aov_pass_color(kg, state, render_buffer, node.z, val);
  }
}

template<uint node_feature_mask, typename ConstIntegratorGenericState>
ccl_device void svm_node_aov_value(KernelGlobals kg,
                                   ConstIntegratorGenericState state,
                                   ccl_private ShaderData *sd,
                                   ccl_private float *stack,
                                   const uint4 node,
                                   ccl_global float *render_buffer)
{
  IF_KERNEL_NODES_FEATURE(AOV)
  {
    const float val = stack_load_float(stack, node.y);
    film_write_aov_pass_value(kg, state, render_buffer, node.z, val);
  }
}
CCL_NAMESPACE_END
