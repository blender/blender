#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_1(node_bsdf_diffuse, Diffuse)

void node_bsdf_diffuse(vec4 color, float roughness, vec3 N, out Closure result)
{
  CLOSURE_VARS_DECLARE_1(Diffuse);

  in_Diffuse_0.N = N; /* Normalized during eval. */
  in_Diffuse_0.albedo = color.rgb;

  CLOSURE_EVAL_FUNCTION_1(node_bsdf_diffuse, Diffuse);

  result = CLOSURE_DEFAULT;

  out_Diffuse_0.radiance = render_pass_diffuse_mask(vec3(1.0), out_Diffuse_0.radiance);
  out_Diffuse_0.radiance *= color.rgb;

  result.radiance = out_Diffuse_0.radiance;

  /* TODO(fclem) Try to not use this. */
  closure_load_ssr_data(vec3(0.0), 0.0, in_Diffuse_0.N, -1.0, result);
}

#else
/* Stub diffuse because it is not compatible with volumetrics. */
#  define node_bsdf_diffuse(a, b, c, d) (d = CLOSURE_DEFAULT)
#endif
