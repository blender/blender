/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_blackbody.glsl"

void node_volume_coefficients(float weight,
                              float3 AbsorptionCoefficients,
                              float3 ScatterCoefficients,
                              float Anisotropy,
                              float IOR,
                              float Backscatter,
                              float Alpha,
                              float Diameter,
                              float3 EmissionCoefficients,
                              out Closure result)
{
  ClosureVolumeScatter volume_scatter_data;
  volume_scatter_data.weight = weight;
  volume_scatter_data.scattering = ScatterCoefficients;
  volume_scatter_data.anisotropy = Anisotropy;

  ClosureVolumeAbsorption volume_absorption_data;
  volume_absorption_data.weight = weight;
  volume_absorption_data.absorption = AbsorptionCoefficients;

  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = EmissionCoefficients;

  result = closure_eval(volume_scatter_data, volume_absorption_data, emission_data);
}
