/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/color_util.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_combine_color(KernelGlobals kg,
                                                ccl_private ShaderData *sd,
                                                ccl_private float *stack,
                                                const uint color_type,
                                                const uint inputs_stack_offsets,
                                                const uint result_stack_offset)
{
  uint red_stack_offset;
  uint green_stack_offset;
  uint blue_stack_offset;
  svm_unpack_node_uchar3(
      inputs_stack_offsets, &red_stack_offset, &green_stack_offset, &blue_stack_offset);

  const float r = stack_load_float(stack, red_stack_offset);
  const float g = stack_load_float(stack, green_stack_offset);
  const float b = stack_load_float(stack, blue_stack_offset);

  /* Combine, and convert back to RGB */
  const float3 color = svm_combine_color((NodeCombSepColorType)color_type, make_float3(r, g, b));

  if (stack_valid(result_stack_offset)) {
    stack_store_float3(stack, result_stack_offset, color);
  }
}

ccl_device_noinline void svm_node_separate_color(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 ccl_private float *stack,
                                                 const uint color_type,
                                                 const uint input_stack_offset,
                                                 const uint results_stack_offsets)
{
  float3 color = stack_load_float3(stack, input_stack_offset);

  /* Convert color space */
  color = svm_separate_color((NodeCombSepColorType)color_type, color);

  uint red_stack_offset;
  uint green_stack_offset;
  uint blue_stack_offset;
  svm_unpack_node_uchar3(
      results_stack_offsets, &red_stack_offset, &green_stack_offset, &blue_stack_offset);

  if (stack_valid(red_stack_offset)) {
    stack_store_float(stack, red_stack_offset, color.x);
  }
  if (stack_valid(green_stack_offset)) {
    stack_store_float(stack, green_stack_offset, color.y);
  }
  if (stack_valid(blue_stack_offset)) {
    stack_store_float(stack, blue_stack_offset, color.z);
  }
}

CCL_NAMESPACE_END
