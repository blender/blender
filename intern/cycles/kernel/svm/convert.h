/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Conversion Nodes */

ccl_device_noinline void svm_node_convert(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          uint type,
                                          uint from,
                                          uint to)
{
  switch (type) {
    case NODE_CONVERT_FI: {
      float f = stack_load_float(stack, from);
      stack_store_int(stack, to, float_to_int(f));
      break;
    }
    case NODE_CONVERT_FV: {
      float f = stack_load_float(stack, from);
      stack_store_float3(stack, to, make_float3(f, f, f));
      break;
    }
    case NODE_CONVERT_CF: {
      float3 f = stack_load_float3(stack, from);
      float g = linear_rgb_to_gray(kg, f);
      stack_store_float(stack, to, g);
      break;
    }
    case NODE_CONVERT_CI: {
      float3 f = stack_load_float3(stack, from);
      int i = (int)linear_rgb_to_gray(kg, f);
      stack_store_int(stack, to, i);
      break;
    }
    case NODE_CONVERT_VF: {
      float3 f = stack_load_float3(stack, from);
      float g = average(f);
      stack_store_float(stack, to, g);
      break;
    }
    case NODE_CONVERT_VI: {
      float3 f = stack_load_float3(stack, from);
      int i = (int)average(f);
      stack_store_int(stack, to, i);
      break;
    }
    case NODE_CONVERT_IF: {
      float f = (float)stack_load_int(stack, from);
      stack_store_float(stack, to, f);
      break;
    }
    case NODE_CONVERT_IV: {
      float f = (float)stack_load_int(stack, from);
      stack_store_float3(stack, to, make_float3(f, f, f));
      break;
    }
  }
}

CCL_NAMESPACE_END
