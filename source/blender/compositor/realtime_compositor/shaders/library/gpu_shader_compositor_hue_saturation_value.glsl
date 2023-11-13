/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void node_composite_hue_saturation_value(
    vec4 color, float hue, float saturation, float value, float factor, out vec4 result)
{
  vec4 hsv;
  rgb_to_hsv(color, hsv);

  hsv.x = fract(hsv.x + hue + 0.5);
  hsv.y = hsv.y * saturation;
  hsv.z = hsv.z * value;

  hsv_to_rgb(hsv, result);
  result.rgb = max(result.rgb, vec3(0.0));

  result = mix(color, result, factor);
}
