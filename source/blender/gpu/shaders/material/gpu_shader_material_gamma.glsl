/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_safe_lib.glsl"

void node_gamma(float4 col, float gamma, out float4 outcol)
{
  outcol = col;

  if (col.r > 0.0f) {
    outcol.r = compatible_pow(col.r, gamma);
  }
  if (col.g > 0.0f) {
    outcol.g = compatible_pow(col.g, gamma);
  }
  if (col.b > 0.0f) {
    outcol.b = compatible_pow(col.b, gamma);
  }
}
