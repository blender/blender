/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_depth_of_field_lib.bsl.hh"

namespace eevee::dof {

struct Tiles {
  [[image(0, read, UFLOAT_11_11_10)]] image2D in_tiles_fg_img;
  [[image(1, read, UFLOAT_11_11_10)]] image2D in_tiles_bg_img;
};

struct TilesWrite {
  [[image(2, write, UFLOAT_11_11_10)]] image2D out_tiles_fg_img;
  [[image(3, write, UFLOAT_11_11_10)]] image2D out_tiles_bg_img;
};

/**
 * Tile flatten pass: Takes the half-resolution CoC buffer and converts it to 8x8 tiles.
 *
 * Output min and max values for each tile and for both foreground & background.
 * Also outputs min intersectable CoC for the background, which is the minimum CoC
 * that comes from the background pixels.
 *
 * Input:
 * - Half-resolution Circle of confusion. Out of setup pass.
 * Output:
 * - Separated foreground and background CoC. 1/8th of half-res resolution. So 1/16th of full-res.
 */
namespace tile_flatten {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[sampler(0)]] sampler2D coc_tx;

  /**
   * In order to use atomic operations, we have to use uints. But this means having to deal with
   * the negative number ourselves. Luckily, each ground have a nicely defined range of values we
   * can remap to positive float.
   */
  [[shared]] uint fg_min_coc;
  [[shared]] uint fg_max_coc;
  [[shared]] uint fg_max_intersectable_coc;
  [[shared]] uint bg_min_coc;
  [[shared]] uint bg_max_coc;
  [[shared]] uint bg_min_intersectable_coc;
};

#define dof_tile_large_coc_uint floatBitsToUint(dof_tile_large_coc)

[[compute, local_size(DOF_TILES_FLATTEN_GROUP_SIZE, DOF_TILES_FLATTEN_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[resource_table]] TilesWrite &tiles,
               [[work_group_id]] const uint3 group_id,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id,
               [[local_invocation_index]] const uint local_index)
{
  if (local_index == 0u) {
    /* NOTE: Min/Max flipped because of inverted fg_coc sign. */
    srt.fg_min_coc = floatBitsToUint(0.0f);
    srt.fg_max_coc = dof_tile_large_coc_uint;
    srt.fg_max_intersectable_coc = dof_tile_large_coc_uint;
    srt.bg_min_coc = dof_tile_large_coc_uint;
    srt.bg_max_coc = floatBitsToUint(0.0f);
    srt.bg_min_intersectable_coc = dof_tile_large_coc_uint;
  }
  barrier();

  int2 sample_texel = min(int2(global_id.xy), textureSize(srt.coc_tx, 0).xy - 1);
  float2 sample_data = texelFetch(srt.coc_tx, sample_texel, 0).rg;

  float sample_coc = sample_data.x;
  uint fg_coc = floatBitsToUint(max(-sample_coc, 0.0f));
  /* NOTE: atomicMin/Max flipped because of inverted fg_coc sign. */
  atomicMax(srt.fg_min_coc, fg_coc);
  atomicMin(srt.fg_max_coc, fg_coc);
  atomicMin(srt.fg_max_intersectable_coc, (sample_coc < 0.0f) ? fg_coc : dof_tile_large_coc_uint);

  uint bg_coc = floatBitsToUint(max(sample_coc, 0.0f));
  atomicMin(srt.bg_min_coc, bg_coc);
  atomicMax(srt.bg_max_coc, bg_coc);
  atomicMin(srt.bg_min_intersectable_coc, (sample_coc > 0.0f) ? bg_coc : dof_tile_large_coc_uint);

  barrier();

  if (local_index == 0u) {
    if (srt.fg_max_intersectable_coc == dof_tile_large_coc_uint) {
      srt.fg_max_intersectable_coc = floatBitsToUint(0.0f);
    }

    CocTile tile;
    /* Foreground sign is flipped since we compare unsigned representation. */
    tile.fg_min_coc = -uintBitsToFloat(srt.fg_min_coc);
    tile.fg_max_coc = -uintBitsToFloat(srt.fg_max_coc);
    tile.fg_max_intersectable_coc = -uintBitsToFloat(srt.fg_max_intersectable_coc);
    tile.bg_min_coc = uintBitsToFloat(srt.bg_min_coc);
    tile.bg_max_coc = uintBitsToFloat(srt.bg_max_coc);
    tile.bg_min_intersectable_coc = uintBitsToFloat(srt.bg_min_intersectable_coc);

    int2 tile_co = int2(group_id.xy);
    dof_coc_tile_store(tiles.out_tiles_fg_img, tiles.out_tiles_bg_img, tile_co, tile);
  }
}

}  // namespace tile_flatten

/**
 * Tile dilate pass: Takes the 8x8 Tiles buffer and converts dilates the tiles with large CoC to
 * their neighborhood. This pass is repeated multiple time until the maximum CoC can be covered.
 *
 * Input & Output:
 * - Separated foreground and background CoC. 1/8th of half-res resolution. So 1/16th of full-res.
 */
namespace tile_dilate {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[compilation_constant]] const bool dilate_mode_min_max;

  [[push_constant]] const int ring_count;
  [[push_constant]] const int ring_width_multiplier;
};

[[compute, local_size(DOF_TILES_DILATE_GROUP_SIZE, DOF_TILES_DILATE_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[resource_table]] Tiles &in_tiles,
               [[resource_table]] TilesWrite &out_tiles,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id,
               [[local_invocation_index]] const uint local_index)
{
  int2 center_tile_pos = int2(global_id.xy);

  /* Error introduced by the random offset of the gathering kernel's center. */
  constexpr float bluring_radius_error = 1.0f + 1.0f / (float(DOF_GATHER_RING_COUNT) + 0.5f);
  constexpr float tile_to_fullres_factor = float(DOF_TILES_SIZE * 2);

  CocTile ring_buckets[DOF_DILATE_RING_COUNT];

  for (int ring = 0; ring < srt.ring_count && ring < DOF_DILATE_RING_COUNT; ring++) {
    ring_buckets[ring] = dof_coc_tile_init();

    int ring_distance = ring + 1;
    for (int sample_id = 0; sample_id < 4 * ring_distance; sample_id++) {
      int2 offset = dof_square_ring_sample_offset(ring_distance, sample_id);

      offset *= srt.ring_width_multiplier;

      for (int i = 0; i < 2; i++) {
        int2 adj_tile_pos = center_tile_pos + ((i == 0) ? offset : -offset);

        CocTile adj_tile = dof_coc_tile_load(
            in_tiles.in_tiles_fg_img, in_tiles.in_tiles_bg_img, adj_tile_pos);

        if (srt.dilate_mode_min_max) [[static_branch]] {
          /* Actually gather the "absolute" biggest coc but keeping the sign. */
          ring_buckets[ring].fg_min_coc = min(ring_buckets[ring].fg_min_coc, adj_tile.fg_min_coc);
          ring_buckets[ring].bg_max_coc = max(ring_buckets[ring].bg_max_coc, adj_tile.bg_max_coc);
        }
        else { /* DILATE_MODE_MIN_ABS */
          ring_buckets[ring].fg_max_coc = max(ring_buckets[ring].fg_max_coc, adj_tile.fg_max_coc);
          ring_buckets[ring].bg_min_coc = min(ring_buckets[ring].bg_min_coc, adj_tile.bg_min_coc);

          /* Should be tight as possible to reduce gather overhead (see slide 61). */
          float closest_neighbor_distance = length(max(abs(float2(offset)) - 1.0f, 0.0f)) *
                                            tile_to_fullres_factor;

          ring_buckets[ring].fg_max_intersectable_coc = max(
              ring_buckets[ring].fg_max_intersectable_coc,
              adj_tile.fg_max_intersectable_coc + closest_neighbor_distance);
          ring_buckets[ring].bg_min_intersectable_coc = min(
              ring_buckets[ring].bg_min_intersectable_coc,
              adj_tile.bg_min_intersectable_coc + closest_neighbor_distance);
        }
      }
    }
  }

  /* Load center tile. */
  CocTile out_tile = dof_coc_tile_load(
      in_tiles.in_tiles_fg_img, in_tiles.in_tiles_bg_img, center_tile_pos);

  for (int ring = 0; ring < srt.ring_count && ring < DOF_DILATE_RING_COUNT; ring++) {
    float ring_distance = float(ring + 1);

    ring_distance = (ring_distance * srt.ring_width_multiplier - 1) * tile_to_fullres_factor;

    if (srt.dilate_mode_min_max) [[static_branch]] {
      /* NOTE(fclem): Unsure if both sides of the inequalities have the same unit. */
      if (-ring_buckets[ring].fg_min_coc * bluring_radius_error > ring_distance) {
        out_tile.fg_min_coc = min(out_tile.fg_min_coc, ring_buckets[ring].fg_min_coc);
      }

      if (ring_buckets[ring].bg_max_coc * bluring_radius_error > ring_distance) {
        out_tile.bg_max_coc = max(out_tile.bg_max_coc, ring_buckets[ring].bg_max_coc);
      }
    }
    else { /* DILATE_MODE_MIN_ABS */
      /* Find minimum absolute CoC radii that will be intersected for the previously
       * computed maximum CoC values. */
      if (-out_tile.fg_min_coc * bluring_radius_error > ring_distance) {
        out_tile.fg_max_coc = max(out_tile.fg_max_coc, ring_buckets[ring].fg_max_coc);
        out_tile.fg_max_intersectable_coc = max(out_tile.fg_max_intersectable_coc,
                                                ring_buckets[ring].fg_max_intersectable_coc);
      }

      if (out_tile.bg_max_coc * bluring_radius_error > ring_distance) {
        out_tile.bg_min_coc = min(out_tile.bg_min_coc, ring_buckets[ring].bg_min_coc);
        out_tile.bg_min_intersectable_coc = min(out_tile.bg_min_intersectable_coc,
                                                ring_buckets[ring].bg_min_intersectable_coc);
      }
    }
  }

  int2 texel_out = int2(global_id.xy);
  dof_coc_tile_store(out_tiles.out_tiles_fg_img, out_tiles.out_tiles_bg_img, texel_out, out_tile);
}

}  // namespace tile_dilate

}  // namespace eevee::dof

PipelineCompute eevee_depth_of_field_tiles_flatten(eevee::dof::tile_flatten::comp_main);

PipelineCompute eevee_depth_of_field_tiles_dilate_minabs(eevee::dof::tile_dilate::comp_main,
                                                         eevee::dof::tile_dilate::Resources{
                                                             .dilate_mode_min_max = false});
PipelineCompute eevee_depth_of_field_tiles_dilate_minmax(eevee::dof::tile_dilate::comp_main,
                                                         eevee::dof::tile_dilate::Resources{
                                                             .dilate_mode_min_max = true});
