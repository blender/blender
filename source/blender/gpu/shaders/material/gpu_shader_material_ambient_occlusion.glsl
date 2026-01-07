/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"

[[node]]
void node_ambient_occlusion(float4 color,
                            float dist,
                            float3 normal,
                            const float inverted,
                            const float sample_count,
                            float4 &result_color,
                            float &result_ao)
{
  result_ao = ambient_occlusion_eval(safe_normalize(normal), dist, inverted, sample_count);
  result_color = result_ao * color;
}
