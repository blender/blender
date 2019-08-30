#ifndef VOLUMETRICS
void node_bsdf_translucent(vec4 color, vec3 N, out Closure result)
{
  node_bsdf_diffuse(color, 0.0, -N, result);
}
#else
/* Stub translucent because it is not compatible with volumetrics. */
#  define node_bsdf_translucent
#endif
