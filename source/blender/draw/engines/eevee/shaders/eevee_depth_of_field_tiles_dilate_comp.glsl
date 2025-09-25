/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Tile dilate pass: Takes the 8x8 Tiles buffer and converts dilates the tiles with large CoC to
 * their neighborhood. This pass is repeated multiple time until the maximum CoC can be covered.
 *
 * Input & Output:
 * - Separated foreground and background CoC. 1/8th of half-res resolution. So 1/16th of full-res.
 */

#include "infos/eevee_depth_of_field_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate)

#include "eevee_depth_of_field_lib.glsl"

/* Error introduced by the random offset of the gathering kernel's center. */
#define bluring_radius_error float(1.0f + 1.0f / (float(DOF_GATHER_RING_COUNT) + 0.5f))
#define tile_to_fullres_factor float(float(DOF_TILES_SIZE * 2))

void main()
{
  int2 center_tile_pos = int2(gl_GlobalInvocationID.xy);

  CocTile ring_buckets[DOF_DILATE_RING_COUNT];

  for (int ring = 0; ring < ring_count && ring < DOF_DILATE_RING_COUNT; ring++) {
    ring_buckets[ring] = dof_coc_tile_init();

    int ring_distance = ring + 1;
    for (int sample_id = 0; sample_id < 4 * ring_distance; sample_id++) {
      int2 offset = dof_square_ring_sample_offset(ring_distance, sample_id);

      offset *= ring_width_multiplier;

      for (int i = 0; i < 2; i++) {
        int2 adj_tile_pos = center_tile_pos + ((i == 0) ? offset : -offset);

        CocTile adj_tile = dof_coc_tile_load(in_tiles_fg_img, in_tiles_bg_img, adj_tile_pos);

        if (DILATE_MODE_MIN_MAX) {
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
  CocTile out_tile = dof_coc_tile_load(in_tiles_fg_img, in_tiles_bg_img, center_tile_pos);

  for (int ring = 0; ring < ring_count && ring < DOF_DILATE_RING_COUNT; ring++) {
    float ring_distance = float(ring + 1);

    ring_distance = (ring_distance * ring_width_multiplier - 1) * tile_to_fullres_factor;

    if (DILATE_MODE_MIN_MAX) {
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

  int2 texel_out = int2(gl_GlobalInvocationID.xy);
  dof_coc_tile_store(out_tiles_fg_img, out_tiles_bg_img, texel_out, out_tile);
}
