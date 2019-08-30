void node_output_material(Closure surface, Closure volume, vec3 displacement, out Closure result)
{
#ifdef VOLUMETRICS
  result = volume;
#else
  result = surface;
#endif
}
