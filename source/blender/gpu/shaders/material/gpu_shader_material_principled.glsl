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
                          float coat,
                          float coat_roughness,
                          float coat_ior,
                          vec4 coat_tint,
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
                          const float do_coat,
                          const float do_refraction,
                          const float do_multiscatter,
                          float do_sss,
                          out Closure result)
{
  /* Match cycles. */
  metallic = clamp(metallic, 0.0, 1.0);
  transmission = clamp(transmission, 0.0, 1.0);
  coat = max(coat, 0.0);
  coat_ior = max(coat_ior, 1.0);

  N = safe_normalize(N);
  CN = safe_normalize(CN);
  vec3 V = cameraVec(g_data.P);
  float NV = dot(N, V);

  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = vec3(1.0 - alpha);
  transparency_data.holdout = 0.0;
  weight *= alpha;

  /* First layer: Sheen */
  /* TODO: Maybe sheen should be specular. */
  vec3 sheen_color = sheen * sheen_tint.rgb * principled_sheen(NV, sheen_roughness);
  ClosureDiffuse diffuse_data;
  diffuse_data.color = weight * sheen_color;
  diffuse_data.N = N;
  /* Attenuate lower layers */
  weight *= (1.0 - max_v3(sheen_color));

  /* Second layer: Coat */
  ClosureReflection coat_data;
  coat_data.N = CN;
  coat_data.roughness = coat_roughness;
  float coat_NV = dot(coat_data.N, V);
  float reflectance = btdf_lut(coat_NV, coat_data.roughness, coat_ior, 0.0).y;
  coat_data.weight = weight * coat * reflectance;
  coat_data.color = vec3(1.0);
  /* Attenuate lower layers */
  weight *= (1.0 - reflectance * coat);

  if (coat == 0) {
    coat_tint.rgb = vec3(1.0);
  }
  else if (!all(equal(coat_tint.rgb, vec3(1.0)))) {
    float coat_neta = 1.0 / coat_ior;
    float NT = fast_sqrt(1.0 - coat_neta * coat_neta * (1 - NV * NV));
    /* Tint lower layers. */
    coat_tint.rgb = pow(coat_tint.rgb, vec3(coat / NT));
  }

  /* Attenuated by sheen and coat. */
  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = coat_tint.rgb * emission.rgb * emission_strength;

  /* Metallic component */
  ClosureReflection reflection_data;
  reflection_data.N = N;
  reflection_data.roughness = roughness;
  vec2 split_sum = brdf_lut(NV, roughness);
  if (true) {
    vec3 f0 = base_color.rgb;
    vec3 f90 = vec3(1.0);
    vec3 metallic_brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(f0, f90, split_sum) :
                                                    F_brdf_single_scatter(f0, f90, split_sum);
    reflection_data.color = weight * metallic * metallic_brdf;
    /* Attenuate lower layers */
    weight *= (1.0 - metallic);
  }

  /* Transmission component */
  ClosureRefraction refraction_data;
  /* TODO: change `specular_tint` to rgb. */
  vec3 reflection_tint = mix(vec3(1.0), base_color.rgb, specular_tint);
  if (true) {
    vec2 bsdf = btdf_lut(NV, roughness, ior, do_multiscatter);

    reflection_data.color += weight * transmission * bsdf.y * reflection_tint;

    refraction_data.weight = weight * transmission * bsdf.x;
    refraction_data.color = base_color.rgb * coat_tint.rgb;
    refraction_data.N = N;
    refraction_data.roughness = roughness;
    refraction_data.ior = ior;
    /* Attenuate lower layers */
    weight *= (1.0 - transmission);
  }

  /* Specular component */
  if (true) {
    vec3 f0 = vec3(F0_from_ior(ior));
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0, 1.33], to avoid harsh
     * transition at `IOR == 1`. */
    vec3 f90 = sqrt(saturate(f0 / 0.02));
    f0 *= 2.0 * specular * reflection_tint;

    vec3 specular_brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(f0, f90, split_sum) :
                                                    F_brdf_single_scatter(f0, f90, split_sum);
    reflection_data.color += weight * specular_brdf;
    /* Attenuate lower layers */
    weight *= (1.0 - max_v3(specular_brdf));
  }

  /* Diffuse component */
  if (true) {
    vec3 diffuse_color = mix(base_color.rgb, subsurface_color.rgb, subsurface);
    diffuse_data.sss_radius = subsurface_radius * subsurface;
    diffuse_data.sss_id = uint(do_sss);
    diffuse_data.color += weight * diffuse_color * coat_tint.rgb;
  }

  /* Adjust the weight of picking the closure. */
  reflection_data.color *= coat_tint.rgb;
  reflection_data.weight = avg(reflection_data.color);
  reflection_data.color *= safe_rcp(reflection_data.weight);

  diffuse_data.weight = avg(diffuse_data.color);
  diffuse_data.color *= safe_rcp(diffuse_data.weight);

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
