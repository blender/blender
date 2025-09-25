/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void node_bsdf_glossy(float4 color,
                      float roughness,
                      float anisotropy,
                      float rotation,
                      float3 N,
                      float3 T,
                      float weight,
                      const float do_multiscatter,
                      out Closure result)
{
  color = max(color, float4(0.0f));
  roughness = saturate(roughness);
  N = safe_normalize(N);
  /* anisotropy = clamp(anisotropy, -0.99f, 0.99f) */

  float3 V = coordinate_incoming(g_data.P);
  float NV = dot(N, V);

  float2 split_sum = brdf_lut(NV, roughness);

  ClosureReflection reflection_data;
  reflection_data.weight = weight;
  reflection_data.color = (do_multiscatter != 0.0f) ?
                              F_brdf_multi_scatter(color.rgb, color.rgb, split_sum) :
                              F_brdf_single_scatter(color.rgb, color.rgb, split_sum);
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  result = closure_eval(reflection_data);
}
