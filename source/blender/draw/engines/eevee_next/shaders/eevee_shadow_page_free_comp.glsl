/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadowmapping: Tile page freeing.
 *
 * Releases the allocated pages held by tilemaps that have been become unused.
 * Also reclaim cached pages if the tiles needs them.
 * Note that we also count the number of new page allocations needed.
 */

#pragma BLENDER_REQUIRE(eevee_shadow_page_ops_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

void main()
{
  ShadowTileMapData tilemap_data = tilemaps_buf[gl_GlobalInvocationID.z];

  int tile_start = tilemap_data.tiles_index;
  for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++) {
    int lod_len = SHADOW_TILEMAP_LOD0_LEN >> (lod * 2);
    int local_tile = int(gl_LocalInvocationID.x);
    if (local_tile < lod_len) {
      int tile_index = tile_start + local_tile;

      ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);

      bool is_orphaned = !tile.is_used && tile.do_update;
      if (is_orphaned) {
        if (tile.is_cached) {
          shadow_page_cache_remove(tile);
        }
        if (tile.is_allocated) {
          shadow_page_free(tile);
        }
      }

      if (tile.is_used) {
        if (tile.is_cached) {
          shadow_page_cache_remove(tile);
        }
        if (!tile.is_allocated) {
          atomicAdd(pages_infos_buf.page_alloc_count, 1);
        }
      }
      else {
        if (tile.is_allocated) {
          shadow_page_cache_append(tile, tile_index);
        }
      }

      tiles_buf[tile_index] = shadow_tile_pack(tile);
    }
    tile_start += lod_len;
  }
}
