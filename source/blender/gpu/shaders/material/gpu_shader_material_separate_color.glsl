/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void separate_color_rgb(vec4 col, out float r, out float g, out float b)
{
  r = col.r;
  g = col.g;
  b = col.b;
}

void separate_color_hsv(vec4 col, out float r, out float g, out float b)
{
  vec4 hsv;

  rgb_to_hsv(col, hsv);
  r = hsv[0];
  g = hsv[1];
  b = hsv[2];
}

void separate_color_hsl(vec4 col, out float r, out float g, out float b)
{
  vec4 hsl;

  rgb_to_hsl(col, hsl);
  r = hsl[0];
  g = hsl[1];
  b = hsl[2];
}
