/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Value Nodes */

ccl_device void svm_node_value_f(ccl_private float *stack,
                                 const uint ivalue,
                                 const uint out_offset,
                                 const bool derivative)
{
  /* Derivative of a constant is zero. */
  stack_store_float(stack, out_offset, dual1(__uint_as_float(ivalue)), derivative);
}

ccl_device int svm_node_value_v(KernelGlobals kg,
                                ccl_private float *stack,
                                const uint out_offset,
                                int offset,
                                const bool derivative)
{
  /* read extra data */
  const uint4 node1 = read_node(kg, &offset);
  const float3 p = make_float3(
      __uint_as_float(node1.y), __uint_as_float(node1.z), __uint_as_float(node1.w));

  /* Derivative of a constant is zero. */
  stack_store_float3(stack, out_offset, dual3(p), derivative);
  return offset;
}

CCL_NAMESPACE_END
