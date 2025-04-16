/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_volume_absorption(float4 color, float density, float weight, out Closure result)
{
  color = max(color, float4(0.0f));
  density = max(density, 0.0f);

  ClosureVolumeAbsorption volume_absorption_data;
  volume_absorption_data.weight = weight;
  volume_absorption_data.absorption = (1.0f - color.rgb) * density;

  result = closure_eval(volume_absorption_data);
}
