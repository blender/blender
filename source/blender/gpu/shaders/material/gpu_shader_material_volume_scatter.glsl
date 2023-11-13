/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_volume_scatter(
    vec4 color, float density, float anisotropy, float weight, out Closure result)
{
  ClosureVolumeScatter volume_scatter_data;
  volume_scatter_data.weight = weight;
  volume_scatter_data.scattering = color.rgb * density;
  volume_scatter_data.anisotropy = anisotropy;

  result = closure_eval(volume_scatter_data);
}
