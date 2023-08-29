/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec3 tint_from_color(vec3 color)
{
  float lum = dot(color, vec3(0.3, 0.6, 0.1));  /* luminance approx. */
  return (lum > 0.0) ? color / lum : vec3(1.0); /* normalize lum. to isolate hue+sat */
}

float principled_sheen(float NV, float rough)
{
  /* Empirical approximation (manual curve fitting) to the sheen albedo. Can be refined. */
  float den = 35.6694f * rough * rough - 24.4269f * rough * NV - 0.1405f * NV * NV +
              6.1211f * rough + 0.28105f * NV - 0.1405f;
  float num = 58.5299f * rough * rough - 85.0941f * rough * NV + 9.8955f * NV * NV +
              1.9250f * rough + 74.2268f * NV - 0.2246f;
  return saturate(den / num);
}

void node_bsdf_principled(vec4 base_color,
                          float subsurface,
                          vec3 subsurface_radius,
                          vec4 subsurface_color,
                          float subsurface_ior,
                          float subsurface_anisotropy,
                          float metallic,
                          float specular,
                          float specular_tint,
                          float roughness,
                          float anisotropic,
                          float anisotropic_rotation,
                          float sheen,
                          float sheen_roughness,
                          vec4 sheen_tint,
                          float clearcoat,
                          float clearcoat_roughness,
                          float ior,
                          float transmission,
                          vec4 emission,
                          float emission_strength,
                          float alpha,
                          vec3 N,
                          vec3 CN,
                          vec3 T,
                          float weight,
                          const float do_diffuse,
                          const float do_clearcoat,
                          const float do_refraction,
                          const float do_multiscatter,
                          float do_sss,
                          out Closure result)
{
  /* Match cycles. */
  metallic = clamp(metallic, 0.0, 1.0);
  transmission = clamp(transmission, 0.0, 1.0) * (1.0 - metallic);
  float diffuse_weight = (1.0 - transmission) * (1.0 - metallic);
  float specular_weight = (1.0 - transmission);
  float clearcoat_weight = max(clearcoat, 0.0) * 0.25;
  specular = max(0.0, specular);

  N = safe_normalize(N);
  CN = safe_normalize(CN);
  vec3 V = cameraVec(g_data.P);
  float NV = dot(N, V);

  float fresnel = (do_multiscatter != 0.0) ? btdf_lut(NV, roughness, ior).y : F_eta(ior, NV);
  float glass_reflection_weight = fresnel * transmission;
  float glass_transmission_weight = (1.0 - fresnel) * transmission;

  vec3 base_color_tint = tint_from_color(base_color.rgb);

  vec2 split_sum = brdf_lut(NV, roughness);

  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = vec3(1.0 - alpha);
  transparency_data.holdout = 0.0;

  weight *= alpha;

  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = emission.rgb * emission_strength;

  /* Diffuse. */
  ClosureDiffuse diffuse_data;
  diffuse_data.weight = diffuse_weight * weight;
  diffuse_data.color = mix(base_color.rgb, subsurface_color.rgb, subsurface);
  /* Sheen Coarse approximation: We reuse the diffuse radiance and just scale it. */
  diffuse_data.color += sheen * sheen_tint.rgb * principled_sheen(NV, sheen_roughness);
  diffuse_data.N = N;
  diffuse_data.sss_radius = subsurface_radius * subsurface;
  diffuse_data.sss_id = uint(do_sss);

  /* NOTE(@fclem): We need to blend the reflection color but also need to avoid applying the
   * weights so we compute the ratio. */
  float reflection_weight = specular_weight + glass_reflection_weight;
  float reflection_weight_inv = safe_rcp(reflection_weight);
  specular_weight *= reflection_weight_inv;
  glass_reflection_weight *= reflection_weight_inv;

  /* Reflection. */
  ClosureReflection reflection_data;
  reflection_data.weight = reflection_weight * weight;
  reflection_data.N = N;
  reflection_data.roughness = roughness;
  if (true) {
    vec3 dielectric_f0_color = mix(vec3(1.0), base_color_tint, specular_tint);
    vec3 metallic_f0_color = base_color.rgb;
    vec3 f0 = mix((0.08 * specular) * dielectric_f0_color, metallic_f0_color, metallic);
    /* Cycles does this blending using the microfacet fresnel factor. However, our fresnel
     * is already baked inside the split sum LUT. We approximate by changing the f90 color
     * directly in a non linear fashion. */
    vec3 f90 = mix(f0, vec3(1.0), fast_sqrt(specular));

    vec3 reflection_brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(f0, f90, split_sum) :
                                                      F_brdf_single_scatter(f0, f90, split_sum);
    reflection_data.color = reflection_brdf * specular_weight;
  }
  if (true) {
    /* Poor approximation since we baked the LUT using a fixed IOR. */
    vec3 f0 = mix(vec3(1.0), base_color.rgb, specular_tint);
    vec3 f90 = vec3(1.0);

    vec3 glass_brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(f0, f90, split_sum) :
                                                 F_brdf_single_scatter(f0, f90, split_sum);

    /* Avoid 3 glossy evaluation. Use the same closure for glass reflection. */
    reflection_data.color += glass_brdf * glass_reflection_weight;
  }

  ClosureReflection clearcoat_data;
  clearcoat_data.weight = clearcoat_weight * weight;
  clearcoat_data.N = CN;
  clearcoat_data.roughness = clearcoat_roughness;
  if (true) {
    float NV = dot(clearcoat_data.N, V);
    vec2 split_sum = brdf_lut(NV, clearcoat_data.roughness);
    vec3 brdf = F_brdf_single_scatter(vec3(0.04), vec3(1.0), split_sum);
    clearcoat_data.color = brdf;
  }

  /* Refraction. */
  ClosureRefraction refraction_data;
  refraction_data.weight = glass_transmission_weight * weight;
  float btdf = (do_multiscatter != 0.0) ? 1.0 : btdf_lut(NV, roughness, ior).x;

  refraction_data.color = base_color.rgb * btdf;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;

  /* Ref. #98190: Defines are optimizations for old compilers.
   * Might become unnecessary with EEVEE-Next. */
  if (do_diffuse == 0.0 && do_refraction == 0.0 && do_clearcoat != 0.0) {
#ifdef PRINCIPLED_CLEARCOAT
    /* Metallic & Clearcoat case. */
    result = closure_eval(reflection_data, clearcoat_data);
#endif
  }
  else if (do_diffuse == 0.0 && do_refraction == 0.0 && do_clearcoat == 0.0) {
#ifdef PRINCIPLED_METALLIC
    /* Metallic case. */
    result = closure_eval(reflection_data);
#endif
  }
  else if (do_diffuse != 0.0 && do_refraction == 0.0 && do_clearcoat == 0.0) {
#ifdef PRINCIPLED_DIELECTRIC
    /* Dielectric case. */
    result = closure_eval(diffuse_data, reflection_data);
#endif
  }
  else if (do_diffuse == 0.0 && do_refraction != 0.0 && do_clearcoat == 0.0) {
#ifdef PRINCIPLED_GLASS
    /* Glass case. */
    result = closure_eval(reflection_data, refraction_data);
#endif
  }
  else {
#ifdef PRINCIPLED_ANY
    /* Un-optimized case. */
    result = closure_eval(diffuse_data, reflection_data, clearcoat_data, refraction_data);
#endif
  }
  Closure emission_cl = closure_eval(emission_data);
  Closure transparency_cl = closure_eval(transparency_data);
  result = closure_add(result, emission_cl);
  result = closure_add(result, transparency_cl);
}
