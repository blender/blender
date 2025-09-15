/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

/**
 * Return the corresponding list index in the `list_start_buf` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest list.
 * Also return the surfel sorting value as `r_ray_distance`.
 */
int surfel_list_index_get(int2 ray_grid_size, float3 P, out float r_ray_distance)
{
  float3 ssP = drw_point_world_to_screen(P);
  r_ray_distance = -ssP.z;
  int2 ray_coord_on_grid = int2(ssP.xy * float2(ray_grid_size));
  ray_coord_on_grid = clamp(ray_coord_on_grid, int2(0), ray_grid_size - 1);

  int list_index = ray_coord_on_grid.y * ray_grid_size.x + ray_coord_on_grid.x;
  return list_index;
}

/**
 * Return the corresponding cluster index in the `cluster_list_tx` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest cluster.
 */
int3 surfel_cluster_index_get(int3 cluster_grid_size,
                              float4x4 irradiance_grid_world_to_local,
                              float3 P)
{
  float3 lP = transform_point(irradiance_grid_world_to_local, P) * 0.5f + 0.5f;
  int3 cluster_index = int3(lP * float3(cluster_grid_size));
  cluster_index = clamp(cluster_index, int3(0), cluster_grid_size - 1);
  return cluster_index;
}
