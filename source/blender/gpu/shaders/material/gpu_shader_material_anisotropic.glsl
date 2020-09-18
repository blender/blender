#ifndef VOLUMETRICS
void node_bsdf_anisotropic(vec4 color,
                           float roughness,
                           float anisotropy,
                           float rotation,
                           vec3 N,
                           vec3 T,
                           float use_multiscatter,
                           out Closure result)
{
  node_bsdf_glossy(color, roughness, N, -1, use_multiscatter, result);
}
#else
/* Stub anisotropic because it is not compatible with volumetrics. */
#  define node_bsdf_anisotropic(a, b, c, d, e, f, g, result) (result = CLOSURE_DEFAULT)
#endif
