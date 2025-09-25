/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void node_bsdf_refraction(
    float4 color, float roughness, float ior, float3 N, float weight, out Closure result)
{
  color = max(color, float4(0.0f));
  roughness = saturate(roughness);
  ior = max(ior, 1e-5f);
  N = safe_normalize(N);

  ClosureRefraction refraction_data;
  refraction_data.weight = weight;
  refraction_data.color = color.rgb;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;

  result = closure_eval(refraction_data);
}
