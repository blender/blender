/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_difference_matte(
    vec4 color, vec4 key, float tolerance, float falloff, out vec4 result, out float matte)
{
  vec4 difference = abs(color - key);
  float average_difference = (difference.r + difference.g + difference.b) / 3.0;
  bool is_opaque = average_difference > tolerance + falloff;
  float alpha = is_opaque ? color.a : (max(0.0, average_difference - tolerance) / falloff);
  matte = min(alpha, color.a);
  result = color * matte;
}
