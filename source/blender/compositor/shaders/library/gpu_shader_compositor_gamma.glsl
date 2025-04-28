/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_lib.glsl"

void node_composite_gamma(float4 color, float gamma, out float4 result)
{
  result = float4(fallback_pow(color.rgb, gamma, color.rgb), color.a);
}
