/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_set_alpha_apply(float4 color, float alpha, out float4 result)
{
  result = color * alpha;
}

void node_composite_set_alpha_replace(float4 color, float alpha, out float4 result)
{
  result = float4(color.rgb, alpha);
}
