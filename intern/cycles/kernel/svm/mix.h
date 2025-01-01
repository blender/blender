/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/color_util.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Node */

ccl_device_noinline int svm_node_mix(KernelGlobals kg,
                                     ccl_private ShaderData *sd,
                                     ccl_private float *stack,
                                     const uint fac_offset,
                                     const uint c1_offset,
                                     const uint c2_offset,
                                     int offset)
{
  /* read extra data */
  const uint4 node1 = read_node(kg, &offset);

  const float fac = stack_load_float(stack, fac_offset);
  const float3 c1 = stack_load_float3(stack, c1_offset);
  const float3 c2 = stack_load_float3(stack, c2_offset);
  const float3 result = svm_mix_clamped_factor((NodeMix)node1.y, fac, c1, c2);

  stack_store_float3(stack, node1.z, result);
  return offset;
}

ccl_device_noinline void svm_node_mix_color(ccl_private ShaderData *sd,
                                            ccl_private float *stack,
                                            const uint options,
                                            const uint input_offset,
                                            const uint result_offset)
{
  uint use_clamp;
  uint blend_type;
  uint use_clamp_result;
  uint fac_in_stack_offset;
  uint a_in_stack_offset;
  uint b_in_stack_offset;
  svm_unpack_node_uchar3(options, &use_clamp, &blend_type, &use_clamp_result);
  svm_unpack_node_uchar3(
      input_offset, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float t = stack_load_float(stack, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturatef(t);
  }
  const float3 a = stack_load_float3(stack, a_in_stack_offset);
  const float3 b = stack_load_float3(stack, b_in_stack_offset);
  float3 result = svm_mix((NodeMix)blend_type, t, a, b);
  if (use_clamp_result) {
    result = saturate(result);
  }
  stack_store_float3(stack, result_offset, result);
}

ccl_device_noinline void svm_node_mix_float(ccl_private ShaderData *sd,
                                            ccl_private float *stack,
                                            const uint use_clamp,
                                            const uint input_offset,
                                            const uint result_offset)
{
  uint fac_in_stack_offset;
  uint a_in_stack_offset;
  uint b_in_stack_offset;
  svm_unpack_node_uchar3(
      input_offset, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float t = stack_load_float(stack, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturatef(t);
  }
  const float a = stack_load_float(stack, a_in_stack_offset);
  const float b = stack_load_float(stack, b_in_stack_offset);
  const float result = a * (1 - t) + b * t;

  stack_store_float(stack, result_offset, result);
}

ccl_device_noinline void svm_node_mix_vector(ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             const uint input_offset,
                                             const uint result_offset)
{
  uint use_clamp;
  uint fac_in_stack_offset;
  uint a_in_stack_offset;
  uint b_in_stack_offset;
  svm_unpack_node_uchar4(
      input_offset, &use_clamp, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float t = stack_load_float(stack, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturatef(t);
  }
  const float3 a = stack_load_float3(stack, a_in_stack_offset);
  const float3 b = stack_load_float3(stack, b_in_stack_offset);
  const float3 result = a * (one_float3() - t) + b * t;
  stack_store_float3(stack, result_offset, result);
}

ccl_device_noinline void svm_node_mix_vector_non_uniform(ccl_private ShaderData *sd,
                                                         ccl_private float *stack,
                                                         const uint input_offset,
                                                         const uint result_offset)
{
  uint use_clamp;
  uint fac_in_stack_offset;
  uint a_in_stack_offset;
  uint b_in_stack_offset;
  svm_unpack_node_uchar4(
      input_offset, &use_clamp, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float3 t = stack_load_float3(stack, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturate(t);
  }
  const float3 a = stack_load_float3(stack, a_in_stack_offset);
  const float3 b = stack_load_float3(stack, b_in_stack_offset);
  const float3 result = a * (one_float3() - t) + b * t;
  stack_store_float3(stack, result_offset, result);
}

CCL_NAMESPACE_END
