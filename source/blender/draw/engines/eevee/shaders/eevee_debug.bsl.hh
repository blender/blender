/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug drawing passes for different system.
 * See eShadowDebug for more information.
 */
#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_light_infos.hh"
#include "infos/eevee_shadow_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(draw_view)
FRAGMENT_SHADER_CREATE_INFO(eevee_hiz_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_light_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_shadow_data)

#include "draw_view_lib.glsl"
#include "eevee_debug_shared.hh"
#include "eevee_defines.hh"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_shadow_lib.glsl"
#include "eevee_shadow_shared.hh"
#include "eevee_shadow_tilemap_lib.bsl.hh"
#include "gpu_shader_debug_gradients_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee {

struct DebugVertOut {
  [[smooth]] float2 screen_uv;
};

[[vertex]] void debug_fullscreen_vert([[vertex_id]] const int vert_id,
                                      [[position]] float4 &out_position,
                                      [[out]] DebugVertOut &v_out)
{
  fullscreen_vertex(vert_id, out_position, v_out.screen_uv);
}

#define default_depth -1.0f

struct ShadowDebug {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;

  [[storage(5, read)]] ShadowTileMapData (&tilemaps_buf)[];
  [[storage(6, read)]] uint (&tiles_buf)[];
  [[push_constant]] int debug_mode;
  [[push_constant]] int debug_tilemap_index;

  ShadowSamplingTile shadow_tile_data_get(usampler2D tilemaps_tx, ShadowCoordinates coord) const
  {
    return shadow_tile_load(tilemaps_tx, coord.tilemap_tile, coord.tilemap_index);
  }

  float3 debug_random_color(int2 v) const
  {
    float r = interleaved_gradient_noise(float2(v), 0.0f, 0.0f);
    return hue_gradient(r);
  }

  float3 debug_random_color(int v) const
  {
    return debug_random_color(int2(v, 0));
  }

  void debug_tile_print([[maybe_unused]] ShadowTileData tile,
                        [[maybe_unused]] int4 tile_coord) const
  {
    /* This `printf` injection is based on string literal detection. Comment it out unless needed.
     */
    /* NOTE: using `#if 0` here causes a crash on exit for debug builds, stick to C++ comments. */
    // printf("Tile (%u, %u) in Tilemap %u: page(%u, %u, %u), cache_index %u",
    // tile_coord.x, tile_coord.y, tile_coord.z, tile.page.x, tile.page.y, tile.page.z,
    // tile.cache_index);
  }

  float3 debug_tile_state_color(ShadowTileData tile) const
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

  float3 debug_tile_lod(eLightType type, ShadowSamplingTile tile) const
  {
    if (!tile.is_valid) {
      return float3(1, 0, 0);
    }
    /* Uses data from another LOD. */
    return neon_gradient(float(tile.lod) / float((type == LIGHT_SUN) ?
                                                     SHADOW_TILEMAP_MAX_CLIPMAP_LOD :
                                                     SHADOW_TILEMAP_LOD));
  }

  ShadowCoordinates debug_coord_get(float3 P, LightData light) const
  {
    if (is_sun_light(light.type)) {
      float3 lP = light_world_to_local_direction(light, P);
      return shadow_directional_coordinates(light, lP);
    }

    float3 lP = light_world_to_local_point(light, P);
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    return shadow_punctual_coordinates(light, lP, face_id);
  }

  ShadowSamplingTile debug_tile_get(float3 P, LightData light) const
  {
    ShadowCoordinates coord = debug_coord_get(P, light);
    return shadow_tile_data_get(shadow_tilemaps_tx, coord);
  }

  LightData debug_light_get() const
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
};

struct ShadowDebugFragOut {
  [[frag_color(0), index(0)]] float4 color_add;
  [[frag_color(0), index(1)]] float4 color_mul;
};

struct ShadowDebugOutput {
  bool valid;
  float4 color_add;
  float4 color_mul;
  float depth;
};

ShadowDebugOutput debug_tilemaps([[resource_table]] const ShadowDebug &srt,
                                 float3 /*P*/,
                                 LightData light,
                                 int2 texel,
                                 bool do_debug_sample_tile)
{
  /** Control the scaling of the tile-map splat. */
  constexpr int debug_tile_size_px = 4;

  int2 px = texel / debug_tile_size_px;
  int tilemap = px.x / SHADOW_TILEMAP_RES;
  int tilemap_index = light.tilemap_index + tilemap;
  if ((px.y < SHADOW_TILEMAP_RES) && (tilemap_index <= light_tilemap_max_get(light))) {
    if (do_debug_sample_tile) {
      /* Debug values in the tilemap_tx. */
      uint2 tilemap_texel = shadow_tile_coord_in_atlas(uint2(px), tilemap_index);
      ShadowSamplingTile tile = shadow_sampling_tile_unpack(
          texelFetch(shadow_tilemaps_tx, int2(tilemap_texel), 0).x);
      /* Leave 1 px border between tile-maps. */
      if (!any(equal(texel % (SHADOW_TILEMAP_RES * debug_tile_size_px), int2(0)))) {
        return {.valid = true,
                .color_add = float4(srt.debug_tile_lod(light.type, tile), 0.0f),
                .color_mul = float4(0.0f),
                .depth = 1.0f};
      }
    }
    else {
      /* Debug actual values in the tile-map buffer. */
      ShadowTileMapData tilemap = srt.tilemaps_buf[tilemap_index];
      int tile_index = shadow_tile_offset(
          uint2(px + SHADOW_TILEMAP_RES) % SHADOW_TILEMAP_RES, tilemap.tiles_index, 0);
      ShadowTileData tile = shadow_tile_unpack(srt.tiles_buf[tile_index]);
      /* Leave 1 px border between tile-maps. */
      if (!any(equal(texel % (SHADOW_TILEMAP_RES * debug_tile_size_px), int2(0)))) {
        return {.valid = true,
                .color_add = float4(srt.debug_tile_state_color(tile), 0.0f),
                .color_mul = float4(0.0f),
                .depth = 1.0f};
      }
    }
  }
  return {false, float4(0.0f), float4(1.0f), 1.0f};
}

ShadowDebugOutput debug_tile_state([[resource_table]] const ShadowDebug &srt,
                                   float3 P,
                                   LightData light)
{
  ShadowSamplingTile tile_samp = srt.debug_tile_get(P, light);
  ShadowCoordinates coord = srt.debug_coord_get(P, light);
  ShadowTileMapData tilemap = srt.tilemaps_buf[coord.tilemap_index];
  int tile_index = shadow_tile_offset(
      uint2(coord.tilemap_tile >> tile_samp.lod), tilemap.tiles_index, int(tile_samp.lod));
  ShadowTileData tile = shadow_tile_unpack(srt.tiles_buf[tile_index]);
  return {.valid = true,
          .color_add = float4(srt.debug_tile_state_color(tile), 0) * 0.5f,
          .color_mul = float4(0.5f),
          .depth = default_depth};
}

ShadowDebugOutput debug_atlas_values([[resource_table]] const ShadowDebug &srt,
                                     float3 P,
                                     LightData light)
{
  ShadowCoordinates coord = srt.debug_coord_get(P, light);
  float depth = shadow_read_depth(shadow_atlas_tx, shadow_tilemaps_tx, coord);
  return {
      .valid = true,
      .color_add = float4((depth == -1) ? float3(1.0f, 0.0f, 0.0f) : float3(1.0f / depth), 0.0f) *
                   0.5f,
      .color_mul = float4(0.5f),
      .depth = default_depth};
}

float debug_atomic_cost([[resource_table]] const ShadowDebug &srt, uint l_idx, float3 P)
{
  LightData light = light_buf[l_idx];
  ShadowCoordinates coord = srt.debug_coord_get(P, light);
  uint cost = floatBitsToUint(shadow_read_depth(shadow_atlas_tx, shadow_tilemaps_tx, coord));
  return float(cost - floatBitsToUint(FLT_MAX));
}

ShadowDebugOutput debug_atomic_cost([[resource_table]] const ShadowDebug &srt,
                                    float3 P,
                                    LightData /*light*/)
{
  float cost = 0.0f;
  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    cost += debug_atomic_cost(srt, l_idx, P);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    cost += debug_atomic_cost(srt, l_idx, P);
  }
  LIGHT_FOREACH_END

  cost /= 60.0f;
  return {.valid = true,
          .color_add = float4(heatmap_gradient(cost), 0.0f),
          .color_mul = float4(0.0f),
          .depth = default_depth};
}

ShadowDebugOutput debug_random_tile_color([[resource_table]] const ShadowDebug &srt,
                                          float3 P,
                                          LightData light)
{
  ShadowSamplingTile tile = srt.debug_tile_get(P, light);
  return {.valid = true,
          .color_add = float4(srt.debug_random_color(int2(tile.page.xy)), 0) * 0.5f,
          .color_mul = float4(0.5f),
          .depth = default_depth};
}

ShadowDebugOutput debug_random_tilemap_color([[resource_table]] const ShadowDebug &srt,
                                             float3 P,
                                             LightData light)
{
  ShadowCoordinates coord = srt.debug_coord_get(P, light);
  return {.valid = true,
          .color_add = float4(srt.debug_random_color(int2(coord.tilemap_index)), 0) * 0.5f,
          .color_mul = float4(0.5f),
          .depth = default_depth};
}

[[fragment]]
void debug_shadow_frag([[resource_table]] const ShadowDebug &srt,
                       [[frag_coord]] const float4 frag_co,
                       [[frag_depth(greater)]] float &frag_depth,
                       [[in]] const DebugVertOut v_out,
                       [[out]] ShadowDebugFragOut &frag_out)
{
  /* Default to no output. */
  frag_out.color_add = float4(0.0f);
  frag_out.color_mul = float4(1.0f);

  float depth = texelFetch(hiz_tx, int2(frag_co.xy), 0).r;
  float3 P = drw_point_screen_to_world(float3(v_out.screen_uv, depth));

  const LightData light = srt.debug_light_get();

  const eDebugMode mode = eDebugMode(srt.debug_mode);
  bool do_debug_sample_tile = mode != DEBUG_SHADOW_TILEMAPS;

  ShadowDebugOutput result;
  result.valid = false;
  if (mode != DEBUG_SHADOW_ATOMIC_COST) {
    result = debug_tilemaps(srt, P, light, int2(frag_co.xy), do_debug_sample_tile);
  }

  if (!result.valid && depth != 1.0f) {
    switch (mode) {
      case DEBUG_SHADOW_TILEMAPS:
        result = debug_tile_state(srt, P, light);
        break;
      case DEBUG_SHADOW_VALUES:
        result = debug_atlas_values(srt, P, light);
        break;
      case DEBUG_SHADOW_TILE_RANDOM_COLOR:
        result = debug_random_tile_color(srt, P, light);
        break;
      case DEBUG_SHADOW_TILEMAP_RANDOM_COLOR:
        result = debug_random_tilemap_color(srt, P, light);
        break;
      case DEBUG_SHADOW_ATOMIC_COST:
        result = debug_atomic_cost(srt, P, light);
        break;
      default:
        break;
    }
  }

  frag_out.color_add = result.color_add;
  frag_out.color_mul = result.color_mul;
  /* Make it pass the depth test by default. */
  frag_depth = result.depth == default_depth ? 1.0f - depth + 1e-6f : result.depth;
}

}  // namespace eevee

PipelineGraphic eevee_shadow_debug(eevee::debug_fullscreen_vert, eevee::debug_shadow_frag);
