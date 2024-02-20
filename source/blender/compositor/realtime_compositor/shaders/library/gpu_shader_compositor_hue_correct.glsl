/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

/* Curve maps are stored in sampler objects that are evaluated in the [0, 1] range, so normalize
 * parameters accordingly. */
#define NORMALIZE_PARAMETER(parameter, minimum, range) ((parameter - minimum) * range)

/* Curve maps are stored in texture samplers, so ensure that the parameters evaluate the sampler at
 * the center of the pixels, because samplers are evaluated using linear interpolation. Given the
 * parameter in the [0, 1] range. */
float compute_curve_map_coordinates(float parameter)
{
  /* Curve maps have a fixed width of 257. We offset by the equivalent of half a pixel and scale
   * down such that the normalized parameter 1.0 corresponds to the center of the last pixel. */
  const float sampler_resolution = 257.0;
  float sampler_offset = 0.5 / sampler_resolution;
  float sampler_scale = 1.0 - (1.0 / sampler_resolution);
  return parameter * sampler_scale + sampler_offset;
}

/* Same as compute_curve_map_coordinates but vectorized. */
vec2 compute_curve_map_coordinates(vec2 parameters)
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

  /* First, adjust the hue channel on its own, since corrections in the saturation and value
   * channels depends on the new value of the hue, not its original value. A curve map value of 0.5
   * means no change in hue, so adjust the value to get an identity at 0.5. Since the identity of
   * addition is 0, we subtract 0.5 (0.5 - 0.5 = 0). */
  const float hue_parameter = NORMALIZE_PARAMETER(hsv.x, minimums.x, range_dividers.x);
  float hue_coordinates = compute_curve_map_coordinates(hue_parameter);
  hsv.x += texture(curve_map, vec2(hue_coordinates, layer)).x - 0.5;

  /* Second, adjust the saturation and value based on the new value of the hue. A curve map value
   * of 0.5 means no change in hue, so adjust the value to get an identity at 0.5. Since the
   * identity of duplication is 1, we multiply by 2 (0.5 * 2 = 1). */
  vec2 parameters = NORMALIZE_PARAMETER(hsv.x, minimums.yz, range_dividers.yz);
  vec2 coordinates = compute_curve_map_coordinates(parameters);
  hsv.y *= texture(curve_map, vec2(coordinates.x, layer)).y * 2.0;
  hsv.z *= texture(curve_map, vec2(coordinates.y, layer)).z * 2.0;

  /* Sanitize the new hue and saturation values. */
  hsv.x = fract(hsv.x);
  hsv.y = clamp(hsv.y, 0.0, 1.0);

  hsv_to_rgb(hsv, result);
  result.rgb = max(result.rgb, vec3(0.0));

  result = mix(color, result, factor);
}
