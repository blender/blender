/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_fast_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_math.glsl)

vec3 tint_from_color(vec3 color)
{
  float lum = dot(color, vec3(0.3, 0.6, 0.1));  /* luminance approx. */
  return (lum > 0.0) ? color / lum : vec3(1.0); /* normalize lum. to isolate hue+sat */
}

float principled_sheen(float NV, float rough)
{
  /* Empirical approximation (manual curve fitting) to the sheen_weight albedo. Can be refined. */
  float den = 35.6694f * rough * rough - 24.4269f * rough * NV - 0.1405f * NV * NV +
              6.1211f * rough + 0.28105f * NV - 0.1405f;
  float num = 58.5299f * rough * rough - 85.0941f * rough * NV + 9.8955f * NV * NV +
              1.9250f * rough + 74.2268f * NV - 0.2246f;
  return saturate(den / num);
}

float ior_from_F0(float F0)
{
  float f = sqrt(clamp(F0, 0.0, 0.99));
  return (-f - 1.0) / (f - 1.0);
}

void node_bsdf_principled(vec4 base_color,
                          float metallic,
                          float roughness,
                          float ior,
                          float alpha,
                          vec3 N,
                          float weight,
                          float subsurface_weight,
                          vec3 subsurface_radius,
                          float subsurface_scale,
                          float subsurface_ior,
                          float subsurface_anisotropy,
                          float specular_ior_level,
                          vec4 specular_tint,
                          float anisotropic,
                          float anisotropic_rotation,
                          vec3 T,
                          float transmission_weight,
                          float coat_weight,
                          float coat_roughness,
                          float coat_ior,
                          vec4 coat_tint,
                          vec3 CN,
                          float sheen_weight,
                          float sheen_roughness,
                          vec4 sheen_tint,
                          vec4 emission,
                          float emission_strength,
                          float thin_film_thickness,
                          float thin_film_ior,
                          const float do_diffuse,
                          const float do_coat,
                          const float do_refraction,
                          const float do_multiscatter,
                          const float do_sss,
                          out Closure result)
{
  /* Match cycles. */
  metallic = saturate(metallic);
  roughness = saturate(roughness);
  ior = max(ior, 1e-5);
  alpha = saturate(alpha);
  subsurface_weight = saturate(subsurface_weight);
  /* Not used by EEVEE */
  /* subsurface_anisotropy = clamp(subsurface_anisotropy, 0.0, 0.9); */
  /* subsurface_ior = clamp(subsurface_ior, 1.01, 3.8); */
  specular_ior_level = max(specular_ior_level, 0.0);
  specular_tint = max(specular_tint, vec4(0.0));
  /* Not used by EEVEE */
  /* anisotropic = saturate(anisotropic); */
  transmission_weight = saturate(transmission_weight);
  coat_weight = max(coat_weight, 0.0);
  coat_roughness = saturate(coat_roughness);
  coat_ior = max(coat_ior, 1.0);
  coat_tint = max(coat_tint, vec4(0.0));
  sheen_weight = max(sheen_weight, 0.0);
  sheen_roughness = saturate(sheen_roughness);
  sheen_tint = max(sheen_tint, vec4(0.0));

  base_color = max(base_color, vec4(0.0));
  vec4 clamped_base_color = min(base_color, vec4(1.0));

  N = safe_normalize(N);
  CN = safe_normalize(CN);
  vec3 V = coordinate_incoming(g_data.P);
  float NV = dot(N, V);

  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = vec3(1.0 - alpha);
  transparency_data.holdout = 0.0;
  weight *= alpha;

  /* First layer: Sheen */
  vec3 sheen_data_color = vec3(0.0);
  if (sheen_weight > 0.0) {
    /* TODO: Maybe sheen_weight should be specular. */
    vec3 sheen_color = sheen_weight * sheen_tint.rgb * principled_sheen(NV, sheen_roughness);
    sheen_data_color = weight * sheen_color;
    /* Attenuate lower layers */
    weight *= max((1.0 - math_reduce_max(sheen_color)), 0.0);
  }

  /* Second layer: Coat */
  ClosureReflection coat_data;
  coat_data.N = CN;
  coat_data.roughness = coat_roughness;
  coat_data.color = vec3(1.0);

  if (coat_weight > 0.0) {
    float coat_NV = dot(coat_data.N, V);
    float reflectance = bsdf_lut(coat_NV, coat_data.roughness, coat_ior, false).x;
    coat_data.weight = weight * coat_weight * reflectance;
    /* Attenuate lower layers */
    weight *= max((1.0 - reflectance * coat_weight), 0.0);

    if (!all(equal(coat_tint.rgb, vec3(1.0)))) {
      float coat_neta = 1.0 / coat_ior;
      float NT = sqrt_fast(1.0 - coat_neta * coat_neta * (1 - NV * NV));
      /* Tint lower layers. */
      coat_tint.rgb = mix(vec3(1.0), pow(coat_tint.rgb, vec3(1.0 / NT)), saturate(coat_weight));
    }
  }
  else {
    coat_tint.rgb = vec3(1.0);
    coat_data.weight = 0.0;
  }

  /* Attenuated by sheen and coat. */
  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = coat_tint.rgb * emission.rgb * emission_strength;

  /* Metallic component */
  ClosureReflection reflection_data;
  reflection_data.N = N;
  reflection_data.roughness = roughness;
  vec3 reflection_tint = specular_tint.rgb;
  if (metallic > 0.0) {
    vec3 F0 = clamped_base_color.rgb;
    vec3 F82 = min(reflection_tint, vec3(1.0));
    vec3 metallic_brdf;
    brdf_f82_tint_lut(F0, F82, NV, roughness, do_multiscatter != 0.0, metallic_brdf);
    reflection_data.color = weight * metallic * metallic_brdf;
    /* Attenuate lower layers */
    weight *= max((1.0 - metallic), 0.0);
  }
  else {
    reflection_data.color = vec3(0.0);
  }

  /* Transmission component */
  ClosureRefraction refraction_data;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;
  if (transmission_weight > 0.0) {
    vec3 F0 = vec3(F0_from_ior(ior)) * reflection_tint;
    vec3 F90 = vec3(1.0);
    vec3 reflectance, transmittance;
    bsdf_lut(F0,
             F90,
#ifdef GPU_SHADER_EEVEE_LEGACY_DEFINES
             clamped_base_color.rgb,
#else
             sqrt(clamped_base_color.rgb),
#endif
             NV,
             roughness,
             ior,
             do_multiscatter != 0.0,
             reflectance,
             transmittance);

    reflection_data.color += weight * transmission_weight * reflectance;

    refraction_data.weight = weight * transmission_weight;
    refraction_data.color = transmittance * coat_tint.rgb;
    /* Attenuate lower layers */
    weight *= max((1.0 - transmission_weight), 0.0);
  }
  else {
    refraction_data.weight = 0.0;
    refraction_data.color = vec3(0.0);
  }

  /* Specular component */
  if (true) {
    float eta = ior;
    float f0 = F0_from_ior(eta);
    if (specular_ior_level != 0.5) {
      f0 *= 2.0 * specular_ior_level;
      eta = ior_from_F0(f0);
      if (ior < 1.0) {
        eta = 1.0 / eta;
      }
    }

    vec3 F0 = vec3(f0) * reflection_tint;
    F0 = clamp(F0, vec3(0.0), vec3(1.0));
    vec3 F90 = vec3(1.0);
    vec3 reflectance, unused;
    bsdf_lut(F0, F90, vec3(0.0), NV, roughness, eta, do_multiscatter != 0.0, reflectance, unused);

    reflection_data.color += weight * reflectance;
    /* Adjust the weight of picking the closure. */
    reflection_data.color *= coat_tint.rgb;
    reflection_data.weight = math_average(reflection_data.color);
    reflection_data.color *= safe_rcp(reflection_data.weight);

    /* Attenuate lower layers */
    weight *= max((1.0 - math_reduce_max(reflectance)), 0.0);
  }

#ifdef GPU_SHADER_EEVEE_LEGACY_DEFINES
  /* EEVEE Legacy evaluates the subsurface as a diffuse closure.
   * So this has no performance penalty. However, using a separate closure for subsurface
   * (just like for EEVEE-Next) would induce a huge performance hit. */
  ClosureSubsurface diffuse_data;
  /* Flag subsurface as disabled by default. */
  diffuse_data.sss_radius.b = -1.0;
#else
  ClosureDiffuse diffuse_data;
#endif
  diffuse_data.N = N;

  subsurface_radius = max(subsurface_radius * subsurface_scale, vec3(0.0));

  /* Subsurface component */
  if (subsurface_weight > 0.0) {
#ifdef GPU_SHADER_EEVEE_LEGACY_DEFINES
    /* For Legacy EEVEE, Subsurface is just part of the diffuse. Just evaluate the mixed color. */
    /* Subsurface Scattering materials behave unpredictably with values greater than 1.0 in
     * Cycles. So it's clamped there and we clamp here for consistency with Cycles. */
    base_color = mix(base_color, clamped_base_color, subsurface_weight);
    if (do_sss != 0.0) {
      diffuse_data.sss_radius = subsurface_weight * subsurface_radius;
    }
#else
    ClosureSubsurface sss_data;
    sss_data.N = N;
    sss_data.sss_radius = subsurface_radius;
    /* Subsurface Scattering materials behave unpredictably with values greater than 1.0 in
     * Cycles. So it's clamped there and we clamp here for consistency with Cycles. */
    sss_data.color = (subsurface_weight * weight) * clamped_base_color.rgb * coat_tint.rgb;
    /* Add energy of the sheen layer until we have proper sheen BSDF. */
    sss_data.color += sheen_data_color;

    sss_data.weight = math_average(sss_data.color);
    sss_data.color *= safe_rcp(sss_data.weight);
    result = closure_eval(sss_data);

    /* Attenuate lower layers */
    weight *= max((1.0 - subsurface_weight), 0.0);
#endif
  }

  /* Diffuse component */
  if (true) {
    diffuse_data.color = weight * base_color.rgb * coat_tint.rgb;
    /* Add energy of the sheen layer until we have proper sheen BSDF. */
    diffuse_data.color += sheen_data_color;

    diffuse_data.weight = math_average(diffuse_data.color);
    diffuse_data.color *= safe_rcp(diffuse_data.weight);
  }

  /* Ref. #98190: Defines are optimizations for old compilers.
   * Might become unnecessary with EEVEE-Next. */
  if (do_diffuse == 0.0 && do_refraction == 0.0 && do_coat != 0.0) {
#ifdef PRINCIPLED_COAT
    /* Metallic & Coat case. */
    result = closure_eval(reflection_data, coat_data);
#endif
  }
  else if (do_diffuse == 0.0 && do_refraction == 0.0 && do_coat == 0.0) {
#ifdef PRINCIPLED_METALLIC
    /* Metallic case. */
    result = closure_eval(reflection_data);
#endif
  }
  else if (do_diffuse != 0.0 && do_refraction == 0.0 && do_coat == 0.0) {
#ifdef PRINCIPLED_DIELECTRIC
    /* Dielectric case. */
    result = closure_eval(diffuse_data, reflection_data);
#endif
  }
  else if (do_diffuse == 0.0 && do_refraction != 0.0 && do_coat == 0.0) {
#ifdef PRINCIPLED_GLASS
    /* Glass case. */
    result = closure_eval(reflection_data, refraction_data);
#endif
  }
  else {
#ifdef PRINCIPLED_ANY
    /* Un-optimized case. */
    result = closure_eval(diffuse_data, reflection_data, coat_data, refraction_data);
#endif
  }
  Closure emission_cl = closure_eval(emission_data);
  Closure transparency_cl = closure_eval(transparency_data);
  result = closure_add(result, emission_cl);
  result = closure_add(result, transparency_cl);
}
