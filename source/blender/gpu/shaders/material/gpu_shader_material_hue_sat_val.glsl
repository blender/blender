/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void hue_sat(float hue, float sat, float value, float fac, vec4 col, out vec4 outcol)
{
  vec4 hsv;

  rgb_to_hsv(col, hsv);

  hsv[0] = fract(hsv[0] + hue + 0.5);
  hsv[1] = clamp(hsv[1] * sat, 0.0, 1.0);
  hsv[2] = hsv[2] * value;

  hsv_to_rgb(hsv, outcol);

  outcol = mix(col, outcol, fac);
}
