/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Operations to move virtual shadow map pages between heaps and tiles.
 * We reuse the blender::vector class denomination.
 *
 * The needed resources for this lib are:
 * - tiles_buf
 * - pages_free_buf
 * - pages_cached_buf
 * - pages_infos_buf
 *
 * A page is can be in 3 state (free, cached, acquired). Each one correspond to a different owner.
 *
 * - The pages_free_buf works in a regular stack containing only the page coordinates.
 *
 * - The pages_cached_buf is a ring buffer where newly cached pages gets added at the end and the
 *   old cached pages gets defragmented at the start of the used portion.
 *
 * - The tiles_buf only owns a page if it is used. If the page is cached, the tile contains a
 *   reference index inside the pages_cached_buf.
 *
 * IMPORTANT: Do not forget to manually store the tile data after doing operations on them.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(eevee_shadow_page_free)
#endif

#include "eevee_shadow_tilemap_lib.glsl"

/* Remove page ownership from the tile and append it to the cache. */
void shadow_page_free(inout ShadowTileData tile)
{
  assert(tile.is_allocated);

  int index = atomicAdd(pages_infos_buf.page_free_count, 1);
  assert(index < SHADOW_MAX_PAGE);
  /* Insert in heap. */
  pages_free_buf[index] = shadow_page_pack(tile.page);
  /* Remove from tile. */
  tile.page = uint3(-1);
  tile.is_cached = false;
  tile.is_allocated = false;
}

/* Remove last page from the free heap and give ownership to the tile. */
void shadow_page_alloc(inout ShadowTileData tile)
{
  assert(!tile.is_allocated);

  int index = atomicAdd(pages_infos_buf.page_free_count, -1) - 1;
  /* This can easily happen in really big scene. */
  if (index < 0) {
    return;
  }
  /* Insert in tile. */
  tile.page = shadow_page_unpack(pages_free_buf[index]);
  tile.is_allocated = true;
  tile.do_update = true;
  /* Remove from heap. */
  pages_free_buf[index] = uint(-1);
}

/* Remove page ownership from the tile cache and append it to the cache. */
void shadow_page_cache_append(inout ShadowTileData tile, uint tile_index)
{
  assert(tile.is_allocated);

  /* The page_cached_next is also wrapped in the defragment phase to avoid unsigned overflow. */
  uint index = atomicAdd(pages_infos_buf.page_cached_next, 1u) % uint(SHADOW_MAX_PAGE);
  /* Insert in heap. */
  pages_cached_buf[index] = uint2(shadow_page_pack(tile.page), tile_index);
  /* Remove from tile. */
  tile.page = uint3(-1);
  tile.cache_index = index;
  tile.is_cached = true;
  tile.is_allocated = false;
}

/* Remove page from cache and give ownership to the tile. */
void shadow_page_cache_remove(inout ShadowTileData tile)
{
  assert(!tile.is_allocated);
  assert(tile.is_cached);

  uint index = tile.cache_index;
  /* Insert in tile. */
  tile.page = shadow_page_unpack(pages_cached_buf[index].x);
  tile.cache_index = uint(-1);
  tile.is_cached = false;
  tile.is_allocated = true;
  /* Remove from heap. Leaves hole in the buffer. This is handled by the defragment phase. */
  pages_cached_buf[index] = uint2(-1);
}

/* Update cached page reference when a cached page moves inside the cached page buffer. */
void shadow_page_cache_update_page_ref(uint page_index, uint new_page_index)
{
  uint tile_index = pages_cached_buf[page_index].y;
  ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
  tile.cache_index = new_page_index;
  tiles_buf[tile_index] = shadow_tile_pack(tile);
}

/* Update cached page reference when a tile referencing a cached page moves inside the tile-map. */
void shadow_page_cache_update_tile_ref(uint page_index, uint new_tile_index)
{
  pages_cached_buf[page_index].y = new_tile_index;
}
