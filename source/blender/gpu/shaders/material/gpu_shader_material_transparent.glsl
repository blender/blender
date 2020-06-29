#ifndef VOLUMETRICS
void node_bsdf_transparent(vec4 color, out Closure result)
{
  result = CLOSURE_DEFAULT;
  result.radiance = vec3(0.0);
  result.transmittance = abs(color.rgb);
}
#else
/* Stub transparent because it is not compatible with volumetrics. */
#  define node_bsdf_transparent(a, b) (b = CLOSURE_DEFAULT)
#endif
