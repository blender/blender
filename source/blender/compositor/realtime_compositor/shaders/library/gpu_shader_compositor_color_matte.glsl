/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void node_composite_color_matte(vec4 color,
                                vec4 key,
                                float hue_epsilon,
                                float saturation_epsilon,
                                float value_epsilon,
                                out vec4 result,
                                out float matte)

{
  vec4 color_hsva;
  rgb_to_hsv(color, color_hsva);
  vec4 key_hsva;
  rgb_to_hsv(key, key_hsva);

  bool is_within_saturation = distance(color_hsva.y, key_hsva.y) < saturation_epsilon;
  bool is_within_value = distance(color_hsva.z, key_hsva.z) < value_epsilon;
  bool is_within_hue = distance(color_hsva.x, key_hsva.x) < hue_epsilon;
  /* Hue wraps around, so check the distance around the boundary. */
  float min_hue = min(color_hsva.x, key_hsva.x);
  float max_hue = max(color_hsva.x, key_hsva.x);
  is_within_hue = is_within_hue || ((min_hue + (1.0 - max_hue)) < hue_epsilon);

  matte = (is_within_hue && is_within_saturation && is_within_value) ? 0.0 : color.a;
  result = color * matte;
}
