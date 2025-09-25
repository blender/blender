/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void node_eevee_specular(float4 diffuse,
                         float4 specular,
                         float roughness,
                         float4 emissive,
                         float transp,
                         float3 N,
                         float clearcoat,
                         float clearcoat_roughness,
                         float3 CN,
                         float weight,
                         const float use_clearcoat,
                         out Closure result)
{
  diffuse = max(diffuse, float4(0));
  specular = max(specular, float4(0));
  roughness = saturate(roughness);
  emissive = max(emissive, float4(0));
  N = safe_normalize(N);
  clearcoat = saturate(clearcoat);
  clearcoat_roughness = saturate(clearcoat_roughness);
  CN = safe_normalize(CN);

  float3 V = coordinate_incoming(g_data.P);

  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = emissive.rgb;

  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = float3(transp);
  transparency_data.holdout = 0.0f;

  float alpha = (1.0f - transp) * weight;

  ClosureDiffuse diffuse_data;
  diffuse_data.weight = alpha;
  diffuse_data.color = diffuse.rgb;
  diffuse_data.N = N;

  ClosureReflection reflection_data;
  reflection_data.weight = alpha;
  if (true) {
    float NV = dot(N, V);
    float2 split_sum = brdf_lut(NV, roughness);
    float3 brdf = F_brdf_single_scatter(specular.rgb, float3(1.0f), split_sum);

    reflection_data.color = brdf;
    reflection_data.N = N;
    reflection_data.roughness = roughness;
  }

  ClosureReflection clearcoat_data;
  clearcoat_data.weight = alpha * clearcoat * 0.25f;
  if (true) {
    float NV = dot(CN, V);
    float2 split_sum = brdf_lut(NV, clearcoat_roughness);
    float3 brdf = F_brdf_single_scatter(float3(0.04f), float3(1.0f), split_sum);

    clearcoat_data.color = brdf;
    clearcoat_data.N = CN;
    clearcoat_data.roughness = clearcoat_roughness;
  }

  if (use_clearcoat != 0.0f) {
    result = closure_eval(diffuse_data, reflection_data, clearcoat_data);
  }
  else {
    result = closure_eval(diffuse_data, reflection_data);
  }
  Closure emission_cl = closure_eval(emission_data);
  Closure transparency_cl = closure_eval(transparency_data);
  result = closure_add(result, emission_cl);
  result = closure_add(result, transparency_cl);
}
