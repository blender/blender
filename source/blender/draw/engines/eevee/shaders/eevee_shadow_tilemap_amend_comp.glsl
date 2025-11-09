/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Amend sampling tile atlas.
 *
 * In order to support sampling different LOD for clipmap shadow projections, we need to scan
 * through the LOD tilemaps from lowest LOD to highest LOD, gathering the last valid tile along the
 * way for the current destination tile. For each new level we gather the previous level tiles from
 * local memory using the correct relative offset from the previous level as they might not be
 * aligned.
 *
 * TODO(fclem): This shader **should** be dispatched for one thread-group per directional light.
 * Currently this shader is dispatched with one thread-group for all directional light.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_tilemap_amend)

#include "eevee_light_iter_lib.glsl"
#include "eevee_shadow_tilemap_lib.glsl"

shared ShadowSamplingTilePacked tiles_local[SHADOW_TILEMAP_RES][SHADOW_TILEMAP_RES];

void main()
{
  int2 tile_co = int2(gl_GlobalInvocationID.xy);

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    LightData light = light_buf[l_idx];
    /* This only works on clip-maps. Cascade have already the same LOD for every tile-maps. */
    if (light.type != LIGHT_SUN) {
      break;
    }
    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      continue;
    }

    int2 base_offset_neg = light_sun_data_get(light).clipmap_base_offset_neg;
    int2 base_offset_pos = light_sun_data_get(light).clipmap_base_offset_pos;
    /* LOD relative max with respect to clipmap_lod_min. */
    int lod_max = light_sun_data_get(light).clipmap_lod_max -
                  light_sun_data_get(light).clipmap_lod_min;
    /* Iterate in reverse. */
    for (int lod = lod_max; lod >= 0; lod--) {
      int tilemap_index = light.tilemap_index + lod;
      uint2 atlas_texel = shadow_tile_coord_in_atlas(uint2(tile_co), tilemap_index);

      ShadowSamplingTilePacked tile_packed = imageLoad(tilemaps_img, int2(atlas_texel)).x;
      ShadowSamplingTile tile = shadow_sampling_tile_unpack(tile_packed);

      if (lod != lod_max && !tile.is_valid) {
        /* Offset this LOD has with the previous one. In unit of tile of the current LOD. */
        int2 offset_binary = ((base_offset_pos >> lod) & 1) - ((base_offset_neg >> lod) & 1);
        int2 offset_centered = int2(SHADOW_TILEMAP_RES / 2) + offset_binary;
        int2 tile_co_prev = (tile_co + offset_centered) >> 1;

        /* Load tile from the previous LOD. */
        ShadowSamplingTilePacked tile_prev_packed = tiles_local[tile_co_prev.y][tile_co_prev.x];
        ShadowSamplingTile tile_prev = shadow_sampling_tile_unpack(tile_prev_packed);

        /* We can only propagate LODs up to a certain level.
         * Afterwards we run out of bits to store the offsets. */
        if (tile_prev.is_valid && tile_prev.lod < SHADOW_TILEMAP_MAX_CLIPMAP_LOD - 1) {
          /* Relative LOD. Used for reducing pixel rate at sampling time.
           * Increase with each new invalid level. */
          tile_prev.lod += 1;

          /* The offset (in tile of current LOD) is equal to the offset from the bottom left corner
           * of both LODs modulo the size of a tile of the source LOD (in tile of current LOD). */

          /* Offset corner to center. */
          tile_prev.lod_offset = uint2(SHADOW_TILEMAP_RES / 2) << tile_prev.lod;
          /* Align center of both LODs. */
          tile_prev.lod_offset -= uint2(SHADOW_TILEMAP_RES / 2);
          /* Add the offset relative to the source LOD. */
          tile_prev.lod_offset += uint2(bitfieldExtract(base_offset_pos, lod, int(tile_prev.lod)) -
                                        bitfieldExtract(base_offset_neg, lod, int(tile_prev.lod)));
          /* Wrap to valid range. */
          tile_prev.lod_offset &= ~(~0u << tile_prev.lod);

          tile_prev_packed = shadow_sampling_tile_pack(tile_prev);
          /* Replace the missing page with the one from the lower LOD. */
          imageStoreFast(tilemaps_img, int2(atlas_texel), uint4(tile_prev_packed));
          /* Push this amended tile to the local tiles. */
          tile_packed = tile_prev_packed;
          tile.is_valid = true;
        }
      }

      barrier();
      tiles_local[tile_co.y][tile_co.x] = (tile.is_valid) ? tile_packed : SHADOW_NO_DATA;
      barrier();
    }
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    LightData light = light_buf[l_idx];
    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      continue;
    }

    int lod_min = 0;
    int tilemap_count = light_local_tilemap_count(light);
    for (int i = 0; i < tilemap_count; i++) {
      ShadowTileMapData tilemap = tilemaps_buf[light.tilemap_index + i];
      lod_min = max(lod_min, tilemap.effective_lod_min);
    }
    if (lod_min > 0) {
      /* Override the effective lod min distance in absolute mode (negative).
       * Note that this only changes the sampling for this AA sample. */
      constexpr float projection_diagonal = 2.0f * M_SQRT2;
      light_buf[l_idx].lod_min = -(projection_diagonal / float(SHADOW_MAP_MAX_RES >> lod_min));
    }
  }
  LIGHT_FOREACH_END
}
