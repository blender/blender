#ifndef VOLUMETRICS
void node_bsdf_refraction(vec4 color, float roughness, float ior, vec3 N, out Closure result)
{
  N = normalize(N);
  vec3 out_refr;
  color.rgb *= (refractionDepth > 0.0) ? color.rgb : vec3(1.0); /* Simulate 2 absorption event. */
  eevee_closure_refraction(N, roughness, ior, true, out_refr);
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  result.ssr_normal = normal_encode(vN, viewCameraVec);
  result.radiance = render_pass_glossy_mask(color.rgb, out_refr * color.rgb);
}
#else
/* Stub refraction because it is not compatible with volumetrics. */
#  define node_bsdf_refraction(a, b, c, d, e) (e = CLOSURE_DEFAULT)
#endif
