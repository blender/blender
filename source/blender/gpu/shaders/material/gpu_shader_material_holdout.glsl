/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_holdout(float weight, out Closure result)
{
  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = float3(0.0f);
  transparency_data.holdout = 1.0f;

  result = closure_eval(transparency_data);
}
