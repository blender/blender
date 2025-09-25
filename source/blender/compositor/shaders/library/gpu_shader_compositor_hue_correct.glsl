/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

/* Curve maps are stored in texture samplers, so ensure that the parameters evaluate the sampler at
 * the center of the pixels, because samplers are evaluated using linear interpolation. Given the
 * parameter in the [0, 1] range. */
float3 compute_hue_curve_map_coordinates(float3 parameters)
{
  constexpr float sampler_resolution = 257.0f;
  float sampler_offset = 0.5f / sampler_resolution;
  float sampler_scale = 1.0f - (1.0f / sampler_resolution);
  return parameters * sampler_scale + sampler_offset;
}

void node_composite_hue_correct(float4 color,
                                float factor,
                                sampler1DArray curve_map,
                                const float layer,
                                float3 minimums,
                                float3 range_dividers,
                                out float4 result)
{
  float4 hsv;
  rgb_to_hsv(color, hsv);

  /* First, normalize the hue value into the [0, 1] range for each of the curve maps and compute
   * the proper sampler coordinates for interpolation, then adjust each of the Hue, Saturation, and
   * Values accordingly to the following rules. A curve map value of 0.5 means no change in hue,
   * so adjust the value to get an identity at 0.5. Since the identity of addition is 0, we
   * subtract 0.5 (0.5 - 0.5 = 0). A curve map value of 0.5 means no change in saturation or
   * value, so adjust the value to get an identity at 0.5. Since the identity of multiplication is
   * 1, we multiply by 2 (0.5 * 2 = 1). */
  float3 parameters = (hsv.xxx - minimums) * range_dividers;
  float3 coordinates = compute_hue_curve_map_coordinates(parameters);
  hsv.x += texture(curve_map, float2(coordinates.x, layer)).x - 0.5f;
  hsv.y *= texture(curve_map, float2(coordinates.y, layer)).y * 2.0f;
  hsv.z *= texture(curve_map, float2(coordinates.z, layer)).z * 2.0f;

  /* Sanitize the new hue and saturation values. */
  hsv.x = fract(hsv.x);
  hsv.y = clamp(hsv.y, 0.0f, 1.0f);

  hsv_to_rgb(hsv, result);
  result.rgb = max(result.rgb, float3(0.0f));

  result = mix(color, result, factor);
}
