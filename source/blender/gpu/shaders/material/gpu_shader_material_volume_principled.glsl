/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_blackbody.glsl"

void node_volume_principled(float4 color,
                            float density,
                            float anisotropy,
                            float4 absorption_color,
                            float emission_strength,
                            float4 emission_color,
                            float blackbody_intensity,
                            float4 blackbody_tint,
                            float temperature,
                            float weight,
                            float4 density_attribute,
                            float4 color_attribute,
                            float4 temperature_attribute,
                            sampler1DArray spectrummap,
                            float layer,
                            out Closure result)
{
  color = max(color, float4(0.0f));
  density = max(density, 0.0f);
  absorption_color = max(absorption_color, float4(0.0f));
  emission_strength = max(emission_strength, 0.0f);
  emission_color = max(emission_color, float4(0.0f));
  blackbody_intensity = max(blackbody_intensity, 0.0f);
  blackbody_tint = max(blackbody_tint, float4(0.0f));
  temperature = max(temperature, 0.0f);

  float3 absorption_coeff = float3(0.0f);
  float3 scatter_coeff = float3(0.0f);
  float3 emission_coeff = float3(0.0f);

  /* Compute density. */
  if (density > 1e-5f) {
    density = max(density * density_attribute.x, 0.0f);
  }

  if (density > 1e-5f) {
    /* Compute scattering and absorption coefficients. */
    float3 scatter_color = color.rgb * color_attribute.rgb;

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
      float4 bb;
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
