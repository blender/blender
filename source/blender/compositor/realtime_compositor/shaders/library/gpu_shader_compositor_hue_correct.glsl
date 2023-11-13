/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

/* Curve maps are stored in sampler objects that are evaluated in the [0, 1] range, so normalize
 * parameters accordingly. */
#define NORMALIZE_PARAMETER(parameter, minimum, range) ((parameter - minimum) * range)

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
  hsv.x += texture(curve_map, vec2(hue_parameter, layer)).x - 0.5;

  /* Second, adjust the saturation and value based on the new value of the hue. A curve map value
   * of 0.5 means no change in hue, so adjust the value to get an identity at 0.5. Since the
   * identity of duplication is 1, we multiply by 2 (0.5 * 2 = 1). */
  vec2 parameters = NORMALIZE_PARAMETER(hsv.x, minimums.yz, range_dividers.yz);
  hsv.y *= texture(curve_map, vec2(parameters.x, layer)).y * 2.0;
  hsv.z *= texture(curve_map, vec2(parameters.y, layer)).z * 2.0;

  /* Sanitize the new hue and saturation values. */
  hsv.x = fract(hsv.x);
  hsv.y = clamp(hsv.y, 0.0, 1.0);

  hsv_to_rgb(hsv, result);
  result.rgb = max(result.rgb, vec3(0.0));

  result = mix(color, result, factor);
}
