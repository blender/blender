uniform float backgroundAlpha;

void node_output_world(Closure surface, Closure volume, out Closure result)
{
#ifndef VOLUMETRICS
  float alpha = renderPassEnvironment ? 1.0 : backgroundAlpha;
  result = CLOSURE_DEFAULT;
  result.radiance = surface.radiance * alpha;
  result.transmittance = vec3(0.0);
  result.holdout = (1.0 - alpha);
#else
  result = volume;
#endif /* VOLUMETRICS */
}
