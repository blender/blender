/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Tile dilate pass: Takes the 8x8 Tiles buffer and converts dilates the tiles with large CoC to
 * their neighborhood. This pass is repeated multiple time until the maximum CoC can be covered.
 */

#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

#define tile_to_fullres_factor float(DOF_TILE_DIVISOR)

/* Error introduced by the random offset of the gathering kernel's center. */
#define bluring_radius_error (1.0 + 1.0 / (gather_ring_count + 0.5))

void main()
{
  ivec2 center_tile_pos = ivec2(gl_FragCoord.xy);

  CocTile ring_buckets[DOF_DILATE_RING_COUNT];

  for (int ring = 0; ring < ringCount && ring < DOF_DILATE_RING_COUNT; ring++) {
    ring_buckets[ring] = dof_coc_tile_init();

    int ring_distance = ring + 1;
    for (int sample_id = 0; sample_id < 4 * ring_distance; sample_id++) {
      ivec2 offset = dof_square_ring_sample_offset(ring_distance, sample_id);

      offset *= ringWidthMultiplier;

      for (int i = 0; i < 2; i++) {
        ivec2 adj_tile_pos = center_tile_pos + ((i == 0) ? offset : -offset);

        CocTile adj_tile = dof_coc_tile_load(cocTilesFgBuffer, cocTilesBgBuffer, adj_tile_pos);

#ifdef DILATE_MODE_MIN_MAX
        /* Actually gather the "absolute" biggest coc but keeping the sign. */
        ring_buckets[ring].fg_min_coc = min(ring_buckets[ring].fg_min_coc, adj_tile.fg_min_coc);
        ring_buckets[ring].bg_max_coc = max(ring_buckets[ring].bg_max_coc, adj_tile.bg_max_coc);

        if (dilateSlightFocus) {
          ring_buckets[ring].fg_slight_focus_max_coc = dof_coc_max_slight_focus(
              ring_buckets[ring].fg_slight_focus_max_coc, adj_tile.fg_slight_focus_max_coc);
        }

#else /* DILATE_MODE_MIN_ABS */
        ring_buckets[ring].fg_max_coc = max(ring_buckets[ring].fg_max_coc, adj_tile.fg_max_coc);
        ring_buckets[ring].bg_min_coc = min(ring_buckets[ring].bg_min_coc, adj_tile.bg_min_coc);

        /* Should be tight as possible to reduce gather overhead (see slide 61). */
        float closest_neighbor_distance = length(max(abs(vec2(offset)) - 1.0, 0.0)) *
                                          tile_to_fullres_factor;

        ring_buckets[ring].fg_max_intersectable_coc = max(
            ring_buckets[ring].fg_max_intersectable_coc,
            adj_tile.fg_max_intersectable_coc + closest_neighbor_distance);
        ring_buckets[ring].bg_min_intersectable_coc = min(
            ring_buckets[ring].bg_min_intersectable_coc,
            adj_tile.bg_min_intersectable_coc + closest_neighbor_distance);
#endif
      }
    }
  }

  /* Load center tile. */
  CocTile out_tile = dof_coc_tile_load(cocTilesFgBuffer, cocTilesBgBuffer, center_tile_pos);

  /* Dilate once. */
  if (dilateSlightFocus) {
    out_tile.fg_slight_focus_max_coc = dof_coc_max_slight_focus(
        out_tile.fg_slight_focus_max_coc, ring_buckets[0].fg_slight_focus_max_coc);
  }

  for (int ring = 0; ring < ringCount && ring < DOF_DILATE_RING_COUNT; ring++) {
    float ring_distance = float(ring + 1);

    ring_distance = (ring_distance * ringWidthMultiplier - 1) * tile_to_fullres_factor;

    /* NOTE(fclem): Unsure if both sides of the inequalities have the same unit. */
#ifdef DILATE_MODE_MIN_MAX
    if (-ring_buckets[ring].fg_min_coc * bluring_radius_error > ring_distance) {
      out_tile.fg_min_coc = min(out_tile.fg_min_coc, ring_buckets[ring].fg_min_coc);
    }

    if (ring_buckets[ring].bg_max_coc * bluring_radius_error > ring_distance) {
      out_tile.bg_max_coc = max(out_tile.bg_max_coc, ring_buckets[ring].bg_max_coc);
    }

#else /* DILATE_MODE_MIN_ABS */
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
#endif
  }

  dof_coc_tile_store(out_tile, outFgCoc, outBgCoc);
}
