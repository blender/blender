#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_1(node_subsurface_scattering, Diffuse)

void node_subsurface_scattering(vec4 color,
                                float scale,
                                vec3 radius,
                                float sharpen,
                                float texture_blur,
                                vec3 N,
                                float sss_id,
                                out Closure result)
{
  CLOSURE_VARS_DECLARE_1(Diffuse);

  in_Diffuse_0.N = N; /* Normalized during eval. */
  in_Diffuse_0.albedo = color.rgb;

  CLOSURE_EVAL_FUNCTION_1(node_subsurface_scattering, Diffuse);

  result = CLOSURE_DEFAULT;

  /* Not perfect for texture_blur values between 0.0 and 1.0.
   * Interpolate between separated color and color applied on irradiance. */
  float one_minus_texture_blur = 1.0 - texture_blur;
  vec3 sss_albedo = color.rgb * texture_blur + one_minus_texture_blur;
  vec3 radiance_tint = color.rgb * one_minus_texture_blur + texture_blur;
  /* Consider output radiance as irradiance. */
  out_Diffuse_0.radiance *= radiance_tint;

  closure_load_sss_data(scale, out_Diffuse_0.radiance, sss_albedo, int(sss_id), result);

  /* TODO(fclem) Try to not use this. */
  closure_load_ssr_data(vec3(0.0), 0.0, in_Diffuse_0.N, -1.0, result);
}

#else
/* Stub subsurface scattering because it is not compatible with volumetrics. */
#  define node_subsurface_scattering(a, b, c, d, e, f, g, h) (h = CLOSURE_DEFAULT)
#endif
