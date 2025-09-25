/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void node_bsdf_glass(float4 color,
                     float roughness,
                     float ior,
                     float3 N,
                     float weight,
                     float thin_film_thickness,
                     float thin_film_ior,
                     const float do_multiscatter,
                     out Closure result)
{
  color = max(color, float4(0.0f));
  roughness = saturate(roughness);
  ior = max(ior, 1e-5f);
  N = safe_normalize(N);

  float3 V = coordinate_incoming(g_data.P);
  float NV = dot(N, V);

  float2 bsdf = bsdf_lut(NV, roughness, ior, do_multiscatter != 0.0f);

  ClosureReflection reflection_data;
  reflection_data.weight = bsdf.x * weight;
  reflection_data.color = color.rgb;
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  ClosureRefraction refraction_data;
  refraction_data.weight = bsdf.y * weight;
  refraction_data.color = color.rgb;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;

  result = closure_eval(reflection_data, refraction_data);
}
