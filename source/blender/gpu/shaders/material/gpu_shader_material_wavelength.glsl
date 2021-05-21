void node_wavelength(float wavelength,
                     sampler1DArray spectrummap,
                     float layer,
                     vec3 xyz_to_r,
                     vec3 xyz_to_g,
                     vec3 xyz_to_b,
                     out vec4 color)
{
  mat3 xyz_to_rgb = mat3(xyz_to_r, xyz_to_g, xyz_to_b);
  float t = (wavelength - 380.0) / (780.0 - 380.0);
  vec3 xyz = texture(spectrummap, vec2(t, layer)).rgb;
  vec3 rgb = xyz * xyz_to_rgb;
  rgb *= 1.0 / 2.52; /* Empirical scale from lg to make all comps <= 1. */
  color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
