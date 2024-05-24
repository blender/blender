/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug drawing for virtual shadow-maps.
 * See eShadowDebug for more information.
 */

#pragma BLENDER_REQUIRE(gpu_shader_debug_gradients_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

/** Control the scaling of the tile-map splat. */
const float pixel_scale = 4.0;

ShadowSamplingTile shadow_tile_data_get(usampler2D tilemaps_tx, ShadowCoordinates coord)
{
  return shadow_tile_load(tilemaps_tx, coord.tilemap_tile, coord.tilemap_index);
}

vec3 debug_random_color(ivec2 v)
{
  float r = interlieved_gradient_noise(vec2(v), 0.0, 0.0);
  return hue_gradient(r);
}

vec3 debug_random_color(int v)
{
  return debug_random_color(ivec2(v, 0));
}

void debug_tile_print(ShadowTileData tile, ivec4 tile_coord)
{
#ifdef DRW_DEBUG_PRINT
  drw_print("Tile (", tile_coord.x, ",", tile_coord.y, ") in Tilemap ", tile_coord.z, " : ");
  drw_print(tile.page);
  drw_print(tile.cache_index);
#endif
}

vec3 debug_tile_state_color(ShadowTileData tile)
{
  if (tile.do_update && tile.is_used) {
    /* Updated. */
    return vec3(0.5, 1, 0);
  }
  if (tile.is_used) {
    /* Used but was cached. */
    return vec3(0, 1, 0);
  }
  vec3 col = vec3(0);
  if (tile.is_cached) {
    col += vec3(0.2, 0, 0.5);
    if (tile.do_update) {
      col += vec3(0.8, 0, 0);
    }
  }
  return col;
}

vec3 debug_tile_lod(eLightType type, ShadowSamplingTile tile)
{
  if (!tile.is_valid) {
    return vec3(1, 0, 0);
  }
  /* Uses data from another LOD. */
  return neon_gradient(float(tile.lod) / float((type == LIGHT_SUN) ?
                                                   SHADOW_TILEMAP_MAX_CLIPMAP_LOD :
                                                   SHADOW_TILEMAP_LOD));
}

ShadowCoordinates debug_coord_get(vec3 P, LightData light)
{
  if (is_sun_light(light.type)) {
    vec3 lP = light_world_to_local_direction(light, P);
    return shadow_directional_coordinates(light, lP);
  }
  else {
    vec3 lP = light_world_to_local_point(light, P);
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    return shadow_punctual_coordinates(light, lP, face_id);
  }
}

ShadowSamplingTile debug_tile_get(vec3 P, LightData light)
{
  ShadowCoordinates coord = debug_coord_get(P, light);
  return shadow_tile_data_get(shadow_tilemaps_tx, coord);
}

LightData debug_light_get()
{
  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    LightData light = light_buf[l_idx];
    if (light.tilemap_index == debug_tilemap_index) {
      return light;
    }
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    LightData light = light_buf[l_idx];
    if (light.tilemap_index == debug_tilemap_index) {
      return light;
    }
  }
  LIGHT_FOREACH_END
}

/** Return true if a pixel was written. */
bool debug_tilemaps(vec3 P, LightData light, bool do_debug_sample_tile)
{
  const int debug_tile_size_px = 4;
  ivec2 px = ivec2(gl_FragCoord.xy) / debug_tile_size_px;
  int tilemap = px.x / SHADOW_TILEMAP_RES;
  int tilemap_index = light.tilemap_index + tilemap;
  if ((px.y < SHADOW_TILEMAP_RES) && (tilemap_index <= light_tilemap_max_get(light))) {
    if (do_debug_sample_tile) {
      /* Debug values in the tilemap_tx. */
      uvec2 tilemap_texel = shadow_tile_coord_in_atlas(uvec2(px), tilemap_index);
      ShadowSamplingTile tile = shadow_sampling_tile_unpack(
          texelFetch(shadow_tilemaps_tx, ivec2(tilemap_texel), 0).x);
      /* Leave 1 px border between tile-maps. */
      if (!any(
              equal(ivec2(gl_FragCoord.xy) % (SHADOW_TILEMAP_RES * debug_tile_size_px), ivec2(0))))
      {
        gl_FragDepth = 0.0;
        out_color_add = vec4(debug_tile_lod(light.type, tile), 0.0);
        out_color_mul = vec4(0.0);

        return true;
      }
    }
    else {
      /* Debug actual values in the tile-map buffer. */
      ShadowTileMapData tilemap = tilemaps_buf[tilemap_index];
      int tile_index = shadow_tile_offset(
          (px + SHADOW_TILEMAP_RES) % SHADOW_TILEMAP_RES, tilemap.tiles_index, 0);
      ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
      /* Leave 1 px border between tile-maps. */
      if (!any(
              equal(ivec2(gl_FragCoord.xy) % (SHADOW_TILEMAP_RES * debug_tile_size_px), ivec2(0))))
      {
        gl_FragDepth = 0.0;
        out_color_add = vec4(debug_tile_state_color(tile), 0.0);
        out_color_mul = vec4(0.0);

        return true;
      }
    }
  }
  return false;
}

void debug_tile_state(vec3 P, LightData light)
{
  ShadowSamplingTile tile_samp = debug_tile_get(P, light);
  ShadowCoordinates coord = debug_coord_get(P, light);
  ShadowTileMapData tilemap = tilemaps_buf[coord.tilemap_index];
  int tile_index = shadow_tile_offset(
      ivec2(coord.tilemap_tile >> tile_samp.lod), tilemap.tiles_index, int(tile_samp.lod));
  ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
  out_color_add = vec4(debug_tile_state_color(tile), 0) * 0.5;
  out_color_mul = vec4(0.5);
}

void debug_atlas_values(vec3 P, LightData light)
{
  ShadowCoordinates coord = debug_coord_get(P, light);
  float depth = shadow_read_depth(shadow_atlas_tx, shadow_tilemaps_tx, coord);
  out_color_add = vec4((depth == -1) ? vec3(1.0, 0.0, 0.0) : float3(1.0 / depth), 0.0);
  out_color_mul = vec4(0.5);
}

void debug_random_tile_color(vec3 P, LightData light)
{
  ShadowSamplingTile tile = debug_tile_get(P, light);
  out_color_add = vec4(debug_random_color(ivec2(tile.page.xy)), 0) * 0.5;
  out_color_mul = vec4(0.5);
}

void debug_random_tilemap_color(vec3 P, LightData light)
{
  ShadowCoordinates coord = debug_coord_get(P, light);
  out_color_add = vec4(debug_random_color(ivec2(coord.tilemap_index)), 0) * 0.5;
  out_color_mul = vec4(0.5);
}

void main()
{
  /* Default to no output. */
  gl_FragDepth = 1.0;
  out_color_add = vec4(0.0);
  out_color_mul = vec4(1.0);

  float depth = texelFetch(hiz_tx, ivec2(gl_FragCoord.xy), 0).r;
  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  /* Make it pass the depth test. */
  gl_FragDepth = depth - 1e-6;

  LightData light = debug_light_get();

  bool do_debug_sample_tile = eDebugMode(debug_mode) != DEBUG_SHADOW_TILEMAPS;
  if (debug_tilemaps(P, light, do_debug_sample_tile)) {
    return;
  }

  if (depth != 1.0) {
    switch (eDebugMode(debug_mode)) {
      case DEBUG_SHADOW_TILEMAPS:
        debug_tile_state(P, light);
        break;
      case DEBUG_SHADOW_VALUES:
        debug_atlas_values(P, light);
        break;
      case DEBUG_SHADOW_TILE_RANDOM_COLOR:
        debug_random_tile_color(P, light);
        break;
      case DEBUG_SHADOW_TILEMAP_RANDOM_COLOR:
        debug_random_tilemap_color(P, light);
        break;
    }
  }
}
