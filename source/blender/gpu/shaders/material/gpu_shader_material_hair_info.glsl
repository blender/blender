
void node_hair_info(float hair_length,
                    out float is_strand,
                    out float intercept,
                    out float length,
                    out float thickness,
                    out vec3 tangent,
                    out float random)
{
  length = hair_length;
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
