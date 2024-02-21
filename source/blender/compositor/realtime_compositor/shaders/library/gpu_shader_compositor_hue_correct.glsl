/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

/* Curve maps are stored in texture samplers, so ensure that the parameters evaluate the sampler at
 * the center of the pixels, because samplers are evaluated using linear interpolation. Given the
 * parameter in the [0, 1] range. */
vec3 compute_curve_map_coordinates(vec3 parameters)
{
  const float sampler_resolution = 257.0;
  float sampler_offset = 0.5 / sampler_resolution;
  float sampler_scale = 1.0 - (1.0 / sampler_resolution);
  return parameters * sampler_scale + sampler_offset;
}

void node_composite_hue_correct(float factor,
                                vec4 color,
                                sampler1DArray curve_map,
                                const float layer,
                                vec3 minimums,
                                vec3 range_dividers,
                                out vec4 result)
{
  vec4 hsv;
  rgb_to_hsv(color, hsv);

  /* First, normalize the hue value into the [0, 1] range for each of the curve maps and compute
   * the proper sampler coordinates for interpolation, then adjust each of the Hue, Saturation, and
   * Values accordingly to the following rules. A curve map value of 0.5 means no change in hue, so
   * adjust the value to get an identity at 0.5. Since the identity of addition is 0, we subtract
   * 0.5 (0.5 - 0.5 = 0). A curve map value of 0.5 means no change in saturation or value, so
   * adjust the value to get an identity at 0.5. Since the identity of multiplication is 1, we
   * multiply by 2 (0.5 * 2 = 1). */
  vec3 parameters = (hsv.xxx - minimums) * range_dividers;
  vec3 coordinates = compute_curve_map_coordinates(parameters);
  hsv.x += texture(curve_map, vec2(coordinates.x, layer)).x - 0.5;
  hsv.y *= texture(curve_map, vec2(coordinates.y, layer)).y * 2.0;
  hsv.z *= texture(curve_map, vec2(coordinates.z, layer)).z * 2.0;

  /* Sanitize the new hue and saturation values. */
  hsv.x = fract(hsv.x);
  hsv.y = clamp(hsv.y, 0.0, 1.0);

  hsv_to_rgb(hsv, result);
  result.rgb = max(result.rgb, vec3(0.0));

  result = mix(color, result, factor);
}
