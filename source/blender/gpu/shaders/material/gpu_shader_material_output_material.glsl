void node_output_material(
    Closure surface, Closure volume, vec3 displacement, float alpha_threshold, out Closure result)
{
#ifdef VOLUMETRICS
  result = volume;
#else
  result = surface;
#  if defined(USE_ALPHA_HASH)
  /* Alpha clip emulation. */
  if (alpha_threshold >= 0.0) {
    float alpha = saturate(1.0 - avg(result.transmittance));
    result.transmittance = vec3(step(alpha, alpha_threshold));
  }
#  endif
#endif
}
