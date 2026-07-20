/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_blackbody.glsl"

[[node]]
void node_volume_coefficients(float weight,
                              float3 AbsorptionCoefficients,
                              float3 ScatterCoefficients,
                              float Anisotropy,
                              float IOR,
                              float Backscatter,
                              float Alpha,
                              float Diameter,
                              float3 EmissionCoefficients,
                              Closure &result)
{
  ClosureVolumeScatter volume_scatter_data;
  volume_scatter_data.scattering = ScatterCoefficients * weight;
  volume_scatter_data.anisotropy = Anisotropy * weight;

  ClosureVolumeAbsorption volume_absorption_data;
  volume_absorption_data.absorption = AbsorptionCoefficients * weight;

  ClosureEmission emission_data;
  emission_data.emission = EmissionCoefficients * weight;

  result = closure_eval(volume_scatter_data, volume_absorption_data, emission_data);
}
