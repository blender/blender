
float wang_hash_noise(uint s)
{
  s = (s ^ 61u) ^ (s >> 16u);
  s *= 9u;
  s = s ^ (s >> 4u);
  s *= 0x27d4eb2du;
  s = s ^ (s >> 15u);

  return fract(float(s) / 4294967296.0);
}

void node_hair_info(out float is_strand,
                    out float intercept,
                    out float thickness,
                    out vec3 tangent,
                    out float random)
{
#ifdef HAIR_SHADER
  is_strand = 1.0;
  intercept = hairTime;
  thickness = hairThickness;
  tangent = normalize(worldNormal);
  random = wang_hash_noise(
      uint(hairStrandID)); /* TODO: could be precomputed per strand instead. */
#else
  is_strand = 0.0;
  intercept = 0.0;
  thickness = 0.0;
  tangent = vec3(1.0);
  random = 0.0;
#endif
}
