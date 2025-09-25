/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

void node_composite_difference_matte(
    float4 color, float4 key, float tolerance, float falloff, out float4 result, out float matte)
{
  float difference = dot(abs(color - key).rgb, float3(1.0f)) / 3.0f;

  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.a : safe_divide(max(0.0f, difference - tolerance), falloff);

  matte = min(alpha, color.a);
  result = color * matte;
}
