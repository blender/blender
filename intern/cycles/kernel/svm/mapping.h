/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/mapping_util.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Mapping Node */

template<typename Float3Type>
ccl_device_noinline void svm_node_mapping(ccl_private float *ccl_restrict stack,
                                          const ccl_global SVMNodeMapping &ccl_restrict node)
{
  const float3 location = stack_load(stack, node.location);
  const float3 rotation = stack_load(stack, node.rotation);
  const float3 scale = stack_load(stack, node.scale);

  const Float3Type vector = stack_load<Float3Type>(stack, node.vector);
  const Float3Type result = svm_mapping(node.mapping_type, vector, location, rotation, scale);
  stack_store(stack, node.result_offset, result);
}

/* Texture Mapping */

template<typename Float3Type>
ccl_device_noinline void svm_node_texture_mapping(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeTextureMapping &ccl_restrict node)
{
  const Float3Type v = stack_load<Float3Type>(stack, node.vec_offset);
  const Transform tfm = make_transform(node.tfm);

  const Float3Type r = transform_point(&tfm, v);
  stack_store(stack, node.out_offset, r);
}

ccl_device_noinline void svm_node_min_max(ccl_private float *ccl_restrict stack,
                                          const ccl_global SVMNodeMinMax &ccl_restrict node)
{
  const float3 v = stack_load_float3(stack, node.vec_offset);

  const float3 mn = node.mn;
  const float3 mx = node.mx;

  const float3 r = min(max(mn, v), mx);
  stack_store_float3(stack, node.out_offset, r);
}

CCL_NAMESPACE_END
