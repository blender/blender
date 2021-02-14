#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_2(node_bsdf_glass, Glossy, Refraction)

void node_bsdf_glass(vec4 color,
                     float roughness,
                     float ior,
                     vec3 N,
                     const float do_multiscatter,
                     const float ssr_id,
                     out Closure result)
{
  CLOSURE_VARS_DECLARE_2(Glossy, Refraction);

  in_Glossy_0.N = N; /* Normalized during eval. */
  in_Glossy_0.roughness = roughness;

  in_Refraction_1.N = N; /* Normalized during eval. */
  in_Refraction_1.roughness = roughness;
  in_Refraction_1.ior = ior;

  CLOSURE_EVAL_FUNCTION_2(node_bsdf_glass, Glossy, Refraction);

  result = CLOSURE_DEFAULT;

  float NV = dot(in_Refraction_1.N, cameraVec);

  float fresnel = (do_multiscatter != 0.0) ?
                      btdf_lut(NV, in_Refraction_1.roughness, in_Refraction_1.ior).y :
                      F_eta(in_Refraction_1.ior, NV);

  vec2 split_sum = brdf_lut(NV, in_Glossy_0.roughness);
  vec3 brdf = (do_multiscatter != 0.0) ? F_brdf_multi_scatter(vec3(1.0), vec3(1.0), split_sum) :
                                         F_brdf_single_scatter(vec3(1.0), vec3(1.0), split_sum);

  out_Glossy_0.radiance = closure_mask_ssr_radiance(out_Glossy_0.radiance, ssr_id);
  out_Glossy_0.radiance *= brdf;
  out_Glossy_0.radiance = render_pass_glossy_mask(vec3(1.0), out_Glossy_0.radiance);
  out_Glossy_0.radiance *= color.rgb * fresnel;
  closure_load_ssr_data(
      out_Glossy_0.radiance, in_Glossy_0.roughness, in_Glossy_0.N, ssr_id, result);

  float btdf = (do_multiscatter != 0.0) ?
                   1.0 :
                   btdf_lut(NV, in_Refraction_1.roughness, in_Refraction_1.ior).x;
  out_Refraction_1.radiance *= btdf;
  out_Refraction_1.radiance = render_pass_glossy_mask(vec3(1.0), out_Refraction_1.radiance);
  out_Refraction_1.radiance *= color.rgb * (1.0 - fresnel);
  /* Simulate 2nd absorption event. */
  out_Refraction_1.radiance *= (refractionDepth > 0.0) ? color.rgb : vec3(1.0);
  result.radiance += out_Refraction_1.radiance;
}

#else
/* Stub glass because it is not compatible with volumetrics. */
#  define node_bsdf_glass(a, b, c, d, e, f, result) (result = CLOSURE_DEFAULT)
#endif
