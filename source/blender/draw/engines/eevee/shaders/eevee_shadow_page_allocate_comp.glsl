/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Allocation.
 *
 * Allocates pages to tiles needing them.
 * Note that allocation can fail, in this case the tile is left with no page.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_page_allocate)

#include "eevee_shadow_page_ops_lib.glsl"

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
      if (tile.is_used && !tile.is_allocated) {
        shadow_page_alloc(tile);
        tiles_buf[tile_index] = shadow_tile_pack(tile);
      }

      if (tile.is_used) {
        atomicAdd(statistics_buf.page_used_count, 1);
      }
      if (tile.is_used && tile.do_update) {
        atomicAdd(statistics_buf.page_update_count, 1);
      }
      if (tile.is_allocated) {
        atomicAdd(statistics_buf.page_allocated_count, 1);
      }
    }
    tile_start += lod_len;
  }
}
