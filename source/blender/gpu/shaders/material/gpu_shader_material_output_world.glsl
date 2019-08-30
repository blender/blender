uniform float backgroundAlpha;

void node_output_world(Closure surface, Closure volume, out Closure result)
{
#ifndef VOLUMETRICS
  result.radiance = surface.radiance * backgroundAlpha;
  result.transmittance = vec3(1.0 - backgroundAlpha);
#else
  result = volume;
#endif /* VOLUMETRICS */
}
