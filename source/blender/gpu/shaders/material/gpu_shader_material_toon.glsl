#ifndef VOLUMETRICS
void node_bsdf_toon(vec4 color, float size, float tsmooth, vec3 N, out Closure result)
{
  node_bsdf_diffuse(color, 0.0, N, result);
}
#else
/* Stub toon because it is not compatible with volumetrics. */
#  define node_bsdf_toon(a, b, c, d, e) (e = CLOSURE_DEFAULT)
#endif
