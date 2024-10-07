/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Value Nodes */

ccl_device void svm_node_value_f(KernelGlobals kg,
                                 ccl_private ShaderData *sd,
                                 ccl_private SVMState *svm,
                                 uint ivalue,
                                 uint out_offset)
{
  stack_store_float(svm, out_offset, __uint_as_float(ivalue));
}

ccl_device void svm_node_value_v(KernelGlobals kg,
                                 ccl_private ShaderData *sd,
                                 ccl_private SVMState *svm,
                                 uint out_offset)
{
  /* read extra data */
  uint4 node1 = read_node(kg, svm);
  float3 p = make_float3(
      __uint_as_float(node1.y), __uint_as_float(node1.z), __uint_as_float(node1.w));

  stack_store_float3(svm, out_offset, p);
}

CCL_NAMESPACE_END
