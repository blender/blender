/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void node_composite_distance_matte_rgba(
    float4 color, float4 key, float tolerance, float falloff, out float4 result, out float matte)
{
  float difference = distance(color.rgb, key.rgb);
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.a : max(0.0f, difference - tolerance) / falloff;
  matte = min(alpha, color.a);
  result = color * matte;
}

void node_composite_distance_matte_ycca(
    float4 color, float4 key, float tolerance, float falloff, out float4 result, out float matte)
{
  float4 color_ycca;
  rgba_to_ycca_itu_709(color, color_ycca);
  float4 key_ycca;
  rgba_to_ycca_itu_709(key, key_ycca);

  float difference = distance(color_ycca.yz, key_ycca.yz);
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.a : max(0.0f, difference - tolerance) / falloff;
  matte = min(alpha, color.a);
  result = color * matte;
}
