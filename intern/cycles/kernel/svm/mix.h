/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Node */

ccl_device_noinline void svm_node_mix(KernelGlobals kg,
                                      ccl_private ShaderData *sd,
                                      ccl_private SVMState *svm,
                                      uint fac_offset,
                                      uint c1_offset,
                                      uint c2_offset)
{
  /* read extra data */
  uint4 node1 = read_node(kg, svm);

  float fac = stack_load_float(svm, fac_offset);
  float3 c1 = stack_load_float3(svm, c1_offset);
  float3 c2 = stack_load_float3(svm, c2_offset);
  float3 result = svm_mix_clamped_factor((NodeMix)node1.y, fac, c1, c2);

  stack_store_float3(svm, node1.z, result);
}

ccl_device_noinline void svm_node_mix_color(ccl_private ShaderData *sd,
                                            ccl_private SVMState *svm,
                                            uint options,
                                            uint input_offset,
                                            uint result_offset)
{
  uint use_clamp, blend_type, use_clamp_result;
  uint fac_in_stack_offset, a_in_stack_offset, b_in_stack_offset;
  svm_unpack_node_uchar3(options, &use_clamp, &blend_type, &use_clamp_result);
  svm_unpack_node_uchar3(
      input_offset, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float t = stack_load_float(svm, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturatef(t);
  }
  float3 a = stack_load_float3(svm, a_in_stack_offset);
  float3 b = stack_load_float3(svm, b_in_stack_offset);
  float3 result = svm_mix((NodeMix)blend_type, t, a, b);
  if (use_clamp_result) {
    result = saturate(result);
  }
  stack_store_float3(svm, result_offset, result);
}

ccl_device_noinline void svm_node_mix_float(ccl_private ShaderData *sd,
                                            ccl_private SVMState *svm,
                                            uint use_clamp,
                                            uint input_offset,
                                            uint result_offset)
{
  uint fac_in_stack_offset, a_in_stack_offset, b_in_stack_offset;
  svm_unpack_node_uchar3(
      input_offset, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float t = stack_load_float(svm, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturatef(t);
  }
  float a = stack_load_float(svm, a_in_stack_offset);
  float b = stack_load_float(svm, b_in_stack_offset);
  float result = a * (1 - t) + b * t;

  stack_store_float(svm, result_offset, result);
}

ccl_device_noinline void svm_node_mix_vector(ccl_private ShaderData *sd,
                                             ccl_private SVMState *svm,
                                             uint input_offset,
                                             uint result_offset)
{
  uint use_clamp, fac_in_stack_offset, a_in_stack_offset, b_in_stack_offset;
  svm_unpack_node_uchar4(
      input_offset, &use_clamp, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float t = stack_load_float(svm, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturatef(t);
  }
  float3 a = stack_load_float3(svm, a_in_stack_offset);
  float3 b = stack_load_float3(svm, b_in_stack_offset);
  float3 result = a * (one_float3() - t) + b * t;
  stack_store_float3(svm, result_offset, result);
}

ccl_device_noinline void svm_node_mix_vector_non_uniform(ccl_private ShaderData *sd,
                                                         ccl_private SVMState *svm,
                                                         uint input_offset,
                                                         uint result_offset)
{
  uint use_clamp, fac_in_stack_offset, a_in_stack_offset, b_in_stack_offset;
  svm_unpack_node_uchar4(
      input_offset, &use_clamp, &fac_in_stack_offset, &a_in_stack_offset, &b_in_stack_offset);

  float3 t = stack_load_float3(svm, fac_in_stack_offset);
  if (use_clamp > 0) {
    t = saturate(t);
  }
  float3 a = stack_load_float3(svm, a_in_stack_offset);
  float3 b = stack_load_float3(svm, b_in_stack_offset);
  float3 result = a * (one_float3() - t) + b * t;
  stack_store_float3(svm, result_offset, result);
}

CCL_NAMESPACE_END
