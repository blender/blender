/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_lightprobe_sphere_infos.hh"

/* TODO(fclem): Pass the lightprobe_sphere_buf around and avoid relying on interface.
 * Currently in conflict with eevee_lightprobe_volume_load. */
#ifndef SPHERE_PROBE_SELECT
SHADER_LIBRARY_CREATE_INFO(eevee_lightprobe_sphere_data)
#endif
SHADER_LIBRARY_CREATE_INFO(eevee_lightprobe_planar_data)
/* TODO(fclem): Pass the atlas texture around and avoid relying on interface.
 * Currently in conflict with eevee_lightprobe_volume_load. */
#ifndef IRRADIANCE_GRID_UPLOAD
SHADER_LIBRARY_CREATE_INFO(eevee_volume_probe_data)
#endif

#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/**
 * Returns world position of a volume lightprobe sample (center of cell).
 * Returned position take into account the half voxel padding on each sides.
 * `grid_local_to_world_mat` is the unmodified object matrix.
 * `grid_res` is the un-padded grid resolution.
 * `cell_coord` is the coordinate of the sample in [0..grid_res) range.
 */
float3 lightprobe_volume_grid_sample_position(float4x4 grid_local_to_world_mat,
                                              int3 grid_res,
                                              int3 cell_coord)
{
  float3 ls_cell_pos = (float3(cell_coord + 1)) / float3(grid_res + 1);
  ls_cell_pos = ls_cell_pos * 2.0f - 1.0f;
  float3 ws_cell_pos = (grid_local_to_world_mat * float4(ls_cell_pos, 1.0f)).xyz;
  return ws_cell_pos;
}

/**
 * Return true if sample position is valid.
 * \a r_lP is the local position in grid units [0..grid_size).
 */
bool lightprobe_volume_grid_local_coord(VolumeProbeData grid_data, float3 P, out float3 r_lP)
{
  /* Position in cell units. */
  /* NOTE: The vector-matrix multiplication swapped on purpose to cancel the matrix transpose. */
  float3 lP = (float4(P, 1.0f) * grid_data.world_to_grid_transposed).xyz;
  r_lP = clamp(lP, float3(0.5f), float3(grid_data.grid_size_padded) - 0.5f);
  /* Sample is valid if position wasn't clamped. */
  return all(equal(lP, r_lP));
}

int lightprobe_volume_grid_brick_index_get(VolumeProbeData grid_data, int3 brick_coord)
{
  int3 grid_size_in_bricks = divide_ceil(grid_data.grid_size_padded,
                                         int3(IRRADIANCE_GRID_BRICK_SIZE - 1));
  int brick_index = grid_data.brick_offset;
  brick_index += brick_coord.x;
  brick_index += brick_coord.y * grid_size_in_bricks.x;
  brick_index += brick_coord.z * grid_size_in_bricks.x * grid_size_in_bricks.y;
  return brick_index;
}

/* Return cell corner from a corner ID [0..7]. */
int3 lightprobe_volume_grid_cell_corner(int cell_corner_id)
{
  return (int3(cell_corner_id) >> int3(0, 1, 2)) & 1;
}

float lightprobe_planar_score(PlanarProbeData planar, float3 P, float3 V, float3 L)
{
  float3 lP = float4(P, 1.0f) * planar.world_to_object_transposed;
  if (any(greaterThan(abs(lP), float3(1.0f)))) {
    /* TODO: Transition in Z. Dither? */
    return 0.0f;
  }
  /* Return how much the ray is lined up with the captured ray. */
  float3 R = -reflect(V, planar.normal);
  return saturate(dot(L, R));
}

#ifdef PLANAR_PROBES
/**
 * Return the best planar probe index for a given light direction vector and position.
 */
int lightprobe_planar_select(float3 P, float3 V, float3 L)
{
  /* Initialize to the score of a camera ray. */
  float best_score = saturate(dot(L, -V));
  int best_index = -1;

  for (int index = 0; index < PLANAR_PROBE_MAX; index++) {
    if (probe_planar_buf[index].layer_id == -1) {
      /* PlanarProbeData doesn't contain any gap, exit at first item that is invalid. */
      break;
    }
    float score = lightprobe_planar_score(probe_planar_buf[index], P, V, L);
    if (score > best_score) {
      best_score = score;
      best_index = index;
    }
  }
  return best_index;
}
#endif
