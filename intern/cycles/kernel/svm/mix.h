/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/color_util.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Node */

ccl_device_noinline void svm_node_mix(ccl_private float *ccl_restrict stack,
                                      const ccl_global SVMNodeMix &ccl_restrict node)
{
  const float fac = stack_load(stack, node.fac);
  const float3 c1 = stack_load(stack, node.c1);
  const float3 c2 = stack_load(stack, node.c2);
  const float3 result = svm_mix_clamped_factor(node.mix_type, fac, c1, c2);

  stack_store_float3(stack, node.result_offset, result);
}

ccl_device_noinline void svm_node_mix_color(ccl_private float *ccl_restrict stack,
                                            const ccl_global SVMNodeMixColor &ccl_restrict node)
{
  float t = stack_load(stack, node.fac);
  if (node.use_clamp > 0) {
    t = saturatef(t);
  }
  const float3 a = stack_load(stack, node.a);
  const float3 b = stack_load(stack, node.b);
  float3 result = svm_mix(node.blend_type, t, a, b);
  if (node.use_clamp_result) {
    result = saturate(result);
  }
  stack_store_float3(stack, node.result_offset, result);
}

ccl_device_noinline void svm_node_mix_float(ccl_private float *ccl_restrict stack,
                                            const ccl_global SVMNodeMixFloat &ccl_restrict node)
{
  float t = stack_load(stack, node.fac);
  if (node.use_clamp > 0) {
    t = saturatef(t);
  }
  const float a = stack_load(stack, node.a);
  const float b = stack_load(stack, node.b);
  const float result = endvalue_preserving_mix(a, b, t);

  stack_store_float(stack, node.result_offset, result);
}

ccl_device_noinline void svm_node_mix_vector(ccl_private float *ccl_restrict stack,
                                             const ccl_global SVMNodeMixVector &ccl_restrict node)
{
  float t = stack_load(stack, node.fac);
  if (node.use_clamp > 0) {
    t = saturatef(t);
  }
  const float3 a = stack_load(stack, node.a);
  const float3 b = stack_load(stack, node.b);
  const float3 result = endvalue_preserving_mix(a, b, t);
  stack_store_float3(stack, node.result_offset, result);
}

ccl_device_noinline void svm_node_mix_vector_non_uniform(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeMixVectorNonUniform &ccl_restrict node)
{
  float3 t = stack_load(stack, node.fac);
  if (node.use_clamp > 0) {
    t = saturate(t);
  }
  const float3 a = stack_load(stack, node.a);
  const float3 b = stack_load(stack, node.b);
  const float3 result = endvalue_preserving_mix(a, b, t);
  stack_store_float3(stack, node.result_offset, result);
}

CCL_NAMESPACE_END
