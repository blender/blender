/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"

void node_bsdf_diffuse(float4 color, float roughness, float3 N, float weight, out Closure result)
{
  ClosureDiffuse diffuse_data;
  diffuse_data.weight = weight;
  diffuse_data.color = color.rgb;
  diffuse_data.N = safe_normalize(N);

  result = closure_eval(diffuse_data);
}
