/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/mapping_util.h"

CCL_NAMESPACE_BEGIN

/* Mapping Node */

ccl_device_noinline void svm_node_mapping(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          uint type,
                                          uint inputs_stack_offsets,
                                          uint result_stack_offset)
{
  uint vector_stack_offset, location_stack_offset, rotation_stack_offset, scale_stack_offset;
  svm_unpack_node_uchar4(inputs_stack_offsets,
                         &vector_stack_offset,
                         &location_stack_offset,
                         &rotation_stack_offset,
                         &scale_stack_offset);

  float3 vector = stack_load_float3(stack, vector_stack_offset);
  float3 location = stack_load_float3(stack, location_stack_offset);
  float3 rotation = stack_load_float3(stack, rotation_stack_offset);
  float3 scale = stack_load_float3(stack, scale_stack_offset);

  float3 result = svm_mapping((NodeMappingType)type, vector, location, rotation, scale);
  stack_store_float3(stack, result_stack_offset, result);
}

/* Texture Mapping */

ccl_device_noinline int svm_node_texture_mapping(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 ccl_private float *stack,
                                                 uint vec_offset,
                                                 uint out_offset,
                                                 int offset)
{
  float3 v = stack_load_float3(stack, vec_offset);

  Transform tfm;
  tfm.x = read_node_float(kg, &offset);
  tfm.y = read_node_float(kg, &offset);
  tfm.z = read_node_float(kg, &offset);

  float3 r = transform_point(&tfm, v);
  stack_store_float3(stack, out_offset, r);
  return offset;
}

ccl_device_noinline int svm_node_min_max(KernelGlobals kg,
                                         ccl_private ShaderData *sd,
                                         ccl_private float *stack,
                                         uint vec_offset,
                                         uint out_offset,
                                         int offset)
{
  float3 v = stack_load_float3(stack, vec_offset);

  float3 mn = float4_to_float3(read_node_float(kg, &offset));
  float3 mx = float4_to_float3(read_node_float(kg, &offset));

  float3 r = min(max(mn, v), mx);
  stack_store_float3(stack, out_offset, r);
  return offset;
}

CCL_NAMESPACE_END
