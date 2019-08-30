void node_volume_principled(vec4 color,
                            float density,
                            float anisotropy,
                            vec4 absorption_color,
                            float emission_strength,
                            vec4 emission_color,
                            float blackbody_intensity,
                            vec4 blackbody_tint,
                            float temperature,
                            float density_attribute,
                            vec4 color_attribute,
                            float temperature_attribute,
                            sampler1DArray spectrummap,
                            float layer,
                            out Closure result)
{
#ifdef VOLUMETRICS
  vec3 absorption_coeff = vec3(0.0);
  vec3 scatter_coeff = vec3(0.0);
  vec3 emission_coeff = vec3(0.0);

  /* Compute density. */
  density = max(density, 0.0);

  if (density > 1e-5) {
    density = max(density * density_attribute, 0.0);
  }

  if (density > 1e-5) {
    /* Compute scattering and absorption coefficients. */
    vec3 scatter_color = color.rgb * color_attribute.rgb;

    scatter_coeff = scatter_color * density;
    absorption_color.rgb = sqrt(max(absorption_color.rgb, 0.0));
    absorption_coeff = max(1.0 - scatter_color, 0.0) * max(1.0 - absorption_color.rgb, 0.0) *
                       density;
  }

  /* Compute emission. */
  emission_strength = max(emission_strength, 0.0);

  if (emission_strength > 1e-5) {
    emission_coeff += emission_strength * emission_color.rgb;
  }

  if (blackbody_intensity > 1e-3) {
    /* Add temperature from attribute. */
    float T = max(temperature * max(temperature_attribute, 0.0), 0.0);

    /* Stefan-Boltzman law. */
    float T2 = T * T;
    float T4 = T2 * T2;
    float sigma = 5.670373e-8 * 1e-6 / M_PI;
    float intensity = sigma * mix(1.0, T4, blackbody_intensity);

    if (intensity > 1e-5) {
      vec4 bb;
      node_blackbody(T, spectrummap, layer, bb);
      emission_coeff += bb.rgb * blackbody_tint.rgb * intensity;
    }
  }

  result = Closure(absorption_coeff, scatter_coeff, emission_coeff, anisotropy);
#else
  result = CLOSURE_DEFAULT;
#endif
}
