/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/film/write_passes.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline bool svm_node_aov_check(const uint32_t path_flag,
                                          ccl_global float *render_buffer)
{
  bool is_primary = (path_flag & PATH_RAY_TRANSPARENT_BACKGROUND) &&
                    (!(path_flag & PATH_RAY_SINGLE_PASS_DONE));

  return ((render_buffer != NULL) && is_primary);
}

template<uint node_feature_mask, typename ConstIntegratorGenericState>
ccl_device void svm_node_aov_color(KernelGlobals kg,
                                   ConstIntegratorGenericState state,
                                   ccl_private ShaderData *sd,
                                   ccl_private float *stack,
                                   uint4 node,
                                   ccl_global float *render_buffer)
{
  IF_KERNEL_NODES_FEATURE(AOV)
  {
    const float3 val = stack_load_float3(stack, node.y);
    const uint32_t render_pixel_index = INTEGRATOR_STATE(state, path, render_pixel_index);
    const uint64_t render_buffer_offset = (uint64_t)render_pixel_index *
                                          kernel_data.film.pass_stride;
    ccl_global float *buffer = render_buffer + render_buffer_offset +
                               (kernel_data.film.pass_aov_color + node.z);
    kernel_write_pass_float4(buffer, make_float4(val.x, val.y, val.z, 1.0f));
  }
}

template<uint node_feature_mask, typename ConstIntegratorGenericState>
ccl_device void svm_node_aov_value(KernelGlobals kg,
                                   ConstIntegratorGenericState state,
                                   ccl_private ShaderData *sd,
                                   ccl_private float *stack,
                                   uint4 node,
                                   ccl_global float *render_buffer)
{
  IF_KERNEL_NODES_FEATURE(AOV)
  {
    const float val = stack_load_float(stack, node.y);
    const uint32_t render_pixel_index = INTEGRATOR_STATE(state, path, render_pixel_index);
    const uint64_t render_buffer_offset = (uint64_t)render_pixel_index *
                                          kernel_data.film.pass_stride;
    ccl_global float *buffer = render_buffer + render_buffer_offset +
                               (kernel_data.film.pass_aov_value + node.z);
    kernel_write_pass_float(buffer, val);
  }
}
CCL_NAMESPACE_END
