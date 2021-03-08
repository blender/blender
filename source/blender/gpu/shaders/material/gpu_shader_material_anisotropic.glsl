#ifndef VOLUMETRICS
void node_bsdf_anisotropic(vec4 color,
                           float roughness,
                           float anisotropy,
                           float rotation,
                           vec3 N,
                           vec3 T,
                           const float use_multiscatter,
                           const float ssr_id,
                           out Closure result)
{
  node_bsdf_glossy(color, roughness, N, use_multiscatter, ssr_id, result);
}
#else
/* Stub anisotropic because it is not compatible with volumetrics. */
#  define node_bsdf_anisotropic(a, b, c, d, e, f, g, h, result) (result = CLOSURE_DEFAULT)
#endif
