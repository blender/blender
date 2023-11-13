/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void combine_color_rgb(float r, float g, float b, out vec4 col)
{
  col = vec4(r, g, b, 1.0);
}

void combine_color_hsv(float h, float s, float v, out vec4 col)
{
  hsv_to_rgb(vec4(h, s, v, 1.0), col);
}

void combine_color_hsl(float h, float s, float l, out vec4 col)
{
  hsl_to_rgb(vec4(h, s, l, 1.0), col);
}
