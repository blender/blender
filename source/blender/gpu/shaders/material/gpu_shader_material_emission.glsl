/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_emission(vec4 color, float strength, float weight, out Closure result)
{
  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = color.rgb * strength;

  result = closure_eval(emission_data);
}
