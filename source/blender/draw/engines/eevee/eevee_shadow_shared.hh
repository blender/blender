/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "eevee_light_shared.hh"
#include "eevee_transform.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/* -------------------------------------------------------------------- */
/** \name Shadows
 *
 * Shadow data for either a directional shadow or a punctual shadow.
 *
 * A punctual shadow is composed of 1, 5 or 6 shadow regions.
 * Regions are sorted in this order -Z, +X, -X, +Y, -Y, +Z.
 * Face index is computed from light's object space coordinates.
 *
 * A directional light shadow is composed of multiple clip-maps with each level
 * covering twice as much area as the previous one.
 * \{ */

enum eCubeFace : uint32_t {
  /* Ordering by culling order. If cone aperture is shallow, we cull the later view. */
  Z_NEG = 0u,
  X_POS = 1u,
  X_NEG = 2u,
  Y_POS = 3u,
  Y_NEG = 4u,
  Z_POS = 5u,
};

enum eShadowProjectionType : uint32_t {
  SHADOW_PROJECTION_CUBEFACE = 0u,
  SHADOW_PROJECTION_CLIPMAP = 1u,
  SHADOW_PROJECTION_CASCADE = 2u,
};

static inline int2 shadow_cascade_grid_offset(int2 base_offset, int level_relative)
{
  return (base_offset * level_relative) / (1 << 16);
}

/**
 * Small descriptor used for the tile update phase. Updated by CPU & uploaded to GPU each redraw.
 */
struct ShadowTileMapData {
  /** Cached, used for rendering. */
  float4x4 viewmat;
  /** Precomputed matrix, not used for rendering but for tagging. */
  float4x4 winmat;
  /** Punctual : Corners of the frustum. (float3 padded to float4) */
  float4 corners[4];
  /** Integer offset of the center of the 16x16 tiles from the origin of the tile space. */
  int2 grid_offset;
  /** Shift between previous and current grid_offset. Allows update tagging. */
  int2 grid_shift;
  /** True for punctual lights. */
  eShadowProjectionType projection_type;
  /** Multiple of SHADOW_TILEDATA_PER_TILEMAP. Offset inside the tile buffer. */
  int tiles_index;
  /** Index of persistent data in the persistent data buffer. */
  int clip_data_index;
  /** Light type this tilemap is from. */
  eLightType light_type;
  /** Entire tilemap (all tiles) needs to be tagged as dirty. */
  bool32_t is_dirty;
  /** Effective minimum resolution after update throttle. */
  int effective_lod_min;
  float _pad2;
  /** Near and far clip distances for punctual. */
  float clip_near;
  float clip_far;
  /** Half of the tilemap size in world units. Used to compute window matrix. */
  float half_size;
  /** Offset in local space to the tilemap center in world units. Used for directional winmat. */
  float2 center_offset;
  /** Shadow set bitmask of the light using this tilemap. */
  uint2 shadow_set_membership;
  uint2 _pad3;
};
BLI_STATIC_ASSERT_ALIGN(ShadowTileMapData, 16)

/**
 * Lightweight version of ShadowTileMapData that only contains data used for rendering the
 * shadow.
 */
struct ShadowRenderView {
  /**
   * Is either:
   * - positive radial distance for point lights.
   * - zero if disabled.
   */
  float clip_distance_inv;
  /** Viewport to submit the geometry of this tile-map view to. */
  uint viewport_index;
  /** True if coming from a sun light shadow. */
  bool32_t is_directional;
  /** If directional, distance along the negative Z axis of the near clip in view space. */
  float clip_near;
  /** Copy of `ShadowTileMapData.tiles_index`. */
  int tilemap_tiles_index;
  /** The level of detail of the tilemap this view is rendering. */
  int tilemap_lod;
  /** Updated region of the tilemap. */
  int2 rect_min;
  /** Shadow set bitmask of the light generating this view. */
  uint2 shadow_set_membership;
  uint2 _pad0;
};
BLI_STATIC_ASSERT_ALIGN(ShadowRenderView, 16)

/**
 * Per tilemap data persistent on GPU.
 * Kept separately for easier clearing on GPU.
 */
struct ShadowTileMapClip {
  /** Clip distances that were used to render the pages. */
  float clip_near_stored;
  float clip_far_stored;
  /** Near and far clip distances for directional. Float stored as int for atomic operations. */
  /** NOTE: These are positive just like camera parameters. */
  int clip_near;
  int clip_far;
  /* Transform the shadow is rendered with. Used to detect updates on GPU. */
  Transform object_to_world;
  /* Integer offset of the center of the 16x16 tiles from the origin of the tile space. */
  int2 grid_offset;
  int _pad0;
  int _pad1;
};
BLI_STATIC_ASSERT_ALIGN(ShadowTileMapClip, 16)

struct ShadowPagesInfoData {
  /** Number of free pages in the free page buffer. */
  int page_free_count;
  /** Number of page allocations needed for this cycle. */
  int page_alloc_count;
  /** Index of the next cache page in the cached page buffer. */
  uint page_cached_next;
  /** Index of the first page in the buffer since the last defragment. */
  uint page_cached_start;
  /** Index of the last page in the buffer since the last defragment. */
  uint page_cached_end;

  int _pad0;
  int _pad1;
  int _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ShadowPagesInfoData, 16)

struct ShadowStatistics {
  /** Statistics that are read back to CPU after a few frame (to avoid stall). */
  /**
   * WARNING: Excepting `view_needed_count` it is uncertain if these are accurate.
   * This is because `eevee_shadow_page_allocate_comp` runs on all pages even for
   * directional. There might be some lingering states somewhere as relying on
   * `page_update_count` was causing non-deterministic infinite loop. Needs further
   * investigation.
   */
  int page_used_count;
  int page_update_count;
  int page_allocated_count;
  int page_rendered_count;
  int view_needed_count;
  int _pad0;
  int _pad1;
  int _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ShadowStatistics, 16)

/** Decoded tile data structure. */
struct ShadowTileData {
  /** Page inside the virtual shadow map atlas. */
  uint3 page;
  /** Page index inside pages_cached_buf. Only valid if `is_cached` is true. */
  uint cache_index;
  /** If the tile is needed for rendering. */
  bool is_used;
  /** True if an update is needed. This persists even if the tile gets unused. */
  bool do_update;
  /** True if the tile owns the page (mutually exclusive with `is_cached`). */
  bool is_allocated;
  /** True if the tile has been staged for rendering. This will remove the `do_update` flag. */
  bool is_rendered;
  /** True if the tile is inside the pages_cached_buf (mutually exclusive with `is_allocated`).
   */
  bool is_cached;
};
/** \note Stored packed as a uint. */
#define ShadowTileDataPacked uint

enum eShadowFlag : uint32_t {
  SHADOW_NO_DATA = 0u,
  SHADOW_IS_CACHED = (1u << 27u),
  SHADOW_IS_ALLOCATED = (1u << 28u),
  SHADOW_DO_UPDATE = (1u << 29u),
  SHADOW_IS_RENDERED = (1u << 30u),
  SHADOW_IS_USED = (1u << 31u)
};

/* NOTE: Trust the input to be in valid range (max is [3,3,255]).
 * If it is in valid range, it should pack to 12bits so that `shadow_tile_pack()` can use it.
 * But sometime this is used to encode invalid pages uint3(-1) and it needs to output uint(-1).
 */
static inline uint shadow_page_pack(uint3 page)
{
  return (page.x << 0u) | (page.y << 2u) | (page.z << 4u);
}
static inline uint3 shadow_page_unpack(uint data)
{
  uint3 page;
  BLI_STATIC_ASSERT(SHADOW_PAGE_PER_ROW <= 4 && SHADOW_PAGE_PER_COL <= 4, "Update page packing")
  page.x = (data >> 0u) & 3u;
  page.y = (data >> 2u) & 3u;
  BLI_STATIC_ASSERT(SHADOW_MAX_PAGE <= 4096, "Update page packing")
  page.z = (data >> 4u) & 255u;
  return page;
}

static inline ShadowTileData shadow_tile_unpack(ShadowTileDataPacked data)
{
  ShadowTileData tile;
  tile.page = shadow_page_unpack(data);
  /* -- 12 bits -- */
  /* Unused bits. */
  /* -- 15 bits -- */
  BLI_STATIC_ASSERT(SHADOW_MAX_PAGE <= 4096, "Update page packing")
  tile.cache_index = (data >> 15u) & 4095u;
  /* -- 27 bits -- */
  tile.is_used = (data & SHADOW_IS_USED) != 0;
  tile.is_cached = (data & SHADOW_IS_CACHED) != 0;
  tile.is_allocated = (data & SHADOW_IS_ALLOCATED) != 0;
  tile.is_rendered = (data & SHADOW_IS_RENDERED) != 0;
  tile.do_update = (data & SHADOW_DO_UPDATE) != 0;
  return tile;
}

static inline ShadowTileDataPacked shadow_tile_pack(ShadowTileData tile)
{
  uint data;
  /* NOTE: Page might be set to invalid values for tracking invalid usages.
   * So we have to mask the result. */
  data = shadow_page_pack(tile.page) & uint(SHADOW_MAX_PAGE - 1);
  data |= (tile.cache_index & 4095u) << 15u;
  data |= (tile.is_used ? uint(SHADOW_IS_USED) : 0);
  data |= (tile.is_allocated ? uint(SHADOW_IS_ALLOCATED) : 0);
  data |= (tile.is_cached ? uint(SHADOW_IS_CACHED) : 0);
  data |= (tile.is_rendered ? uint(SHADOW_IS_RENDERED) : 0);
  data |= (tile.do_update ? uint(SHADOW_DO_UPDATE) : 0);
  return data;
}

/**
 * Decoded tile data structure.
 * Similar to ShadowTileData, this one is only used for rendering and packed into `tilemap_tx`.
 * This allow to reuse some bits for other purpose.
 */
struct ShadowSamplingTile {
  /** Page inside the virtual shadow map atlas. */
  uint3 page;
  /** LOD pointed by LOD 0 tile page. */
  uint lod;
  /** Offset to the texel position to align with the LOD page start. (directional only). */
  uint2 lod_offset;
  /** If the tile is needed for rendering. */
  bool is_valid;
};
/** \note Stored packed as a uint. */
#define ShadowSamplingTilePacked uint

/* NOTE: Trust the input to be in valid range [0, (1 << SHADOW_TILEMAP_MAX_CLIPMAP_LOD) - 1].
 * Maximum LOD level index we can store is SHADOW_TILEMAP_MAX_CLIPMAP_LOD,
 * so we need SHADOW_TILEMAP_MAX_CLIPMAP_LOD bits to store the offset in each dimension.
 * Result fits into SHADOW_TILEMAP_MAX_CLIPMAP_LOD * 2 bits. */
static inline uint shadow_lod_offset_pack(uint2 ofs)
{
  BLI_STATIC_ASSERT(SHADOW_TILEMAP_MAX_CLIPMAP_LOD <= 8, "Update page packing")
  return ofs.x | (ofs.y << SHADOW_TILEMAP_MAX_CLIPMAP_LOD);
}
static inline uint2 shadow_lod_offset_unpack(uint data)
{
  return (uint2(data) >> uint2(0, SHADOW_TILEMAP_MAX_CLIPMAP_LOD)) &
         uint2((1 << SHADOW_TILEMAP_MAX_CLIPMAP_LOD) - 1);
}

static inline ShadowSamplingTile shadow_sampling_tile_unpack(ShadowSamplingTilePacked data)
{
  ShadowSamplingTile tile;
  tile.page = shadow_page_unpack(data);
  /* -- 12 bits -- */
  /* Max value is actually SHADOW_TILEMAP_MAX_CLIPMAP_LOD but we mask the bits. */
  tile.lod = (data >> 12u) & 15u;
  /* -- 16 bits -- */
  tile.lod_offset = shadow_lod_offset_unpack(data >> 16u);
  /* -- 32 bits -- */
  tile.is_valid = data != 0u;
#ifndef GPU_SHADER
  /* Make tests pass on CPU but it is not required for proper rendering. */
  if (tile.lod == 0) {
    tile.lod_offset.x = 0;
  }
#endif
  return tile;
}

static inline ShadowSamplingTilePacked shadow_sampling_tile_pack(ShadowSamplingTile tile)
{
  if (!tile.is_valid) {
    return 0u;
  }
  /* Tag a valid tile of LOD0 valid by setting their offset to 1.
   * This doesn't change the sampling and allows to use of all bits for data.
   * This makes sure no valid packed tile is 0u. */
  if (tile.lod == 0) {
    tile.lod_offset.x = 1;
  }
  uint data = shadow_page_pack(tile.page);
  /* Max value is actually SHADOW_TILEMAP_MAX_CLIPMAP_LOD but we mask the bits. */
  data |= (tile.lod & 15u) << 12u;
  data |= shadow_lod_offset_pack(tile.lod_offset) << 16u;
  return data;
}

static inline ShadowSamplingTile shadow_sampling_tile_create(ShadowTileData tile_data, uint lod)
{
  ShadowSamplingTile tile;
  tile.page = tile_data.page;
  tile.lod = lod;
  tile.lod_offset = uint2(0, 0); /* Computed during tilemap amend phase. */
  /* At this point, it should be the case that all given tiles that have been tagged as used are
   * ready for sampling. Otherwise tile_data should be SHADOW_NO_DATA. */
  tile.is_valid = tile_data.is_used;
  return tile;
}

/** \} */

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
