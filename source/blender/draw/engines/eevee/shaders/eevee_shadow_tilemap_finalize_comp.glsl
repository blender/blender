/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Tile-map to texture conversion.
 *
 * For all visible light tile-maps, copy page coordinate to a texture.
 * This avoids one level of indirection when evaluating shadows and allows
 * to use a sampler instead of a SSBO bind.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_tilemap_finalize)

#include "eevee_shadow_tilemap_lib.glsl"
#include "gpu_shader_math_matrix_projection_lib.glsl"

shared int rect_min_x;
shared int rect_min_y;
shared int rect_max_x;
shared int rect_max_y;
shared uint lod_rendered;

/**
 * Select the smallest viewport that can contain the given rect of tiles to render.
 * Returns the viewport index.
 */
int viewport_select(int2 rect_size)
{
  /* TODO(fclem): Experiment with non squared viewports. */
  int max_dim = max(rect_size.x, rect_size.y);
  /* Assumes max_dim is non-null. */
  int power_of_two = int(findMSB(uint(max_dim)));
  if ((1 << power_of_two) != max_dim) {
    power_of_two += 1;
  }
  return power_of_two;
}

void main()
{
  int tilemap_index = int(gl_GlobalInvocationID.z);
  int2 tile_co = int2(gl_GlobalInvocationID.xy);

  ShadowTileMapData tilemap_data = tilemaps_buf[tilemap_index];
  bool is_cubemap = (tilemap_data.projection_type == SHADOW_PROJECTION_CUBEFACE);
  int lod_max = is_cubemap ? SHADOW_TILEMAP_LOD : 0;

  lod_rendered = 0u;

  for (int lod = lod_max; lod >= 0; lod--) {
    int2 tile_co_lod = tile_co >> lod;
    int tile_index = shadow_tile_offset(uint2(tile_co_lod), tilemap_data.tiles_index, lod);

    /* Compute update area. */
    if (gl_LocalInvocationIndex == 0u) {
      rect_min_x = SHADOW_TILEMAP_RES;
      rect_min_y = SHADOW_TILEMAP_RES;
      rect_max_x = 0;
      rect_max_y = 0;
    }

    barrier();

    ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
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
    if (gl_LocalInvocationIndex == 0u) {
      bool lod_has_update = rect_min.x < rect_max.x;
      if (lod_has_update) {
        int view_index = atomicAdd(statistics_buf.view_needed_count, 1);
        if (view_index < SHADOW_VIEW_MAX) {
          lod_rendered |= 1u << lod;

          /* Setup the view. */
          view_infos_buf[view_index].viewmat = tilemap_data.viewmat;
          view_infos_buf[view_index].viewinv = inverse(tilemap_data.viewmat);

          float lod_res = float(SHADOW_TILEMAP_RES >> lod);

          /* TODO(fclem): These should be the culling planes. */
          // float2 cull_region_start = (float2(rect_min) / lod_res) * 2.0f - 1.0f;
          // float2 cull_region_end = (float2(rect_max) / lod_res) * 2.0f - 1.0f;
          float2 view_start = (float2(rect_min) / lod_res) * 2.0f - 1.0f;
          float2 view_end = (float2(rect_min + viewport_size) / lod_res) * 2.0f - 1.0f;

          int clip_index = tilemap_data.clip_data_index;
          float clip_far = tilemaps_clip_buf[clip_index].clip_far_stored;
          float clip_near = tilemaps_clip_buf[clip_index].clip_near_stored;

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

          view_infos_buf[view_index].winmat = winmat;
          view_infos_buf[view_index].wininv = inverse(winmat);

          render_view_buf[view_index].viewport_index = viewport_index;
          render_view_buf[view_index].is_directional = !is_cubemap;
          render_view_buf[view_index].clip_near = clip_near;
          /* Clipping setup. */
          if (is_point_light(tilemap_data.light_type)) {
            /* Clip as a sphere around the clip_near cube. */
            render_view_buf[view_index].clip_distance_inv = M_SQRT1_3 / tilemap_data.clip_near;
          }
          else {
            /* Disable local clipping. */
            render_view_buf[view_index].clip_distance_inv = 0.0f;
          }
          /* For building the render map. */
          render_view_buf[view_index].tilemap_tiles_index = tilemap_data.tiles_index;
          render_view_buf[view_index].tilemap_lod = lod;
          render_view_buf[view_index].rect_min = rect_min;
          /* For shadow linking. */
          render_view_buf[view_index].shadow_set_membership = tilemap_data.shadow_set_membership;
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
    ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);

    bool lod_is_rendered = ((lod_rendered >> lod) & 1u) == 1u;
    if (tile.is_used && tile.is_allocated && (!tile.do_update || lod_is_rendered)) {
      /* Save highest lod for this thread. */
      valid_tile_index = tile_index;
      valid_lod = uint(lod);
    }
  }

  /* Store the highest LOD valid page for rendering. */
  ShadowTileDataPacked tile_packed = (valid_tile_index != -1) ? tiles_buf[valid_tile_index] :
                                                                SHADOW_NO_DATA;
  ShadowTileData tile_data = shadow_tile_unpack(tile_packed);
  ShadowSamplingTile tile_sampling = shadow_sampling_tile_create(tile_data, valid_lod);
  ShadowSamplingTilePacked tile_sampling_packed = shadow_sampling_tile_pack(tile_sampling);

  uint2 atlas_texel = shadow_tile_coord_in_atlas(uint2(tile_co), tilemap_index);
  imageStoreFast(tilemaps_img, int2(atlas_texel), uint4(tile_sampling_packed));

  if (all(equal(gl_GlobalInvocationID, uint3(0)))) {
    /* Clamp it as it can underflow if there is too much tile present on screen. */
    pages_infos_buf.page_free_count = max(pages_infos_buf.page_free_count, 0);
  }
}
