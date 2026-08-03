/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

[[node]]
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
                         Closure &result)
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
  emission_data.emission = emissive.rgb * weight;

  ClosureTransparency transparency_data;
  transparency_data.transmittance = float3(transp * weight);
  transparency_data.holdout = 0.0f;

  float alpha = (1.0f - transp) * weight;

  [[resource_table]] UtilityTexture &util_tx = resource_table_get(UtilityTexture);

  ClosureDiffuse diffuse_data;
  diffuse_data.color = diffuse.rgb * alpha;
  diffuse_data.N = N;

  ClosureReflection reflection_data;
  {
    float weight = alpha;

    float NV = dot(N, V);
    eevee::lut::GGXBrdfData lut = eevee::lut::GGXBrdfData::sample_utility_tx(
        util_tx, NV, roughness);
    float3 brdf = F_brdf_single_scatter(specular.rgb, float3(1.0f), lut);

    reflection_data.color = brdf * weight;
    reflection_data.N = N;
    reflection_data.roughness = roughness;
  }

  ClosureReflection clearcoat_data;
  {
    float weight = alpha * clearcoat * 0.25f;

    float NV = dot(CN, V);
    eevee::lut::GGXBrdfData lut = eevee::lut::GGXBrdfData::sample_utility_tx(
        util_tx, NV, clearcoat_roughness);
    float3 brdf = F_brdf_single_scatter(float3(0.04f), float3(1.0f), lut);

    clearcoat_data.color = brdf * weight;
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
