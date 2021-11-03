/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
