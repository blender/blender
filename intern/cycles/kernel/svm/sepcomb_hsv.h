/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_noinline int svm_node_combine_hsv(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             uint hue_in,
                                             uint saturation_in,
                                             uint value_in,
                                             int offset)
{
  uint4 node1 = read_node(kg, &offset);
  uint color_out = node1.y;

  float hue = stack_load_float(stack, hue_in);
  float saturation = stack_load_float(stack, saturation_in);
  float value = stack_load_float(stack, value_in);

  /* Combine, and convert back to RGB */
  float3 color = hsv_to_rgb(make_float3(hue, saturation, value));

  if (stack_valid(color_out))
    stack_store_float3(stack, color_out, color);
  return offset;
}

ccl_device_noinline int svm_node_separate_hsv(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              uint color_in,
                                              uint hue_out,
                                              uint saturation_out,
                                              int offset)
{
  uint4 node1 = read_node(kg, &offset);
  uint value_out = node1.y;

  float3 color = stack_load_float3(stack, color_in);

  /* Convert to HSV */
  color = rgb_to_hsv(color);

  if (stack_valid(hue_out))
    stack_store_float(stack, hue_out, color.x);
  if (stack_valid(saturation_out))
    stack_store_float(stack, saturation_out, color.y);
  if (stack_valid(value_out))
    stack_store_float(stack, value_out, color.z);
  return offset;
}

CCL_NAMESPACE_END
