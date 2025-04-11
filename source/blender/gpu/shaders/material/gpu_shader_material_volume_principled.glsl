/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_blackbody.glsl"

void node_volume_principled(vec4 color,
                            float density,
                            float anisotropy,
                            vec4 absorption_color,
                            float emission_strength,
                            vec4 emission_color,
                            float blackbody_intensity,
                            vec4 blackbody_tint,
                            float temperature,
                            float weight,
                            vec4 density_attribute,
                            vec4 color_attribute,
                            vec4 temperature_attribute,
                            sampler1DArray spectrummap,
                            float layer,
                            out Closure result)
{
  color = max(color, vec4(0.0f));
  density = max(density, 0.0f);
  absorption_color = max(absorption_color, vec4(0.0f));
  emission_strength = max(emission_strength, 0.0f);
  emission_color = max(emission_color, vec4(0.0f));
  blackbody_intensity = max(blackbody_intensity, 0.0f);
  blackbody_tint = max(blackbody_tint, vec4(0.0f));
  temperature = max(temperature, 0.0f);

  vec3 absorption_coeff = vec3(0.0f);
  vec3 scatter_coeff = vec3(0.0f);
  vec3 emission_coeff = vec3(0.0f);

  /* Compute density. */
  if (density > 1e-5f) {
    density = max(density * density_attribute.x, 0.0f);
  }

  if (density > 1e-5f) {
    /* Compute scattering and absorption coefficients. */
    vec3 scatter_color = color.rgb * color_attribute.rgb;

    scatter_coeff = scatter_color * density;
    absorption_color.rgb = sqrt(max(absorption_color.rgb, 0.0f));
    absorption_coeff = max(1.0f - scatter_color, 0.0f) * max(1.0f - absorption_color.rgb, 0.0f) *
                       density;
  }

  /* Compute emission. */
  emission_strength = max(emission_strength, 0.0f);

  if (emission_strength > 1e-5f) {
    emission_coeff += emission_strength * emission_color.rgb;
  }

  if (blackbody_intensity > 1e-3f) {
    /* Add temperature from attribute. */
    float T = max(temperature * max(temperature_attribute.x, 0.0f), 0.0f);

    /* Stefan-Boltzman law. */
    float T2 = T * T;
    float T4 = T2 * T2;
    float sigma = 5.670373e-8f * 1e-6f / M_PI;
    float intensity = sigma * mix(1.0f, T4, blackbody_intensity);

    if (intensity > 1e-5f) {
      vec4 bb;
      node_blackbody(T, spectrummap, layer, bb);
      emission_coeff += bb.rgb * blackbody_tint.rgb * intensity;
    }
  }

  ClosureVolumeScatter volume_scatter_data;
  volume_scatter_data.weight = weight;
  volume_scatter_data.scattering = scatter_coeff;
  volume_scatter_data.anisotropy = anisotropy;

  ClosureVolumeAbsorption volume_absorption_data;
  volume_absorption_data.weight = weight;
  volume_absorption_data.absorption = absorption_coeff;

  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = emission_coeff;

  result = closure_eval(volume_scatter_data, volume_absorption_data, emission_data);
}
