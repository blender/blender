/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Conversion Nodes */

ccl_device_noinline void svm_node_convert(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private SVMState *svm,
                                          uint type,
                                          uint from,
                                          uint to)
{
  switch (type) {
    case NODE_CONVERT_FI: {
      float f = stack_load_float(svm, from);
      stack_store_int(svm, to, float_to_int(f));
      break;
    }
    case NODE_CONVERT_FV: {
      float f = stack_load_float(svm, from);
      stack_store_float3(svm, to, make_float3(f, f, f));
      break;
    }
    case NODE_CONVERT_CF: {
      float3 f = stack_load_float3(svm, from);
      float g = linear_rgb_to_gray(kg, f);
      stack_store_float(svm, to, g);
      break;
    }
    case NODE_CONVERT_CI: {
      float3 f = stack_load_float3(svm, from);
      int i = (int)linear_rgb_to_gray(kg, f);
      stack_store_int(svm, to, i);
      break;
    }
    case NODE_CONVERT_VF: {
      float3 f = stack_load_float3(svm, from);
      float g = average(f);
      stack_store_float(svm, to, g);
      break;
    }
    case NODE_CONVERT_VI: {
      float3 f = stack_load_float3(svm, from);
      int i = (int)average(f);
      stack_store_int(svm, to, i);
      break;
    }
    case NODE_CONVERT_IF: {
      float f = (float)stack_load_int(svm, from);
      stack_store_float(svm, to, f);
      break;
    }
    case NODE_CONVERT_IV: {
      float f = (float)stack_load_int(svm, from);
      stack_store_float3(svm, to, make_float3(f, f, f));
      break;
    }
  }
}

CCL_NAMESPACE_END
