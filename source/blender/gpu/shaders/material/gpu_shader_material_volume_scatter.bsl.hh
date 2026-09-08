/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_volume_scatter(float4 color,
                         float density,
                         float anisotropy,
                         float /*ior*/,
                         float /*backscatter*/,
                         float /*alpha*/,
                         float /*diameter*/,
                         float weight,
                         Closure &result)
{
  color = max(color, float4(0.0f));
  density = max(density, 0.0f);

  ClosureVolumeScatter volume_scatter_data;
  volume_scatter_data.scattering = color.rgb * (density * weight);
  volume_scatter_data.anisotropy = anisotropy * weight;

  result = closure_eval(volume_scatter_data);
}
