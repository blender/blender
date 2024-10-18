/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void node_composite_color_balance_lgg(
    float factor, vec4 color, vec3 lift, vec3 gamma, vec3 gain, out vec4 result)
{
  vec3 inverse_lift = 2.0 - lift;
  vec3 srgb_color = linear_rgb_to_srgb(color.rgb);
  vec3 lift_balanced = ((srgb_color - 1.0) * inverse_lift) + 1.0;

  vec3 gain_balanced = lift_balanced * gain;
  gain_balanced = max(gain_balanced, vec3(0.0));

  vec3 linear_color = srgb_to_linear_rgb(gain_balanced);
  vec3 sanitized_gamma = mix(gamma, vec3(1e-6), equal(gamma, vec3(0.0)));
  vec3 gamma_balanced = pow(linear_color, 1.0 / sanitized_gamma);

  result = vec4(mix(color.rgb, gamma_balanced, min(factor, 1.0)), color.a);
}

void node_composite_color_balance_asc_cdl(float factor,
                                          vec4 color,
                                          vec3 offset,
                                          vec3 power,
                                          vec3 slope,
                                          float offset_basis,
                                          out vec4 result)
{
  vec3 full_offset = offset + offset_basis;
  vec3 balanced = color.rgb * slope + full_offset;
  balanced = pow(max(balanced, vec3(0.0)), power);
  result = vec4(mix(color.rgb, balanced, min(factor, 1.0)), color.a);
}

void node_composite_color_balance_whitepoint(float factor,
                                             vec4 color,
                                             mat4 matrix,
                                             out vec4 result)
{
  result = mix(color, matrix * color, min(factor, 1.0));
}
