#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_1(node_bsdf_refraction, Refraction)

void node_bsdf_refraction(vec4 color, float roughness, float ior, vec3 N, out Closure result)
{
  CLOSURE_VARS_DECLARE_1(Refraction);

  in_Refraction_0.N = N; /* Normalized during eval. */
  in_Refraction_0.roughness = roughness;
  in_Refraction_0.ior = ior;

  CLOSURE_EVAL_FUNCTION_1(node_bsdf_refraction, Refraction);

  result = CLOSURE_DEFAULT;

  out_Refraction_0.radiance = render_pass_glossy_mask(vec3(1.0), out_Refraction_0.radiance);
  out_Refraction_0.radiance *= color.rgb;
  /* Simulate 2nd absorption event. */
  out_Refraction_0.radiance *= (refractionDepth > 0.0) ? color.rgb : vec3(1.0);

  result.radiance = out_Refraction_0.radiance;

  /* TODO(fclem) Try to not use this. */
  result.ssr_normal = normal_encode(mat3(ViewMatrix) * in_Refraction_0.N,
                                    viewCameraVec(viewPosition));
}

#else
/* Stub refraction because it is not compatible with volumetrics. */
#  define node_bsdf_refraction(a, b, c, d, e) (e = CLOSURE_DEFAULT)
#endif
