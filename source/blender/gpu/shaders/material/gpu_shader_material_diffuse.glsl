#ifndef VOLUMETRICS
void node_bsdf_diffuse(vec4 color, float roughness, vec3 N, out Closure result)
{
  N = normalize(N);
  result = CLOSURE_DEFAULT;
  eevee_closure_diffuse(N, color.rgb, 1.0, true, result.radiance);
  closure_load_ssr_data(vec3(0.0), 0.0, N, viewCameraVec, -1, result);
  result.radiance *= color.rgb;
}
#else
/* Stub diffuse because it is not compatible with volumetrics. */
#  define node_bsdf_diffuse
#endif
