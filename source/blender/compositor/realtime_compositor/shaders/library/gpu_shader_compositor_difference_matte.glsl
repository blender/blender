/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

void node_composite_difference_matte(
    vec4 color, vec4 key, float tolerance, float falloff, out vec4 result, out float matte)
{
  float difference = dot(abs(color - key).rgb, vec3(1.0)) / 3.0;

  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.a : safe_divide(max(0.0, difference - tolerance), falloff);

  matte = min(alpha, color.a);
  result = color * matte;
}
