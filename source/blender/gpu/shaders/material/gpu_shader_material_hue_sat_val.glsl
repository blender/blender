/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void hue_sat(float hue, float sat, float value, float fac, float4 col, out float4 outcol)
{
  float4 hsv;

  rgb_to_hsv(col, hsv);

  hsv[0] = fract(hsv[0] + hue + 0.5f);
  hsv[1] = clamp(hsv[1] * sat, 0.0f, 1.0f);
  hsv[2] = hsv[2] * value;

  hsv_to_rgb(hsv, outcol);

  outcol = mix(col, outcol, fac);
}
