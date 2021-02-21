#ifndef VOLUMETRICS
vec3 tint_from_color(vec3 color)
{
  float lum = dot(color, vec3(0.3, 0.6, 0.1));  /* luminance approx. */
  return (lum > 0.0) ? color / lum : vec3(0.0); /* normalize lum. to isolate hue+sat */
}

float principled_sheen(float NV)
{
  float f = 1.0 - NV;
  /* Empirical approximation (manual curve fitting). Can be refined. */
  float sheen = f * f * f * 0.077 + f * 0.01 + 0.00026;
  return sheen;
}

CLOSURE_EVAL_FUNCTION_DECLARE_4(node_bsdf_principled, Diffuse, Glossy, Glossy, Refraction)

void node_bsdf_principled(vec4 base_color,
                          float subsurface,
                          vec3 subsurface_radius,
                          vec4 subsurface_color,
                          float metallic,
                          float specular,
                          float specular_tint,
                          float roughness,
                          float anisotropic,
                          float anisotropic_rotation,
                          float sheen,
                          float sheen_tint,
                          float clearcoat,
                          float clearcoat_roughness,
                          float ior,
                          float transmission,
                          float transmission_roughness,
                          vec4 emission,
                          float emission_strength,
                          float alpha,
                          vec3 N,
                          vec3 CN,
                          vec3 T,
                          const float do_diffuse,
                          const float do_clearcoat,
                          const float do_refraction,
                          const float do_multiscatter,
                          float ssr_id,
                          float sss_id,
                          vec3 sss_scale,
                          out Closure result)
{
  /* Match cycles. */
  metallic = saturate(metallic);
  transmission = saturate(transmission);
  float diffuse_weight = (1.0 - transmission) * (1.0 - metallic);
  transmission *= (1.0 - metallic);
  float specular_weight = (1.0 - transmission);
  clearcoat = max(clearcoat, 0.0);
  transmission_roughness = 1.0 - (1.0 - roughness) * (1.0 - transmission_roughness);

  CLOSURE_VARS_DECLARE_4(Diffuse, Glossy, Glossy, Refraction);

  in_Diffuse_0.N = N; /* Normalized during eval. */
  in_Diffuse_0.albedo = mix(base_color.rgb, subsurface_color.rgb, subsurface);

  in_Glossy_1.N = N; /* Normalized during eval. */
  in_Glossy_1.roughness = roughness;

  in_Glossy_2.N = CN; /* Normalized during eval. */
  in_Glossy_2.roughness = clearcoat_roughness;

  in_Refraction_3.N = N; /* Normalized during eval. */
  in_Refraction_3.roughness = do_multiscatter != 0.0 ? roughness : transmission_roughness;
  in_Refraction_3.ior = ior;

  CLOSURE_EVAL_FUNCTION_4(node_bsdf_principled, Diffuse, Glossy, Glossy, Refraction);

  result = CLOSURE_DEFAULT;

  /* This will tag the whole eval for optimisation. */
  if (do_diffuse == 0.0) {
    out_Diffuse_0.radiance = vec3(0);
  }
  if (do_clearcoat == 0.0) {
    out_Glossy_2.radiance = vec3(0);
  }
  if (do_refraction == 0.0) {
    out_Refraction_3.radiance = vec3(0);
  }

  vec3 V = cameraVec(worldPosition);

  /* Glossy_1 will always be evaluated. */
  float NV = dot(in_Glossy_1.N, V);

  vec3 base_color_tint = tint_from_color(base_color.rgb);

  float fresnel = (do_multiscatter != 0.0) ?
                      btdf_lut(NV, in_Glossy_1.roughness, in_Refraction_3.ior).y :
                      F_eta(in_Refraction_3.ior, NV);

  {
    /* Glossy reflections.
     * Separate Glass reflections and main specular reflections to match Cycles renderpasses. */
    out_Glossy_1.radiance = closure_mask_ssr_radiance(out_Glossy_1.radiance, ssr_id);

    vec2 split_sum = brdf_lut(NV, roughness);

    vec3 glossy_radiance_final = vec3(0.0);
    if (transmission > 1e-5) {
      /* Glass Reflection: Reuse radiance from Glossy1. */
      vec3 out_glass_refl_radiance = out_Glossy_1.radiance;

      /* Poor approximation since we baked the LUT using a fixed IOR. */
      vec3 f0 = mix(vec3(1.0), base_color.rgb, specular_tint);
      vec3 f90 = vec3(1);

      vec3 brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(f0, f90, split_sum) :
                                             F_brdf_single_scatter(f0, f90, split_sum);

      out_glass_refl_radiance *= brdf;
      out_glass_refl_radiance = render_pass_glossy_mask(vec3(1), out_glass_refl_radiance);
      out_glass_refl_radiance *= fresnel * transmission;
      glossy_radiance_final += out_glass_refl_radiance;
    }
    if (specular_weight > 1e-5) {
      vec3 dielectric_f0_color = mix(vec3(1.0), base_color_tint, specular_tint);
      vec3 metallic_f0_color = base_color.rgb;
      vec3 f0 = mix((0.08 * specular) * dielectric_f0_color, metallic_f0_color, metallic);
      /* Cycles does this blending using the microfacet fresnel factor. However, our fresnel
       * is already baked inside the split sum LUT. We approximate using by modifying the
       * changing the f90 color directly in a non linear fashion. */
      vec3 f90 = mix(f0, vec3(1), fast_sqrt(specular));

      vec3 brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(f0, f90, split_sum) :
                                             F_brdf_single_scatter(f0, f90, split_sum);

      out_Glossy_1.radiance *= brdf;
      out_Glossy_1.radiance = render_pass_glossy_mask(vec3(1), out_Glossy_1.radiance);
      out_Glossy_1.radiance *= specular_weight;
      glossy_radiance_final += out_Glossy_1.radiance;
    }

    closure_load_ssr_data(
        glossy_radiance_final, in_Glossy_1.roughness, in_Glossy_1.N, ssr_id, result);
  }

  if (diffuse_weight > 1e-5) {
    /* Mask over all diffuse radiance. */
    out_Diffuse_0.radiance *= diffuse_weight;

    /* Sheen Coarse approximation: We reuse the diffuse radiance and just scale it. */
    vec3 sheen_color = mix(vec3(1), base_color_tint, sheen_tint);
    vec3 out_sheen_radiance = out_Diffuse_0.radiance * principled_sheen(NV);
    out_sheen_radiance = render_pass_diffuse_mask(vec3(1), out_sheen_radiance);
    out_sheen_radiance *= sheen * sheen_color;
    result.radiance += out_sheen_radiance;

    /* Diffuse / Subsurface. */
    float scale = avg(sss_scale) * subsurface;
    closure_load_sss_data(scale, out_Diffuse_0.radiance, in_Diffuse_0.albedo, int(sss_id), result);
  }

  if (transmission > 1e-5) {
    float btdf = (do_multiscatter != 0.0) ?
                     1.0 :
                     btdf_lut(NV, in_Refraction_3.roughness, in_Refraction_3.ior).x;
    /* TODO(fclem) This could be going to a transmission render pass instead. */
    out_Refraction_3.radiance *= btdf;
    out_Refraction_3.radiance = render_pass_glossy_mask(vec3(1), out_Refraction_3.radiance);
    out_Refraction_3.radiance *= base_color.rgb;
    /* Simulate 2nd transmission event. */
    out_Refraction_3.radiance *= (refractionDepth > 0.0) ? base_color.rgb : vec3(1);
    out_Refraction_3.radiance *= (1.0 - fresnel) * transmission;
    result.radiance += out_Refraction_3.radiance;
  }

  if (clearcoat > 1e-5) {
    float NV = dot(in_Glossy_2.N, V);
    vec2 split_sum = brdf_lut(NV, in_Glossy_2.roughness);
    vec3 brdf = F_brdf_single_scatter(vec3(0.04), vec3(1.0), split_sum);

    out_Glossy_2.radiance *= brdf * clearcoat * 0.25;
    out_Glossy_2.radiance = render_pass_glossy_mask(vec3(1), out_Glossy_2.radiance);
    result.radiance += out_Glossy_2.radiance;
  }

  {
    vec3 out_emission_radiance = render_pass_emission_mask(emission.rgb);
    out_emission_radiance *= emission_strength;
    result.radiance += out_emission_radiance;
  }

  result.transmittance = vec3(1.0 - alpha);
  result.radiance *= alpha;
  result.ssr_data.rgb *= alpha;
#  ifdef USE_SSS
  result.sss_irradiance *= alpha;
#  endif
}

#else
/* clang-format off */
/* Stub principled because it is not compatible with volumetrics. */
#  define node_bsdf_principled(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, bb, cc, dd, result) (result = CLOSURE_DEFAULT)
/* clang-format on */
#endif
