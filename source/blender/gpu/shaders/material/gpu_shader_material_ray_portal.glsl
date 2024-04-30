/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_ray_portal(
    vec4 color, vec3 position, vec3 direction, float weight, out Closure result)
{
  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = color.rgb;
  transparency_data.holdout = 0.0;

  result = closure_eval(transparency_data);
}
