/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void node_composite_invert(
    float4 color, float factor, float invert_color, float invert_alpha, out float4 result)
{
  result = color;
  if (invert_color != 0.0f) {
    result.rgb = 1.0f - result.rgb;
  }
  if (invert_alpha != 0.0f) {
    result.a = 1.0f - result.a;
  }
  result = mix(color, result, factor);
}
