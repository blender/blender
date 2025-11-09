/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Tile-map to render-map conversion.
 *
 * For each shadow view, copy page atlas location to the indirection table before render.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_tilemap_rendermap)

#include "eevee_shadow_tilemap_lib.glsl"

void main()
{
  int view_index = int(gl_GlobalInvocationID.z);
  /* Dispatch size if already bounded by SHADOW_VIEW_MAX. */
  if (view_index >= statistics_buf.view_needed_count) {
    return;
  }

  int2 rect_min = render_view_buf[view_index].rect_min;
  int tilemap_tiles_index = render_view_buf[view_index].tilemap_tiles_index;
  int lod = render_view_buf[view_index].tilemap_lod;
  int2 viewport_size = shadow_viewport_size_get(render_view_buf[view_index].viewport_index);

  int2 tile_co = int2(gl_GlobalInvocationID.xy);
  int2 tile_co_lod = tile_co >> lod;
  bool lod_valid_thread = all(equal(tile_co, tile_co_lod << lod));

  int tile_index = shadow_tile_offset(uint2(tile_co_lod), tilemap_tiles_index, lod);

  if (lod_valid_thread) {
    ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
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
      render_map_buf[render_page_index] = do_page_render ? page_packed : 0xFFFFFFFFu;

      if (do_page_render) {
        /* Add page to clear dispatch. */
        uint page_index = atomicAdd(clear_dispatch_buf.num_groups_z, 1u);
        /* Add page to tile processing. */
        atomicAdd(tile_draw_buf.vertex_len, 6u);
        /* Add page mapping for indexing the page position in atlas and in the frame-buffer. */
        dst_coord_buf[page_index] = page_packed;
        src_coord_buf[page_index] = packUvec4x8(
            uint4(relative_tile_co.x, relative_tile_co.y, view_index, 0));
        /* Tag tile as rendered. Should be safe since only one thread is reading and writing. */
        tiles_buf[tile_index] |= SHADOW_IS_RENDERED;
        /* Statistics. */
        atomicAdd(statistics_buf.page_rendered_count, 1);
      }
    }
  }
}
