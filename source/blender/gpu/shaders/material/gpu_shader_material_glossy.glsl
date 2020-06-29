#ifndef VOLUMETRICS
void node_bsdf_glossy(vec4 color, float roughness, vec3 N, float ssr_id, out Closure result)
{
  N = normalize(N);
  vec3 out_spec, ssr_spec;
  eevee_closure_glossy(
      N, vec3(1.0), vec3(1.0), int(ssr_id), roughness, 1.0, true, out_spec, ssr_spec);
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.radiance = render_pass_glossy_mask(vec3(1.0), out_spec) * color.rgb;
  closure_load_ssr_data(ssr_spec * color.rgb, roughness, N, viewCameraVec, int(ssr_id), result);
}
#else
/* Stub glossy because it is not compatible with volumetrics. */
#  define node_bsdf_glossy(a, b, c, d, e) (e = CLOSURE_DEFAULT)
#endif
