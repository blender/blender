/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_holdout(float weight, Closure &result)
{
  ClosureTransparency transparency_data;
  transparency_data.transmittance = float3(0.0f);
  transparency_data.holdout = weight;

  result = closure_eval(transparency_data);
}
