/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_normal(KernelGlobals kg,
                                         ccl_private ShaderData *sd,
                                         ccl_private SVMState *svm,
                                         uint in_normal_offset,
                                         uint out_normal_offset,
                                         uint out_dot_offset)
{
  /* read extra data */
  uint4 node1 = read_node(kg, svm);
  float3 normal = stack_load_float3(svm, in_normal_offset);

  float3 direction;
  direction.x = __int_as_float(node1.x);
  direction.y = __int_as_float(node1.y);
  direction.z = __int_as_float(node1.z);
  direction = normalize(direction);

  if (stack_valid(out_normal_offset)) {
    stack_store_float3(svm, out_normal_offset, direction);
  }

  if (stack_valid(out_dot_offset)) {
    stack_store_float(svm, out_dot_offset, dot(direction, normalize(normal)));
  }
}

CCL_NAMESPACE_END
