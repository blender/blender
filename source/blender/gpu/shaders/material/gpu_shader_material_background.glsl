void node_background(vec4 color, float strength, out Closure result)
{
#ifndef VOLUMETRICS
  color *= strength;
  result = CLOSURE_DEFAULT;
  result.radiance = color.rgb;
  result.transmittance = vec3(0.0);
#else
  result = CLOSURE_DEFAULT;
#endif
}
