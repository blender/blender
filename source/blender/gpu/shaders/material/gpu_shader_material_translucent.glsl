#ifndef VOLUMETRICS
void node_bsdf_translucent(vec4 color, vec3 N, out Closure result)
{
  N = normalize(N);
  result = CLOSURE_DEFAULT;
  eevee_closure_diffuse(-N, color.rgb, 1.0, false, result.radiance);
  closure_load_ssr_data(vec3(0.0), 0.0, N, viewCameraVec, -1, result);
  result.radiance = render_pass_diffuse_mask(color.rgb, result.radiance * color.rgb);
}
#else
/* Stub translucent because it is not compatible with volumetrics. */
#  define node_bsdf_translucent(a, b, c) (c = CLOSURE_DEFAULT)
#endif
