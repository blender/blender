/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_exposure(float4 color, float exposure, out float4 result)
{
  float multiplier = exp2(exposure);
  result = float4(color.rgb * multiplier, color.a);
}
