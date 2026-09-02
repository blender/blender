/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_shadow_raycast(const float light_index, float3 position, float softness, float4 &color)
{
  node_shadow_raycast_impl(int(light_index), position, softness, color);
}
