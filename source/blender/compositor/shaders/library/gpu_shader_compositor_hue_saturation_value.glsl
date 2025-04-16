/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void node_composite_hue_saturation_value(
    float4 color, float hue, float saturation, float value, float factor, out float4 result)
{
  float4 hsv;
  rgb_to_hsv(color, hsv);

  hsv.x = fract(hsv.x + hue + 0.5f);
  hsv.y = hsv.y * saturation;
  hsv.z = hsv.z * value;

  hsv_to_rgb(hsv, result);
  result.rgb = max(result.rgb, float3(0.0f));

  result = mix(color, result, factor);
}
