/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug drawing for virtual shadow-maps.
 * See eShadowDebug for more information.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_shadow_debug)

#include "draw_view_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_shadow_lib.glsl"
#include "eevee_shadow_tilemap_lib.glsl"
#include "gpu_shader_debug_gradients_lib.glsl"

/** Control the scaling of the tile-map splat. */
#define pixel_scale float(4.0f)

ShadowSamplingTile shadow_tile_data_get(usampler2D tilemaps_tx, ShadowCoordinates coord)
{
  return shadow_tile_load(tilemaps_tx, coord.tilemap_tile, coord.tilemap_index);
}

float3 debug_random_color(int2 v)
{
  float r = interleaved_gradient_noise(float2(v), 0.0f, 0.0f);
  return hue_gradient(r);
}

float3 debug_random_color(int v)
{
  return debug_random_color(int2(v, 0));
}

void debug_tile_print(ShadowTileData tile, int4 tile_coord)
{
  /* This `printf` injection is based on string literal detection. Comment it out unless needed. */
  /* NOTE: using `#if 0` here causes a crash on exit for debug builds, stick to C++ comments. */
  // printf("Tile (%u, %u) in Tilemap %u: page(%u, %u, %u), cache_index %u",
  // tile_coord.x, tile_coord.y, tile_coord.z, tile.page.x, tile.page.y, tile.page.z,
  // tile.cache_index);
}

float3 debug_tile_state_color(ShadowTileData tile)
{
  if (tile.do_update && tile.is_used) {
    /* Updated. */
    return float3(0.5f, 1, 0);
  }
  if (tile.is_used) {
    /* Used but was cached. */
    return float3(0, 1, 0);
  }
  float3 col = float3(0);
  if (tile.is_cached) {
    col += float3(0.2f, 0, 0.5f);
    if (tile.do_update) {
      col += float3(0.8f, 0, 0);
    }
  }
  return col;
}

float3 debug_tile_lod(eLightType type, ShadowSamplingTile tile)
{
  if (!tile.is_valid) {
    return float3(1, 0, 0);
  }
  /* Uses data from another LOD. */
  return neon_gradient(float(tile.lod) / float((type == LIGHT_SUN) ?
                                                   SHADOW_TILEMAP_MAX_CLIPMAP_LOD :
                                                   SHADOW_TILEMAP_LOD));
}

ShadowCoordinates debug_coord_get(float3 P, LightData light)
{
  if (is_sun_light(light.type)) {
    float3 lP = light_world_to_local_direction(light, P);
    return shadow_directional_coordinates(light, lP);
  }
  else {
    float3 lP = light_world_to_local_point(light, P);
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    return shadow_punctual_coordinates(light, lP, face_id);
  }
}

ShadowSamplingTile debug_tile_get(float3 P, LightData light)
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

  /* TODO Assert. */
  /* Silence compiler warning. */
  return light_buf[0];
}

/** Return true if a pixel was written. */
bool debug_tilemaps(float3 P, LightData light, bool do_debug_sample_tile)
{
  constexpr int debug_tile_size_px = 4;
  int2 px = int2(gl_FragCoord.xy) / debug_tile_size_px;
  int tilemap = px.x / SHADOW_TILEMAP_RES;
  int tilemap_index = light.tilemap_index + tilemap;
  if ((px.y < SHADOW_TILEMAP_RES) && (tilemap_index <= light_tilemap_max_get(light))) {
    if (do_debug_sample_tile) {
      /* Debug values in the tilemap_tx. */
      uint2 tilemap_texel = shadow_tile_coord_in_atlas(uint2(px), tilemap_index);
      ShadowSamplingTile tile = shadow_sampling_tile_unpack(
          texelFetch(shadow_tilemaps_tx, int2(tilemap_texel), 0).x);
      /* Leave 1 px border between tile-maps. */
      if (!any(equal(int2(gl_FragCoord.xy) % (SHADOW_TILEMAP_RES * debug_tile_size_px), int2(0))))
      {
        gl_FragDepth = 0.0f;
        out_color_add = float4(debug_tile_lod(light.type, tile), 0.0f);
        out_color_mul = float4(0.0f);

        return true;
      }
    }
    else {
      /* Debug actual values in the tile-map buffer. */
      ShadowTileMapData tilemap = tilemaps_buf[tilemap_index];
      int tile_index = shadow_tile_offset(
          uint2(px + SHADOW_TILEMAP_RES) % SHADOW_TILEMAP_RES, tilemap.tiles_index, 0);
      ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
      /* Leave 1 px border between tile-maps. */
      if (!any(equal(int2(gl_FragCoord.xy) % (SHADOW_TILEMAP_RES * debug_tile_size_px), int2(0))))
      {
        gl_FragDepth = 0.0f;
        out_color_add = float4(debug_tile_state_color(tile), 0.0f);
        out_color_mul = float4(0.0f);

        return true;
      }
    }
  }
  return false;
}

void debug_tile_state(float3 P, LightData light)
{
  ShadowSamplingTile tile_samp = debug_tile_get(P, light);
  ShadowCoordinates coord = debug_coord_get(P, light);
  ShadowTileMapData tilemap = tilemaps_buf[coord.tilemap_index];
  int tile_index = shadow_tile_offset(
      uint2(coord.tilemap_tile >> tile_samp.lod), tilemap.tiles_index, int(tile_samp.lod));
  ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);
  out_color_add = float4(debug_tile_state_color(tile), 0) * 0.5f;
  out_color_mul = float4(0.5f);
}

void debug_atlas_values(float3 P, LightData light)
{
  ShadowCoordinates coord = debug_coord_get(P, light);
  float depth = shadow_read_depth(shadow_atlas_tx, shadow_tilemaps_tx, coord);
  out_color_add = float4((depth == -1) ? float3(1.0f, 0.0f, 0.0f) : float3(1.0f / depth), 0.0f);
  out_color_mul = float4(0.5f);
}

void debug_random_tile_color(float3 P, LightData light)
{
  ShadowSamplingTile tile = debug_tile_get(P, light);
  out_color_add = float4(debug_random_color(int2(tile.page.xy)), 0) * 0.5f;
  out_color_mul = float4(0.5f);
}

void debug_random_tilemap_color(float3 P, LightData light)
{
  ShadowCoordinates coord = debug_coord_get(P, light);
  out_color_add = float4(debug_random_color(int2(coord.tilemap_index)), 0) * 0.5f;
  out_color_mul = float4(0.5f);
}

void main()
{
  /* Default to no output. */
  gl_FragDepth = 1.0f;
  out_color_add = float4(0.0f);
  out_color_mul = float4(1.0f);

  float depth = texelFetch(hiz_tx, int2(gl_FragCoord.xy), 0).r;
  float3 P = drw_point_screen_to_world(float3(screen_uv, depth));
  /* Make it pass the depth test. */
  gl_FragDepth = depth - 1e-6f;

  LightData light = debug_light_get();

  bool do_debug_sample_tile = eDebugMode(debug_mode) != DEBUG_SHADOW_TILEMAPS;
  if (debug_tilemaps(P, light, do_debug_sample_tile)) {
    return;
  }

  if (depth != 1.0f) {
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
      default:
        break;
    }
  }
}
