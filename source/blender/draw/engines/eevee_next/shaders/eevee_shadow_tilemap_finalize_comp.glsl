
/**
 * Virtual shadowmapping: Tilemap to texture conversion.
 *
 * For all visible light tilemaps, copy page coordinate to a texture.
 * This avoids one level of indirection when evaluating shadows and allows
 * to use a sampler instead of a SSBO bind.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

shared uint tile_updates_count;
shared int view_index;

void page_clear_buf_append(uint page_packed)
{
  uint clear_page_index = atomicAdd(clear_dispatch_buf.num_groups_z, 1u);
  clear_page_buf[clear_page_index] = page_packed;
}

void page_tag_as_rendered(ivec2 tile_co, int tiles_index, int lod)
{
  int tile_index = shadow_tile_offset(tile_co, tiles_index, lod);
  tiles_buf[tile_index] |= SHADOW_IS_RENDERED;
  atomicAdd(statistics_buf.page_rendered_count, 1);
}

void main()
{
  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    tile_updates_count = uint(0);
  }
  barrier();

  int tilemap_index = int(gl_GlobalInvocationID.z);
  ivec2 tile_co = ivec2(gl_GlobalInvocationID.xy);

  ivec2 atlas_texel = shadow_tile_coord_in_atlas(tile_co, tilemap_index);

  ShadowTileMapData tilemap_data = tilemaps_buf[tilemap_index];
  int lod_max = (tilemap_data.projection_type == SHADOW_PROJECTION_CUBEFACE) ? SHADOW_TILEMAP_LOD :
                                                                               0;

  int lod_valid = 0;
  /* One bit per lod. */
  int do_lod_update = 0;
  /* Packed page (packUvec2x16) to render per LOD. */
  uint updated_lod_page[SHADOW_TILEMAP_LOD + 1];
  uvec2 page_valid;
  /* With all threads (LOD0 size dispatch) load each lod tile from the highest lod
   * to the lowest, keeping track of the lowest one allocated which will be use for shadowing.
   * Also save which page are to be updated. */
  for (int lod = SHADOW_TILEMAP_LOD; lod >= 0; lod--) {
    if (lod > lod_max) {
      updated_lod_page[lod] = 0xFFFFFFFFu;
      continue;
    }

    int tile_index = shadow_tile_offset(tile_co >> lod, tilemap_data.tiles_index, lod);

    ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);

    if (tile.is_used && tile.do_update) {
      do_lod_update = 1 << lod;
      updated_lod_page[lod] = packUvec2x16(tile.page);
    }
    else {
      updated_lod_page[lod] = 0xFFFFFFFFu;
    }

    /* Save highest lod for this thread. */
    if (tile.is_used && lod > 0) {
      /* Reload the page in case there was an allocation in the valid thread. */
      page_valid = tile.page;
      lod_valid = lod;
    }
    else if (lod == 0 && lod_valid != 0 && !tile.is_allocated) {
      /* If the tile is not used, store the valid LOD level in LOD0. */
      tile.page = page_valid;
      tile.lod = lod_valid;
      /* This is not a real ownership. It is just a tag so that the shadowing is deemed correct. */
      tile.is_allocated = true;
    }

    if (lod == 0) {
      imageStore(tilemaps_img, atlas_texel, uvec4(shadow_tile_pack(tile)));
    }
  }

  if (do_lod_update > 0) {
    atomicAdd(tile_updates_count, 1u);
  }

  barrier();

  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    /* No update by default. */
    view_index = 64;

    if (tile_updates_count > 0) {
      view_index = atomicAdd(pages_infos_buf.view_count, 1);
      if (view_index < 64) {
        view_infos_buf[view_index].viewmat = tilemap_data.viewmat;
        view_infos_buf[view_index].viewinv = inverse(tilemap_data.viewmat);

        if (tilemap_data.projection_type != SHADOW_PROJECTION_CUBEFACE) {
          int clip_index = tilemap_data.clip_data_index;
          /* For directionnal, we need to modify winmat to encompass all casters. */
          float clip_far = -tilemaps_clip_buf[clip_index].clip_far_stored;
          float clip_near = -tilemaps_clip_buf[clip_index].clip_near_stored;
          tilemap_data.winmat[2][2] = -2.0 / (clip_far - clip_near);
          tilemap_data.winmat[3][2] = -(clip_far + clip_near) / (clip_far - clip_near);
        }
        view_infos_buf[view_index].winmat = tilemap_data.winmat;
        view_infos_buf[view_index].wininv = inverse(tilemap_data.winmat);
      }
    }
  }

  barrier();

  if (view_index < 64) {
    ivec3 render_map_texel = ivec3(tile_co, view_index);

    /* Store page indirection for rendering. Update every texel in the view array level. */
    if (true) {
      imageStore(render_map_lod0_img, render_map_texel, uvec4(updated_lod_page[0]));
      if (updated_lod_page[0] != 0xFFFFFFFFu) {
        page_clear_buf_append(updated_lod_page[0]);
        page_tag_as_rendered(render_map_texel.xy, tilemap_data.tiles_index, 0);
      }
    }
    render_map_texel.xy >>= 1;
    if (all(equal(tile_co, render_map_texel.xy << 1u))) {
      imageStore(render_map_lod1_img, render_map_texel, uvec4(updated_lod_page[1]));
      if (updated_lod_page[1] != 0xFFFFFFFFu) {
        page_clear_buf_append(updated_lod_page[1]);
        page_tag_as_rendered(render_map_texel.xy, tilemap_data.tiles_index, 1);
      }
    }
    render_map_texel.xy >>= 1;
    if (all(equal(tile_co, render_map_texel.xy << 2u))) {
      imageStore(render_map_lod2_img, render_map_texel, uvec4(updated_lod_page[2]));
      if (updated_lod_page[2] != 0xFFFFFFFFu) {
        page_clear_buf_append(updated_lod_page[2]);
        page_tag_as_rendered(render_map_texel.xy, tilemap_data.tiles_index, 2);
      }
    }
    render_map_texel.xy >>= 1;
    if (all(equal(tile_co, render_map_texel.xy << 3u))) {
      imageStore(render_map_lod3_img, render_map_texel, uvec4(updated_lod_page[3]));
      if (updated_lod_page[3] != 0xFFFFFFFFu) {
        page_clear_buf_append(updated_lod_page[3]);
        page_tag_as_rendered(render_map_texel.xy, tilemap_data.tiles_index, 3);
      }
    }
    render_map_texel.xy >>= 1;
    if (all(equal(tile_co, render_map_texel.xy << 4u))) {
      imageStore(render_map_lod4_img, render_map_texel, uvec4(updated_lod_page[4]));
      if (updated_lod_page[4] != 0xFFFFFFFFu) {
        page_clear_buf_append(updated_lod_page[4]);
        page_tag_as_rendered(render_map_texel.xy, tilemap_data.tiles_index, 4);
      }
    }
    render_map_texel.xy >>= 1;
    if (all(equal(tile_co, render_map_texel.xy << 5u))) {
      imageStore(render_map_lod5_img, render_map_texel, uvec4(updated_lod_page[5]));
      if (updated_lod_page[5] != 0xFFFFFFFFu) {
        page_clear_buf_append(updated_lod_page[5]);
        page_tag_as_rendered(render_map_texel.xy, tilemap_data.tiles_index, 5);
      }
    }
  }

  if (all(equal(gl_GlobalInvocationID, uvec3(0)))) {
    /* Clamp it as it can underflow if there is too much tile present on screen. */
    pages_infos_buf.page_free_count = max(pages_infos_buf.page_free_count, 0);
  }
}
