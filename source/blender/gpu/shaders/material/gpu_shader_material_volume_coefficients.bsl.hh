/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_volume_coefficients(float weight,
                              float3 absorption_coefficients,
                              float3 scatter_coefficients,
                              float anisotropy,
                              float /*ior*/,
                              float /*backscatter*/,
                              float /*alpha*/,
                              float /*diameter*/,
                              float3 emission_coefficients,
                              Closure &result)
{
  ClosureVolumeScatter volume_scatter_data;
  volume_scatter_data.scattering = scatter_coefficients * weight;
  volume_scatter_data.anisotropy = anisotropy * weight;

  ClosureVolumeAbsorption volume_absorption_data;
  volume_absorption_data.absorption = absorption_coefficients * weight;

  ClosureEmission emission_data;
  emission_data.emission = emission_coefficients * weight;

  result = closure_eval(volume_scatter_data, volume_absorption_data, emission_data);
}
