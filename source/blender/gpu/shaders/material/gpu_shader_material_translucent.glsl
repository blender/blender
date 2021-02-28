#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_1(node_bsdf_translucent, Translucent)

void node_bsdf_translucent(vec4 color, vec3 N, out Closure result)
{
  CLOSURE_VARS_DECLARE_1(Translucent);

  in_Translucent_0.N = -N; /* Normalized during eval. */

  CLOSURE_EVAL_FUNCTION_1(node_bsdf_translucent, Translucent);

  result = CLOSURE_DEFAULT;
  closure_load_ssr_data(vec3(0.0), 0.0, -in_Translucent_0.N, -1.0, result);
  result.radiance = render_pass_diffuse_mask(color.rgb, out_Translucent_0.radiance * color.rgb);
}

#else
/* Stub translucent because it is not compatible with volumetrics. */
#  define node_bsdf_translucent(a, b, c) (c = CLOSURE_DEFAULT)
#endif
