/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Node */

ccl_device_noinline int svm_node_mix(KernelGlobals kg,
                                     ccl_private ShaderData *sd,
                                     ccl_private float *stack,
                                     uint fac_offset,
                                     uint c1_offset,
                                     uint c2_offset,
                                     int offset)
{
  /* read extra data */
  uint4 node1 = read_node(kg, &offset);

  float fac = stack_load_float(stack, fac_offset);
  float3 c1 = stack_load_float3(stack, c1_offset);
  float3 c2 = stack_load_float3(stack, c2_offset);
  float3 result = svm_mix((NodeMix)node1.y, fac, c1, c2);

  stack_store_float3(stack, node1.z, result);
  return offset;
}

CCL_NAMESPACE_END
