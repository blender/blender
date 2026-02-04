/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void world_normals_get(float3 &N)
{
  N = g_data.N;
}

[[node]]
void world_position_get(out float3 P)
{
  P = g_data.P;
}
