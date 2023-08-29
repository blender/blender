/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

void node_composite_gamma(vec4 color, float gamma, out vec4 result)
{
  result.rgb = fallback_pow(color.rgb, gamma, color.rgb);
  result.a = color.a;
}
