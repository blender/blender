/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/color_util.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_brightness(
    ccl_private ShaderData *sd, ccl_private float *stack, uint in_color, uint out_color, uint node)
{
  uint bright_offset, contrast_offset;
  float3 color = stack_load_float3(stack, in_color);

  svm_unpack_node_uchar2(node, &bright_offset, &contrast_offset);
  float brightness = stack_load_float(stack, bright_offset);
  float contrast = stack_load_float(stack, contrast_offset);

  color = svm_brightness_contrast(color, brightness, contrast);

  if (stack_valid(out_color))
    stack_store_float3(stack, out_color, color);
}

CCL_NAMESPACE_END
