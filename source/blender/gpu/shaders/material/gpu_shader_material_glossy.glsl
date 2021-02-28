#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_1(node_bsdf_glossy, Glossy)

void node_bsdf_glossy(
    vec4 color, float roughness, vec3 N, float use_multiscatter, float ssr_id, out Closure result)
{
  bool do_ssr = (ssrToggle && int(ssr_id) == outputSsrId);

  CLOSURE_VARS_DECLARE_1(Glossy);

  in_Glossy_0.N = N; /* Normalized during eval. */
  in_Glossy_0.roughness = roughness;

  CLOSURE_EVAL_FUNCTION_1(node_bsdf_glossy, Glossy);

  result = CLOSURE_DEFAULT;

  vec2 split_sum = brdf_lut(dot(in_Glossy_0.N, cameraVec(worldPosition)), in_Glossy_0.roughness);
  vec3 brdf = (use_multiscatter != 0.0) ? F_brdf_multi_scatter(vec3(1.0), vec3(1.0), split_sum) :
                                          F_brdf_single_scatter(vec3(1.0), vec3(1.0), split_sum);
  out_Glossy_0.radiance = closure_mask_ssr_radiance(out_Glossy_0.radiance, ssr_id);
  out_Glossy_0.radiance *= brdf;
  out_Glossy_0.radiance = render_pass_glossy_mask(vec3(1.0), out_Glossy_0.radiance);
  out_Glossy_0.radiance *= color.rgb;
  closure_load_ssr_data(
      out_Glossy_0.radiance, in_Glossy_0.roughness, in_Glossy_0.N, ssr_id, result);
}

#else
/* Stub glossy because it is not compatible with volumetrics. */
#  define node_bsdf_glossy(a, b, c, d, e, result) (result = CLOSURE_DEFAULT)
#endif
