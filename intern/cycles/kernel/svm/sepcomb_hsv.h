/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline int svm_node_combine_hsv(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             const uint hue_in,
                                             const uint saturation_in,
                                             const uint value_in,
                                             int offset)
{
  const uint4 node1 = read_node(kg, &offset);
  const uint color_out = node1.y;

  const float hue = stack_load_float(stack, hue_in);
  const float saturation = stack_load_float(stack, saturation_in);
  const float value = stack_load_float(stack, value_in);

  /* Combine, and convert back to RGB */
  const float3 color = hsv_to_rgb(make_float3(hue, saturation, value));

  if (stack_valid(color_out)) {
    stack_store_float3(stack, color_out, color);
  }
  return offset;
}

ccl_device_noinline int svm_node_separate_hsv(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              const uint color_in,
                                              const uint hue_out,
                                              const uint saturation_out,
                                              int offset)
{
  const uint4 node1 = read_node(kg, &offset);
  const uint value_out = node1.y;

  float3 color = stack_load_float3(stack, color_in);

  /* Convert to HSV */
  color = rgb_to_hsv(color);

  if (stack_valid(hue_out)) {
    stack_store_float(stack, hue_out, color.x);
  }
  if (stack_valid(saturation_out)) {
    stack_store_float(stack, saturation_out, color.y);
  }
  if (stack_valid(value_out)) {
    stack_store_float(stack, value_out, color.z);
  }
  return offset;
}

CCL_NAMESPACE_END
