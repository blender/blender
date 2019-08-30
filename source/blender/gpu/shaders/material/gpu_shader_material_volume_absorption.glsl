void node_volume_absorption(vec4 color, float density, out Closure result)
{
#ifdef VOLUMETRICS
  result = Closure((1.0 - color.rgb) * density, vec3(0.0), vec3(0.0), 0.0);
#else
  result = CLOSURE_DEFAULT;
#endif
}
