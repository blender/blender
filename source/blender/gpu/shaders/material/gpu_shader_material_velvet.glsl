#ifndef VOLUMETRICS
void node_bsdf_velvet(vec4 color, float sigma, vec3 N, out Closure result)
{
  node_bsdf_diffuse(color, 0.0, N, result);
}
#else
/* Stub velvet because it is not compatible with volumetrics. */
#  define node_bsdf_velvet(a, b, c, d) (d = CLOSURE_DEFAULT)
#endif
