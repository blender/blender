/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void node_composite_invert(float fac, vec4 color, float do_rgb, float do_alpha, out vec4 result)
{
  result = color;
  if (do_rgb != 0.0) {
    result.rgb = 1.0 - result.rgb;
  }
  if (do_alpha != 0.0) {
    result.a = 1.0 - result.a;
  }
  result = mix(color, result, fac);
}
