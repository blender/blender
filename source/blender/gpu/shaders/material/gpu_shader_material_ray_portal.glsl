/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_ray_portal(
    float4 color, float3 position, float3 direction, float weight, out Closure result)
{
  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = color.rgb;
  transparency_data.holdout = 0.0f;

  result = closure_eval(transparency_data);
}
