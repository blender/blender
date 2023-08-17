
/**
 * Virtual shadowmapping: Setup phase for tilemaps.
 *
 * Clear the usage flag.
 * Also tag for update shifted tiles for directional shadow clipmaps.
 * Dispatched with one local thread per LOD0 tile and one workgroup per tilemap.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

shared bool directional_range_changed;

ShadowTileDataPacked init_tile_data(ShadowTileDataPacked tile, bool do_update)
{
  if (flag_test(tile, SHADOW_IS_RENDERED)) {
    tile &= ~(SHADOW_DO_UPDATE | SHADOW_IS_RENDERED);
  }
  if (do_update) {
    tile |= SHADOW_DO_UPDATE;
  }
  tile &= ~SHADOW_IS_USED;
  return tile;
}

void main()
{
  uint tilemap_index = gl_GlobalInvocationID.z;
  ShadowTileMapData tilemap = tilemaps_buf[tilemap_index];

  barrier();

  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    /* Reset shift to not tag for update more than once per sync cycle. */
    tilemaps_buf[tilemap_index].grid_shift = ivec2(0);

    directional_range_changed = false;

    int clip_index = tilemap.clip_data_index;
    if (clip_index == -1) {
      /* Noop. This is the case for unused tilemaps that are getting pushed to the free heap. */
    }
    else if (tilemap.projection_type != SHADOW_PROJECTION_CUBEFACE) {
      ShadowTileMapClip clip_data = tilemaps_clip_buf[clip_index];
      float clip_near_new = orderedIntBitsToFloat(clip_data.clip_near);
      float clip_far_new = orderedIntBitsToFloat(clip_data.clip_far);
      bool near_changed = clip_near_new != clip_data.clip_near_stored;
      bool far_changed = clip_far_new != clip_data.clip_far_stored;
      directional_range_changed = near_changed || far_changed;
      /* NOTE(fclem): This assumes clip near/far are computed each time the init phase runs. */
      tilemaps_clip_buf[clip_index].clip_near_stored = clip_near_new;
      tilemaps_clip_buf[clip_index].clip_far_stored = clip_far_new;
      /* Reset for next update. */
      tilemaps_clip_buf[clip_index].clip_near = floatBitsToOrderedInt(FLT_MAX);
      tilemaps_clip_buf[clip_index].clip_far = floatBitsToOrderedInt(-FLT_MAX);
    }
    else {
      /* For cubefaces, simply use the light near and far distances. */
      tilemaps_clip_buf[clip_index].clip_near_stored = tilemap.clip_near;
      tilemaps_clip_buf[clip_index].clip_far_stored = tilemap.clip_far;
    }
  }

  barrier();

  ivec2 tile_co = ivec2(gl_GlobalInvocationID.xy);
  ivec2 tile_shifted = tile_co + tilemap.grid_shift;
  /* Ensure value is shifted into positive range to avoid modulo on negative. */
  ivec2 tile_wrapped = ivec2((ivec2(SHADOW_TILEMAP_RES) + tile_shifted) % SHADOW_TILEMAP_RES);

  /* If this tile was shifted in and contains old information, update it.
   * Note that cubemap always shift all tiles on update. */
  bool do_update = !in_range_inclusive(tile_shifted, ivec2(0), ivec2(SHADOW_TILEMAP_RES - 1));

  /* TODO(fclem): Might be better to resize the depth stored instead of a full render update. */
  if (directional_range_changed) {
    do_update = true;
  }

  int lod_max = (tilemap.projection_type == SHADOW_PROJECTION_CUBEFACE) ? SHADOW_TILEMAP_LOD : 0;
  uint lod_size = uint(SHADOW_TILEMAP_RES);
  for (int lod = 0; lod <= lod_max; lod++, lod_size >>= 1u) {
    bool thread_active = all(lessThan(tile_co, ivec2(lod_size)));
    ShadowTileDataPacked tile = 0;
    int tile_load = shadow_tile_offset(tile_wrapped, tilemap.tiles_index, lod);
    if (thread_active) {
      tile = init_tile_data(tiles_buf[tile_load], do_update);
    }

    /* Uniform control flow for barrier. Needed to avoid race condition on shifted loads. */
    barrier();

    if (thread_active) {
      int tile_store = shadow_tile_offset(tile_co, tilemap.tiles_index, lod);
      if ((tile_load != tile_store) && flag_test(tile, SHADOW_IS_CACHED)) {
        /* Inlining of shadow_page_cache_update_tile_ref to avoid buffer depedencies. */
        pages_cached_buf[shadow_tile_unpack(tile).cache_index].y = tile_store;
      }
      tiles_buf[tile_store] = tile;
    }
  }
}
