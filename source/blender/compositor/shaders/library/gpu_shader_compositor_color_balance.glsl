/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void node_composite_color_balance_lgg(
    float factor, float4 color, float3 lift, float3 gamma, float3 gain, out float4 result)
{
  float3 inverse_lift = 2.0f - lift;
  float3 srgb_color = linear_rgb_to_srgb(color.rgb);
  float3 lift_balanced = ((srgb_color - 1.0f) * inverse_lift) + 1.0f;

  float3 gain_balanced = lift_balanced * gain;
  gain_balanced = max(gain_balanced, float3(0.0f));

  float3 linear_color = srgb_to_linear_rgb(gain_balanced);
  float3 gamma_balanced = pow(linear_color, 1.0f / gamma);

  result = float4(mix(color.rgb, gamma_balanced, min(factor, 1.0f)), color.a);
}

void node_composite_color_balance_asc_cdl(
    float factor, float4 color, float3 offset, float3 power, float3 slope, out float4 result)
{
  float3 balanced = color.rgb * slope + offset;
  balanced = pow(max(balanced, float3(0.0f)), power);
  result = float4(mix(color.rgb, balanced, min(factor, 1.0f)), color.a);
}

void node_composite_color_balance_whitepoint(float factor,
                                             float4 color,
                                             float4x4 matrix,
                                             out float4 result)
{
  result = mix(color, matrix * color, min(factor, 1.0f));
}
