void node_volume_scatter(vec4 color, float density, float anisotropy, out Closure result)
{
#ifdef VOLUMETRICS
  result = Closure(vec3(0.0), color.rgb * density, vec3(0.0), anisotropy);
#else
  result = CLOSURE_DEFAULT;
#endif
}
