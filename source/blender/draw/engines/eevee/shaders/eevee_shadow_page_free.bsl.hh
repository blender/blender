/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Tile page freeing.
 *
 * Releases the allocated pages held by tile-maps that have been become unused.
 * Also reclaim cached pages if the tiles needs them.
 * Note that we also count the number of new page allocations needed.
 */

#pragma once
#pragma create_info

#include "eevee_shadow_page_ops.bsl.hh"

namespace eevee::shadow {

using PageAllocator = eevee::shadow::PageAllocator;
using TileMaps = eevee::shadow::TileMaps;

[[compute, local_size(SHADOW_TILEMAP_LOD0_LEN)]]
void free([[resource_table]] PageAllocator &allocator,
          [[resource_table]] TileMaps &tilemaps,
          [[global_invocation_id]] const uint3 global_invocation_id,
          [[local_invocation_index]] const uint local_tile)
{
  ShadowTileMapData tilemap_data = tilemaps.tilemaps_buf[global_invocation_id.z];

  uint tile_start = tilemap_data.tiles_index;
  for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++) {
    uint lod_len = uint(SHADOW_TILEMAP_LOD0_LEN >> (lod * 2));
    if (local_tile < lod_len) {
      uint tile_index = tile_start + local_tile;

      ShadowTileData tile = shadow_tile_unpack(allocator.tiles_buf[tile_index]);

      bool is_orphaned = !tile.is_used && tile.do_update;
      if (is_orphaned) {
        if (tile.is_cached) {
          allocator.page_cache_remove(tile);
        }
        if (tile.is_allocated) {
          allocator.page_free(tile);
        }
      }

      if (tile.is_used) {
        if (tile.is_cached) {
          allocator.page_cache_remove(tile);
        }
        if (!tile.is_allocated) {
          atomicAdd(allocator.pages_infos_buf.page_alloc_count, 1);
        }
      }
      else {
        if (tile.is_allocated) {
          allocator.page_cache_append(tile, uint(tile_index));
        }
      }

      allocator.tiles_buf[tile_index] = shadow_tile_pack(tile);
    }
    tile_start += lod_len;
  }
}

PipelineCompute page_free(free);

}  // namespace eevee::shadow
