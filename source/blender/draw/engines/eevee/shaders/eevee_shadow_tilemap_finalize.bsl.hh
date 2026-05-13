/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_shader_shared.hh"
#include "eevee_defines.hh"
#include "eevee_shadow_shared.hh"

#include "eevee_shadow_tilemap_lib.bsl.hh"
#include "gpu_shader_math_matrix_projection_lib.glsl"

shared int rect_min_x;
shared int rect_min_y;
shared int rect_max_x;
shared int rect_max_y;
shared uint lod_rendered;

namespace eevee::shadow {

/**
 * Select the smallest viewport that can contain the given rect of tiles to render.
 * Returns the viewport index.
 */
int viewport_select(int2 rect_size)
{
  /* TODO(fclem): Experiment with non squared viewports. */
  int max_dim = max(rect_size.x, rect_size.y);
  /* Assumes max_dim is non-null. */
  int power_of_two = findMSB(uint(max_dim));
  if ((1 << power_of_two) != max_dim) {
    power_of_two += 1;
  }
  return power_of_two;
}

struct TilemapFinalize {
  [[storage(0, read)]] const ShadowTileMapData (&tilemaps_buf)[];
  [[storage(1, read)]] const uint (&tiles_buf)[];
  [[storage(2, read_write)]] ShadowPagesInfoData &pages_infos_buf;
  [[storage(3, read_write)]] ShadowStatistics &statistics_buf;
  [[storage(4, write)]] ViewMatrices (&view_infos_buf)[SHADOW_VIEW_MAX];
  [[storage(5, write)]] ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];
  [[storage(6, read)]] const ShadowTileMapClip (&tilemaps_clip_buf)[];
  [[image(0, write, UINT_32)]] uimage2D tilemaps_img;
};

/**
 * Virtual shadow-mapping: Tile-map to texture conversion.
 *
 * For all visible light tile-maps, copy page coordinate to a texture.
 * This avoids one level of indirection when evaluating shadows and allows
 * to use a sampler instead of a SSBO bind.
 */
[[compute, local_size(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES)]]
void tilemap_finalize_main([[resource_table]] TilemapFinalize &srt,
                           [[global_invocation_id]] const uint3 global_id,
                           [[local_invocation_index]] const uint local_index)

{
  int tilemap_index = int(global_id.z);
  int2 tile_co = int2(global_id.xy);

  ShadowTileMapData tilemap_data = srt.tilemaps_buf[tilemap_index];
  bool is_cubemap = (tilemap_data.projection_type == SHADOW_PROJECTION_CUBEFACE);
  int lod_max = is_cubemap ? SHADOW_TILEMAP_LOD : 0;

  lod_rendered = 0u;

  for (int lod = lod_max; lod >= 0; lod--) {
    int2 tile_co_lod = tile_co >> lod;
    int tile_index = shadow_tile_offset(uint2(tile_co_lod), tilemap_data.tiles_index, lod);

    /* Compute update area. */
    if (local_index == 0u) {
      rect_min_x = SHADOW_TILEMAP_RES;
      rect_min_y = SHADOW_TILEMAP_RES;
      rect_max_x = 0;
      rect_max_y = 0;
    }

    barrier();

    ShadowTileData tile = shadow_tile_unpack(srt.tiles_buf[tile_index]);
    bool lod_valid_thread = all(equal(tile_co, tile_co_lod << lod));
    bool do_page_render = tile.is_used && tile.do_update && lod_valid_thread;
    if (do_page_render) {
      atomicMin(rect_min_x, tile_co_lod.x);
      atomicMin(rect_min_y, tile_co_lod.y);
      atomicMax(rect_max_x, tile_co_lod.x + 1);
      atomicMax(rect_max_y, tile_co_lod.y + 1);
    }

    barrier();

    int2 rect_min = int2(rect_min_x, rect_min_y);
    int2 rect_max = int2(rect_max_x, rect_max_y);

    int viewport_index = viewport_select(rect_max - rect_min);
    int2 viewport_size = shadow_viewport_size_get(uint(viewport_index));

    /* Issue one view if there is an update in the LOD. */
    if (local_index == 0u) {
      bool lod_has_update = rect_min.x < rect_max.x;
      if (lod_has_update) {
        int view_index = atomicAdd(srt.statistics_buf.view_needed_count, 1);
        if (view_index < SHADOW_VIEW_MAX) {
          lod_rendered |= 1u << lod;

          /* Setup the view. */
          srt.view_infos_buf[view_index].viewmat = tilemap_data.viewmat;
          srt.view_infos_buf[view_index].viewinv = inverse(tilemap_data.viewmat);

          float lod_res = float(SHADOW_TILEMAP_RES >> lod);

          /* TODO(fclem): These should be the culling planes. */
          // float2 cull_region_start = (float2(rect_min) / lod_res) * 2.0f - 1.0f;
          // float2 cull_region_end = (float2(rect_max) / lod_res) * 2.0f - 1.0f;
          float2 view_start = (float2(rect_min) / lod_res) * 2.0f - 1.0f;
          float2 view_end = (float2(rect_min + viewport_size) / lod_res) * 2.0f - 1.0f;

          int clip_index = tilemap_data.clip_data_index;
          float clip_far = srt.tilemaps_clip_buf[clip_index].clip_far_stored;
          float clip_near = srt.tilemaps_clip_buf[clip_index].clip_near_stored;

          view_start = view_start * tilemap_data.half_size + tilemap_data.center_offset;
          view_end = view_end * tilemap_data.half_size + tilemap_data.center_offset;

          float4x4 winmat;
          if (tilemap_data.projection_type != SHADOW_PROJECTION_CUBEFACE) {
            winmat = projection_orthographic(
                view_start.x, view_end.x, view_start.y, view_end.y, clip_near, clip_far);
          }
          else {
            winmat = projection_perspective(
                view_start.x, view_end.x, view_start.y, view_end.y, clip_near, clip_far);
          }

          srt.view_infos_buf[view_index].winmat = winmat;
          srt.view_infos_buf[view_index].wininv = inverse(winmat);

          srt.render_view_buf[view_index].viewport_index = uint(viewport_index);
          srt.render_view_buf[view_index].is_directional = !is_cubemap;
          srt.render_view_buf[view_index].clip_near = clip_near;
          /* Clipping setup. */
          if (is_point_light(tilemap_data.light_type)) {
            /* Clip as a sphere around the clip_near cube. */
            srt.render_view_buf[view_index].clip_distance_inv = M_SQRT1_3 / tilemap_data.clip_near;
          }
          else {
            /* Disable local clipping. */
            srt.render_view_buf[view_index].clip_distance_inv = 0.0f;
          }
          /* For building the render map. */
          srt.render_view_buf[view_index].tilemap_tiles_index = tilemap_data.tiles_index;
          srt.render_view_buf[view_index].tilemap_lod = lod;
          srt.render_view_buf[view_index].rect_min = rect_min;
          /* For shadow linking. */
          srt.render_view_buf[view_index].shadow_set_membership =
              tilemap_data.shadow_set_membership;
        }
      }
    }
  }

  /* Broadcast result of `lod_rendered`. */
  barrier();

  /* With all threads (LOD0 size dispatch) load each lod tile from the highest lod
   * to the lowest, keeping track of the lowest one allocated which will be use for shadowing.
   * This guarantee a O(1) lookup time.
   * Add one render view per LOD that has tiles to be rendered. */
  int valid_tile_index = -1;
  uint valid_lod = 0u;
  for (int lod = lod_max; lod >= 0; lod--) {
    int2 tile_co_lod = tile_co >> lod;
    int tile_index = shadow_tile_offset(uint2(tile_co_lod), tilemap_data.tiles_index, lod);
    ShadowTileData tile = shadow_tile_unpack(srt.tiles_buf[tile_index]);

    bool lod_is_rendered = ((lod_rendered >> lod) & 1u) == 1u;
    if (tile.is_used && tile.is_allocated && (!tile.do_update || lod_is_rendered)) {
      /* Save highest lod for this thread. */
      valid_tile_index = tile_index;
      valid_lod = uint(lod);
    }
  }

  /* Store the highest LOD valid page for rendering. */
  ShadowTileDataPacked tile_packed = (valid_tile_index != -1) ? srt.tiles_buf[valid_tile_index] :
                                                                SHADOW_NO_DATA;
  ShadowTileData tile_data = shadow_tile_unpack(tile_packed);
  ShadowSamplingTile tile_sampling = shadow_sampling_tile_create(tile_data, valid_lod);
  ShadowSamplingTilePacked tile_sampling_packed = shadow_sampling_tile_pack(tile_sampling);

  uint2 atlas_texel = shadow_tile_coord_in_atlas(uint2(tile_co), tilemap_index);
  imageStoreFast(srt.tilemaps_img, int2(atlas_texel), uint4(tile_sampling_packed));

  if (all(equal(global_id, uint3(0)))) {
    /* Clamp it as it can underflow if there is too much tile present on screen. */
    srt.pages_infos_buf.page_free_count = max(srt.pages_infos_buf.page_free_count, 0);
  }
}

struct RendermapFinalize {
  [[storage(0, read_write)]] ShadowStatistics &statistics_buf;
  [[storage(1, read)]] const ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];
  [[storage(2, read_write)]] uint (&tiles_buf)[];
  [[storage(3, read_write)]] DispatchCommand &clear_dispatch_buf;
  [[storage(4, read_write)]] DrawCommandArray &tile_draw_buf;
  [[storage(5, write)]] uint (&dst_coord_buf)[SHADOW_RENDER_MAP_SIZE];
  [[storage(6, write)]] uint (&src_coord_buf)[SHADOW_RENDER_MAP_SIZE];
  [[storage(7, write)]] uint (&render_map_buf)[SHADOW_RENDER_MAP_SIZE];
};

/**
 * Virtual shadow-mapping: Tile-map to render-map conversion.
 *
 * For each shadow view, copy page atlas location to the indirection table before render.
 */
[[compute, local_size(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES)]]
void rendermap_finalize_main([[resource_table]] RendermapFinalize &srt,
                             [[global_invocation_id]] const uint3 global_id)
{
  int view_index = int(global_id.z);
  /* Dispatch size if already bounded by SHADOW_VIEW_MAX. */
  if (view_index >= srt.statistics_buf.view_needed_count) {
    return;
  }

  int2 rect_min = srt.render_view_buf[view_index].rect_min;
  int tilemap_tiles_index = srt.render_view_buf[view_index].tilemap_tiles_index;
  int lod = srt.render_view_buf[view_index].tilemap_lod;
  int2 viewport_size = shadow_viewport_size_get(srt.render_view_buf[view_index].viewport_index);

  int2 tile_co = int2(global_id.xy);
  int2 tile_co_lod = tile_co >> lod;
  bool lod_valid_thread = all(equal(tile_co, tile_co_lod << lod));

  int tile_index = shadow_tile_offset(uint2(tile_co_lod), tilemap_tiles_index, lod);

  if (lod_valid_thread) {
    ShadowTileData tile = shadow_tile_unpack(srt.tiles_buf[tile_index]);
    /* Tile coordinate relative to chosen viewport origin. */
    int2 viewport_tile_co = tile_co_lod - rect_min;
    /* We need to add page indirection to the render map for the whole viewport even if this one
     * might extend outside of the shadow-map range. To this end, we need to wrap the threads to
     * always cover the whole mip. This is because the viewport cannot be bigger than the mip
     * level itself. */
    int lod_res = SHADOW_TILEMAP_RES >> lod;
    int2 relative_tile_co = (viewport_tile_co + lod_res) % lod_res;
    if (all(lessThan(relative_tile_co, viewport_size))) {
      bool do_page_render = tile.is_used && tile.do_update;
      uint page_packed = shadow_page_pack(tile.page);
      /* Add page to render map. */
      int render_page_index = shadow_render_page_index_get(view_index, relative_tile_co);
      srt.render_map_buf[render_page_index] = do_page_render ? page_packed : 0xFFFFFFFFu;

      if (do_page_render) {
        /* Add page to clear dispatch. */
        uint page_index = atomicAdd(srt.clear_dispatch_buf.num_groups_z, 1u);
        /* Add page to tile processing. */
        atomicAdd(srt.tile_draw_buf.vertex_len, 6u);
        /* Add page mapping for indexing the page position in atlas and in the frame-buffer. */
        srt.dst_coord_buf[page_index] = page_packed;
        srt.src_coord_buf[page_index] = packUvec4x8(
            uint4(int4(relative_tile_co.x, relative_tile_co.y, view_index, 0)));
        /* Tag tile as rendered. Should be safe since only one thread is reading and writing. */
        srt.tiles_buf[tile_index] |= SHADOW_IS_RENDERED;
        /* Statistics. */
        atomicAdd(srt.statistics_buf.page_rendered_count, 1);
      }
    }
  }
}

PipelineCompute tilemap_finalize(eevee::shadow::tilemap_finalize_main);
PipelineCompute tilemap_rendermap(eevee::shadow::rendermap_finalize_main);

}  // namespace eevee::shadow
