/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

struct VolumesInfoData {
  /* During object voxelization, we need to use an infinite projection matrix to avoid clipping
   * faces. But they cannot be used for recovering the view position from froxel position as they
   * are not invertible. We store the finite projection matrix and use it for this purpose. */
  float4x4 winmat_finite;
  float4x4 wininv_finite;
  /* Copies of the matrices above but without jittering. Used for re-projection. */
  float4x4 wininv_stable;
  float4x4 winmat_stable;
  /* Previous render sample copy of winmat_stable. */
  float4x4 history_winmat_stable;
  /* Transform from current view space to previous render sample view space. */
  float4x4 curr_view_to_past_view;
  /* Size of the froxel grid texture. */
  packed_int3 tex_size;
  /* Maximum light intensity during volume lighting evaluation. */
  float light_clamp;
  /* Inverse of size of the froxel grid. */
  packed_float3 inv_tex_size;
  /* Maximum light intensity during volume lighting evaluation. */
  float shadow_steps;
  /* 2D scaling factor to make froxel squared. */
  float2 coord_scale;
  /* Extent and inverse extent of the main shading view (render extent, not film extent). */
  float2 main_view_extent;
  float2 main_view_extent_inv;
  /* Size in main view pixels of one froxel in XY. */
  int tile_size;
  /* Hi-Z LOD to use during volume shadow tagging. */
  int tile_size_lod;
  /* Depth to froxel mapping. */
  float depth_near;
  float depth_far;
  float depth_distribution;
  /* Previous render sample copy of the depth mapping parameters. */
  float history_depth_near;
  float history_depth_far;
  float history_depth_distribution;
  /* Amount of history to blend during the scatter phase. */
  float history_opacity;

  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(VolumesInfoData, 16)

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
