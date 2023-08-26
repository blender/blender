/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

void node_gamma(vec4 col, float gamma, out vec4 outcol)
{
  outcol = col;

  if (col.r > 0.0) {
    outcol.r = compatible_pow(col.r, gamma);
  }
  if (col.g > 0.0) {
    outcol.g = compatible_pow(col.g, gamma);
  }
  if (col.b > 0.0) {
    outcol.b = compatible_pow(col.b, gamma);
  }
}
