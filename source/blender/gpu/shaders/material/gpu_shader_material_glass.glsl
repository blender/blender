#ifndef VOLUMETRICS
void node_bsdf_glass(
    vec4 color, float roughness, float ior, vec3 N, float ssr_id, out Closure result)
{
  N = normalize(N);
  vec3 out_spec, out_refr, ssr_spec;
  vec3 refr_color = (refractionDepth > 0.0) ? color.rgb * color.rgb :
                                              color.rgb; /* Simulate 2 transmission event */
  eevee_closure_glass(N,
                      vec3(1.0),
                      vec3(1.0),
                      int(ssr_id),
                      roughness,
                      1.0,
                      ior,
                      true,
                      out_spec,
                      out_refr,
                      ssr_spec);
  float fresnel = F_eta(ior, dot(N, cameraVec));
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = render_pass_glossy_mask(refr_color, out_refr * refr_color) * (1.0 - fresnel);
  result.radiance += render_pass_glossy_mask(color.rgb, out_spec * color.rgb) * fresnel;

  closure_load_ssr_data(
      ssr_spec * color.rgb * fresnel, roughness, N, viewCameraVec, int(ssr_id), result);
}
#else
/* Stub glass because it is not compatible with volumetrics. */
#  define node_bsdf_glass(a, b, c, d, e, f) (f = CLOSURE_DEFAULT)
#endif
