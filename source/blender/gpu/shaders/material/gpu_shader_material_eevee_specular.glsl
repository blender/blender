/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_eevee_specular(vec4 diffuse,
                         vec4 specular,
                         float roughness,
                         vec4 emissive,
                         float transp,
                         vec3 N,
                         float clearcoat,
                         float clearcoat_roughness,
                         vec3 CN,
                         float weight,
                         const float use_clearcoat,
                         out Closure result)
{
  diffuse = max(diffuse, vec4(0));
  specular = max(specular, vec4(0));
  roughness = saturate(roughness);
  emissive = max(emissive, vec4(0));
  N = safe_normalize(N);
  clearcoat = saturate(clearcoat);
  clearcoat_roughness = saturate(clearcoat_roughness);
  CN = safe_normalize(CN);

  vec3 V = coordinate_incoming(g_data.P);

  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = emissive.rgb;

  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = vec3(transp);
  transparency_data.holdout = 0.0;

  float alpha = (1.0 - transp) * weight;

#ifdef GPU_SHADER_EEVEE_LEGACY_DEFINES
  ClosureSubsurface diffuse_data;
  /* Flag subsurface as disabled. */
  diffuse_data.sss_radius.b = -1.0;
#else
  ClosureDiffuse diffuse_data;
#endif
  diffuse_data.weight = alpha;
  diffuse_data.color = diffuse.rgb;
  diffuse_data.N = N;

  ClosureReflection reflection_data;
  reflection_data.weight = alpha;
  if (true) {
    float NV = dot(N, V);
    vec2 split_sum = brdf_lut(NV, roughness);
    vec3 brdf = F_brdf_single_scatter(specular.rgb, vec3(1.0), split_sum);

    reflection_data.color = brdf;
    reflection_data.N = N;
    reflection_data.roughness = roughness;
  }

  ClosureReflection clearcoat_data;
  clearcoat_data.weight = alpha * clearcoat * 0.25;
  if (true) {
    float NV = dot(CN, V);
    vec2 split_sum = brdf_lut(NV, clearcoat_roughness);
    vec3 brdf = F_brdf_single_scatter(vec3(0.04), vec3(1.0), split_sum);

    clearcoat_data.color = brdf;
    clearcoat_data.N = CN;
    clearcoat_data.roughness = clearcoat_roughness;
  }

  if (use_clearcoat != 0.0) {
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
