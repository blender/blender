/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage un-tagging.
 *
 * Remove used tag from masked tiles (LOD overlap) or for load balancing (reducing the number of
 * views per shadow map).
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_page_mask)

#include "eevee_shadow_tilemap_lib.glsl"

/* Reuse the same enum values for these transient flag during the amend phase.
 * They are never written to the tile data SSBO. */
#define SHADOW_TILE_AMENDED SHADOW_IS_RENDERED
/* Visibility value to write back. */
#define SHADOW_TILE_MASKED SHADOW_IS_ALLOCATED

int shadow_tile_offset_lds(int2 tile, int lod)
{
  return shadow_tile_offset(uint2(tile), 0, lod);
}

/* Deactivate threads that are not part of this LOD. Will only let pass threads which tile
 * coordinate fits the given tilemap LOD. */
bool thread_mask(int2 tile_co, int lod)
{
  constexpr uint lod_size = uint(SHADOW_TILEMAP_RES);
  return all(lessThan(tile_co, int2(lod_size >> lod)));
}

void main()
{
  int2 tile_co = int2(gl_GlobalInvocationID.xy);
  uint tilemap_index = gl_GlobalInvocationID.z;
  ShadowTileMapData tilemap = tilemaps_buf[tilemap_index];

  /* NOTE: Barriers are ok since this branch is taken by all threads. */
  if (tilemap.projection_type == SHADOW_PROJECTION_CUBEFACE) {
    /* Check if any page is allocated in this tilemap. Force base page if that's the case to avoid
     * artifact during shadow tracing. */
    if (gl_LocalInvocationIndex == 0u) {
      force_base_page = 0u;
    }
    barrier();

    /* Load all data to LDS. Allows us to do some modification on the flag bits and only flush to
     * main memory the usage bit. */
    for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++) {
      if (thread_mask(tile_co, lod)) {
        int tile_offset = shadow_tile_offset(uint2(tile_co), tilemap.tiles_index, lod);
        ShadowTileDataPacked tile_data = tiles_buf[tile_offset];

        if ((tile_data & SHADOW_IS_USED) == 0) {
          /* Do not consider this tile as going to be rendered if it is not used.
           * Simplify checks later. This is a local modification. */
          tile_data &= ~SHADOW_DO_UPDATE;
        }
        else {
          /* Tag base level to be used. */
          force_base_page = 1u;
        }
        /* Clear these flags as they could contain any values. */
        tile_data &= ~(SHADOW_TILE_AMENDED | SHADOW_TILE_MASKED);

        int tile_lds = shadow_tile_offset_lds(tile_co, lod);
        tiles_local[tile_lds] = tile_data;
      }
    }

#if 1 /* Can be disabled for debugging. */
    /* For each level collect the number of used (or masked) tile that are covering the tile from
     * the level underneath. If this adds up to 4 the underneath tile is flag unused as its data
     * is not needed for rendering.
     *
     * This is because 2 receivers can tag "used" the same area of the shadow-map but with
     * different LODs. */
    for (int lod = 1; lod <= SHADOW_TILEMAP_LOD; lod++) {
      barrier();
      if (thread_mask(tile_co, lod)) {
        int2 tile_co_prev_lod = tile_co * 2;
        int prev_lod = lod - 1;

        int tile_0 = shadow_tile_offset_lds(tile_co_prev_lod + int2(0, 0), prev_lod);
        int tile_1 = shadow_tile_offset_lds(tile_co_prev_lod + int2(1, 0), prev_lod);
        int tile_2 = shadow_tile_offset_lds(tile_co_prev_lod + int2(0, 1), prev_lod);
        int tile_3 = shadow_tile_offset_lds(tile_co_prev_lod + int2(1, 1), prev_lod);
        /* Is masked if all tiles from the previous level were tagged as used. */
        bool is_masked = ((tiles_local[tile_0] & tiles_local[tile_1] & tiles_local[tile_2] &
                           tiles_local[tile_3]) &
                          SHADOW_IS_USED) != 0;

        int tile_offset = shadow_tile_offset_lds(tile_co, lod);
        if (is_masked) {
          /* Consider this tile occluding lower levels. Use SHADOW_IS_USED flag for that. */
          tiles_local[tile_offset] |= SHADOW_IS_USED;
          /* Do not consider this tile when checking which tilemap level to render in next loop. */
          tiles_local[tile_offset] &= ~SHADOW_DO_UPDATE;
          /* Tag as modified so that we can amend it inside the `tiles_buf`. */
          tiles_local[tile_offset] |= SHADOW_TILE_AMENDED;
          /* Visibility value to write back. */
          tiles_local[tile_offset] |= SHADOW_TILE_MASKED;
        }
      }
    }
#endif

#if 1 /* Can be disabled for debugging. */
    /* Count the number of LOD level to render for this tilemap and to clamp it to a maximum number
     * of view per tilemap.
     * This avoid flooding the 64 view limit per redraw with ~3-4 LOD levels per tilemaps leaving
     * some lights unshadowed.
     * The clamped LOD levels' tiles need to be merged to the highest LOD allowed. */

    /* Construct bitmask of LODs that contain tiles to render (i.e: that will request a view). */
    if (gl_LocalInvocationIndex == 0u) {
      levels_rendered = 0u;
    }
    barrier();
    for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++) {
      /* TODO(fclem): Could maybe speedup using WaveAllBitOr. */
      if (thread_mask(tile_co, lod)) {
        int tile_offset = shadow_tile_offset_lds(tile_co, lod);
        if ((tiles_local[tile_offset] & SHADOW_DO_UPDATE) != 0) {
          atomicOr(levels_rendered, 1u << lod);
        }
      }
    }
    barrier();

    /* If there is more LODs to update than the load balancing heuristic allows. */
    if (bitCount(levels_rendered) > max_view_per_tilemap) {
      /* Find the cutoff LOD that contain tiles to render. */
      int max_lod = findMSB(levels_rendered);
      /* Allow more than one level. */
      for (int i = 1; i < max_view_per_tilemap; i++) {
        max_lod = findMSB(levels_rendered & ~(~0u << max_lod));
      }
      /* NOTE: Concurrent writing of the same value to the same data. */
      tilemaps_buf[tilemap_index].effective_lod_min = max_lod;
      /* Collapse all bits to highest level. */
      for (int lod = 0; lod < max_lod; lod++) {
        if (thread_mask(tile_co, lod)) {
          int tile_offset = shadow_tile_offset_lds(tile_co, lod);
          if ((tiles_local[tile_offset] & SHADOW_DO_UPDATE) != 0) {
            /* This tile is now masked and not considered for rendering. */
            tiles_local[tile_offset] |= SHADOW_TILE_MASKED | SHADOW_TILE_AMENDED;
            /* Note that we can have multiple thread writing to this tile. */
            int tile_bottom_offset = shadow_tile_offset_lds(tile_co >> (max_lod - lod), max_lod);
            /* Tag the associated tile in max_lod to be used as it contains the shadow-map area
             * covered by this collapsed tile. */
            atomicOr(tiles_local[tile_bottom_offset], uint(SHADOW_TILE_AMENDED));
            /* This tile could have been masked by the masking phase.
             * Make sure the flag is unset. */
            atomicAnd(tiles_local[tile_bottom_offset], ~uint(SHADOW_TILE_MASKED));
          }
        }
      }
    }
    else {
      /* NOTE: Concurrent writing of the same value to the same data. */
      tilemaps_buf[tilemap_index].effective_lod_min = 0;
    }
#else
    /* NOTE: Concurrent writing of the same value to the same data. */
    tilemaps_buf[tilemap_index].effective_lod_min = 0;
#endif

    barrier();

#if 1 /* Can be disabled for debugging. */
    if (gl_LocalInvocationIndex == 0u) {
      /* WATCH: To be kept in sync with `max_view_per_tilemap()` function. */
      bool is_render = max_view_per_tilemap == SHADOW_TILEMAP_LOD;
      /* Tag base page to be rendered if any other tile is needed by this shadow.
       * Fixes issue with shadow map ray tracing sampling invalid tiles.
       * Only do this in for final render or if all the main levels were already rendered.
       * This last heuristic avoids very low quality shadows during viewport animation, transform
       * or jittered shadows. */
      if ((force_base_page != 0u) && ((levels_rendered == 0u) || is_render)) {
        int tile_offset = shadow_tile_offset_lds(int2(0), SHADOW_TILEMAP_LOD);
        /* Tag as modified so that we can amend it inside the `tiles_buf`. */
        tiles_local[tile_offset] |= SHADOW_TILE_AMENDED;
        /* Visibility value to write back. */
        tiles_local[tile_offset] &= ~SHADOW_TILE_MASKED;
      }
    }
#endif

    /* Flush back visibility bits to the tile SSBO. */
    for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++) {
      if (thread_mask(tile_co, lod)) {
        int tile_lds = shadow_tile_offset_lds(tile_co, lod);
        if ((tiles_local[tile_lds] & SHADOW_TILE_AMENDED) != 0) {
          int tile_offset = shadow_tile_offset(uint2(tile_co), tilemap.tiles_index, lod);
          /* Note that we only flush the visibility so that cached pages can be reused. */
          if ((tiles_local[tile_lds] & SHADOW_TILE_MASKED) != 0) {
            tiles_buf[tile_offset] &= ~SHADOW_IS_USED;
          }
          else {
            tiles_buf[tile_offset] |= SHADOW_IS_USED;
          }
        }
      }
    }
  }
}
