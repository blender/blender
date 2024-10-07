/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Clamp Node */

ccl_device_noinline void svm_node_clamp(KernelGlobals kg,
                                        ccl_private ShaderData *sd,
                                        ccl_private SVMState *svm,
                                        uint value_stack_offset,
                                        uint parameters_stack_offsets,
                                        uint result_stack_offset)
{
  uint min_stack_offset, max_stack_offset, type;
  svm_unpack_node_uchar3(parameters_stack_offsets, &min_stack_offset, &max_stack_offset, &type);

  uint4 defaults = read_node(kg, svm);

  float value = stack_load_float(svm, value_stack_offset);
  float min = stack_load_float_default(svm, min_stack_offset, defaults.x);
  float max = stack_load_float_default(svm, max_stack_offset, defaults.y);

  if (type == NODE_CLAMP_RANGE && (min > max)) {
    stack_store_float(svm, result_stack_offset, clamp(value, max, min));
  }
  else {
    stack_store_float(svm, result_stack_offset, clamp(value, min, max));
  }
}

CCL_NAMESPACE_END
