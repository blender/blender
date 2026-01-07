/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Allocation.
 *
 * Allocates pages to tiles needing them.
 * Note that allocation can fail, in this case the tile is left with no page.
 */

#pragma once
#pragma create_info

#include "eevee_shadow_page_ops.bsl.hh"

namespace eevee::shadow {

using PageAllocator = eevee::shadow::PageAllocator;
using Statistics = eevee::shadow::Statistics;
using TileMaps = eevee::shadow::TileMaps;

[[compute, local_size(SHADOW_TILEMAP_LOD0_LEN)]]
void allocate([[resource_table]] PageAllocator &allocator,
              [[resource_table]] TileMaps &tilemaps,
              [[resource_table]] Statistics &stats,
              [[global_invocation_id]] const uint3 global_invocation_id,
              [[local_invocation_index]] const uint local_tile)
{
  ShadowTileMapData tilemap_data = tilemaps.tilemaps_buf[global_invocation_id.z];

  uint tile_start = uint(tilemap_data.tiles_index);
  for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++) {
    uint lod_len = uint(SHADOW_TILEMAP_LOD0_LEN >> (lod * 2));
    if (local_tile < lod_len) {
      uint tile_index = tile_start + local_tile;

      ShadowTileData tile = shadow_tile_unpack(allocator.tiles_buf[tile_index]);
      if (tile.is_used && !tile.is_allocated) {
        allocator.page_alloc(tile);
        allocator.tiles_buf[tile_index] = shadow_tile_pack(tile);
      }

      if (tile.is_used) {
        atomicAdd(stats.statistics_buf.page_used_count, 1);
      }
      if (tile.is_used && tile.do_update) {
        atomicAdd(stats.statistics_buf.page_update_count, 1);
      }
      if (tile.is_allocated) {
        atomicAdd(stats.statistics_buf.page_allocated_count, 1);
      }
    }
    tile_start += lod_len;
  }
}

PipelineCompute page_allocate(allocate);

}  // namespace eevee::shadow
