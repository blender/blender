/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/mapping_util.h"

CCL_NAMESPACE_BEGIN

/* Mapping Node */

ccl_device_noinline void svm_node_mapping(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private SVMState *svm,
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

  float3 vector = stack_load_float3(svm, vector_stack_offset);
  float3 location = stack_load_float3(svm, location_stack_offset);
  float3 rotation = stack_load_float3(svm, rotation_stack_offset);
  float3 scale = stack_load_float3(svm, scale_stack_offset);

  float3 result = svm_mapping((NodeMappingType)type, vector, location, rotation, scale);
  stack_store_float3(svm, result_stack_offset, result);
}

/* Texture Mapping */

ccl_device_noinline void svm_node_texture_mapping(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private SVMState *svm,
                                                  uint vec_offset,
                                                  uint out_offset)
{
  float3 v = stack_load_float3(svm, vec_offset);

  Transform tfm;
  tfm.x = read_node_float(kg, svm);
  tfm.y = read_node_float(kg, svm);
  tfm.z = read_node_float(kg, svm);

  float3 r = transform_point(&tfm, v);
  stack_store_float3(svm, out_offset, r);
}

ccl_device_noinline void svm_node_min_max(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private SVMState *svm,
                                          uint vec_offset,
                                          uint out_offset)
{
  float3 v = stack_load_float3(svm, vec_offset);

  float3 mn = float4_to_float3(read_node_float(kg, svm));
  float3 mx = float4_to_float3(read_node_float(kg, svm));

  float3 r = min(max(mn, v), mx);
  stack_store_float3(svm, out_offset, r);
}

CCL_NAMESPACE_END
