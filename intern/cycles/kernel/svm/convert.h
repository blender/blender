/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

/* Conversion Nodes */

template<typename FloatType, typename Float3Type>
ccl_device_noinline void svm_node_convert(KernelGlobals kg,
                                          ccl_private float *ccl_restrict stack,
                                          const ccl_global SVMNodeConvert &ccl_restrict node)
{

  switch (node.convert_type) {
    case NODE_CONVERT_FI: {
      /* TODO(weizhen): should actually store 0 for int, but none of the nodes that we compute
       * derivatives for has int inputs, so seems fine. */
      const float f = stack_load_float(stack, node.from_offset);
      stack_store_int(stack, node.to_offset, float_to_int(f));
      break;
    }
    case NODE_CONVERT_FV: {
      const FloatType f = stack_load<FloatType>(stack, node.from_offset);
      stack_store(stack, node.to_offset, make_float3(f, f, f));
      break;
    }
    case NODE_CONVERT_CF: {
      const Float3Type f = stack_load<Float3Type>(stack, node.from_offset);
      stack_store(stack, node.to_offset, linear_rgb_to_gray(kg, f));
      break;
    }
    case NODE_CONVERT_CI: {
      const float3 f = stack_load_float3(stack, node.from_offset);
      const int i = (int)linear_rgb_to_gray(kg, f);
      stack_store_int(stack, node.to_offset, i);
      break;
    }
    case NODE_CONVERT_VF: {
      const Float3Type f = stack_load<Float3Type>(stack, node.from_offset);
      stack_store(stack, node.to_offset, average(f));
      break;
    }
    case NODE_CONVERT_VI: {
      const float3 f = stack_load_float3(stack, node.from_offset);
      const int i = (int)average(f);
      stack_store_int(stack, node.to_offset, i);
      break;
    }
    case NODE_CONVERT_IF: {
      const float f = (float)stack_load_int(stack, node.from_offset);
      stack_store(stack, node.to_offset, FloatType(f));
      break;
    }
    case NODE_CONVERT_IV: {
      const float f = (float)stack_load_int(stack, node.from_offset);
      stack_store(stack, node.to_offset, Float3Type(make_float3(f, f, f)));
      break;
    }
    default: {
      kernel_assert(false);
      break;
    }
  }
}

CCL_NAMESPACE_END
