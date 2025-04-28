/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void node_composite_color_matte(float4 color,
                                float4 key,
                                float hue_threshold,
                                float saturation_epsilon,
                                float value_epsilon,
                                out float4 result,
                                out float matte)

{
  float4 color_hsva;
  rgb_to_hsv(color, color_hsva);
  float4 key_hsva;
  rgb_to_hsv(key, key_hsva);

  /* Divide by 2 because the hue wraps around. */
  float hue_epsilon = hue_threshold / 2.0f;

  bool is_within_saturation = distance(color_hsva.y, key_hsva.y) < saturation_epsilon;
  bool is_within_value = distance(color_hsva.z, key_hsva.z) < value_epsilon;
  bool is_within_hue = distance(color_hsva.x, key_hsva.x) < hue_epsilon;
  /* Hue wraps around, so check the distance around the boundary. */
  float min_hue = min(color_hsva.x, key_hsva.x);
  float max_hue = max(color_hsva.x, key_hsva.x);
  is_within_hue = is_within_hue || ((min_hue + (1.0f - max_hue)) < hue_epsilon);

  matte = (is_within_hue && is_within_saturation && is_within_value) ? 0.0f : color.a;
  result = color * matte;
}
