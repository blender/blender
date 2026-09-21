/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

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
                         [[resource_table]] KernelGlobals &kg,
                         ShadingData &sd,
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

  float3 V = coordinate_impl(kg, sd, sd.P, sd.N).incoming;

  ClosureEmission emission_data;
  emission_data.emission = emissive.rgb * weight;

  ClosureTransparency transparency_data;
  transparency_data.transmittance = float3(transp * weight);
  transparency_data.holdout = 0.0f;

  float alpha = (1.0f - transp) * weight;

  ClosureDiffuse diffuse_data;
  diffuse_data.color = diffuse.rgb * alpha;
  diffuse_data.N = N;

  ClosureReflection reflection_data;
  {
    float weight = alpha;
    float3 brdf = brdf_lut(kg, specular.rgb, float3(1.0f), dot(N, V), roughness, false);

    reflection_data.color = brdf * weight;
    reflection_data.N = N;
    reflection_data.roughness = roughness;
  }

  ClosureReflection clearcoat_data;
  {
    float weight = alpha * clearcoat * 0.25f;
    float3 brdf = brdf_lut(
        kg, float3(0.04f), float3(1.0f), dot(CN, V), clearcoat_roughness, false);

    clearcoat_data.color = brdf * weight;
    clearcoat_data.N = CN;
    clearcoat_data.roughness = clearcoat_roughness;
  }

  if (use_clearcoat != 0.0f) {
    result = closure_eval(sd, diffuse_data, reflection_data, clearcoat_data);
  }
  else {
    result = closure_eval(sd, diffuse_data, reflection_data);
  }
  Closure emission_cl = closure_eval(sd, emission_data);
  Closure transparency_cl = closure_eval(sd, transparency_data);
  result = closure_add(result, emission_cl);
  result = closure_add(result, transparency_cl);
}
