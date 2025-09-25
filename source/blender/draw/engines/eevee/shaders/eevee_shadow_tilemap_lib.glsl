/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

#include "draw_shape_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/**
 * Select the smallest viewport that can contain the given rectangle of tiles to render.
 * Returns the viewport size in tile.
 */
int2 shadow_viewport_size_get(uint viewport_index)
{
  /* TODO(fclem): Experiment with non squared viewports. */
  return int2(1u << viewport_index);
}

/* ---------------------------------------------------------------------- */
/** \name Tile-map data
 * \{ */

int shadow_tile_index(int2 tile)
{
  return tile.x + tile.y * SHADOW_TILEMAP_RES;
}

uint2 shadow_tile_coord(int tile_index)
{
  return uint2(tile_index % SHADOW_TILEMAP_RES, tile_index / SHADOW_TILEMAP_RES);
}

/* Return bottom left pixel position of the tile-map inside the tile-map atlas. */
uint2 shadow_tilemap_start(int tilemap_index)
{
  return SHADOW_TILEMAP_RES *
         uint2(tilemap_index % SHADOW_TILEMAP_PER_ROW, tilemap_index / SHADOW_TILEMAP_PER_ROW);
}

uint2 shadow_tile_coord_in_atlas(uint2 tile, int tilemap_index)
{
  return shadow_tilemap_start(tilemap_index) + tile;
}

/**
 * Return tile index inside `tiles_buf` for a given tile coordinate inside a specific LOD.
 * `tiles_index` should be `ShadowTileMapData.tiles_index`.
 */
int shadow_tile_offset(uint2 tile, int tiles_index, int lod)
{
#if SHADOW_TILEMAP_LOD > 5
#  error This needs to be adjusted
#endif
  constexpr int lod0_width = SHADOW_TILEMAP_RES / 1;
  constexpr int lod1_width = SHADOW_TILEMAP_RES / 2;
  constexpr int lod2_width = SHADOW_TILEMAP_RES / 4;
  constexpr int lod3_width = SHADOW_TILEMAP_RES / 8;
  constexpr int lod4_width = SHADOW_TILEMAP_RES / 16;
  constexpr int lod5_width = SHADOW_TILEMAP_RES / 32;
  constexpr int lod0_size = lod0_width * lod0_width;
  constexpr int lod1_size = lod1_width * lod1_width;
  constexpr int lod2_size = lod2_width * lod2_width;
  constexpr int lod3_size = lod3_width * lod3_width;
  constexpr int lod4_size = lod4_width * lod4_width;

  /* TODO(fclem): Convert everything to uint. */
  int offset = tiles_index;
  switch (lod) {
    case 5:
      offset += lod0_size + lod1_size + lod2_size + lod3_size + lod4_size;
      offset += int(tile.y) * lod5_width;
      break;
    case 4:
      offset += lod0_size + lod1_size + lod2_size + lod3_size;
      offset += int(tile.y) * lod4_width;
      break;
    case 3:
      offset += lod0_size + lod1_size + lod2_size;
      offset += int(tile.y) * lod3_width;
      break;
    case 2:
      offset += lod0_size + lod1_size;
      offset += int(tile.y) * lod2_width;
      break;
    case 1:
      offset += lod0_size;
      offset += int(tile.y) * lod1_width;
      break;
    case 0:
    default:
      offset += int(tile.y) * lod0_width;
      break;
  }
  offset += int(tile.x);
  return offset;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Load / Store functions.
 * \{ */

/** \note Will clamp if out of bounds. */
ShadowSamplingTile shadow_tile_load(usampler2D tilemaps_tx, uint2 tile_co, int tilemap_index)
{
  /* NOTE(@fclem): This clamp can hide some small imprecision at clip-map transition.
   * Can be disabled to check if the clip-map is well centered. */
  tile_co = clamp(tile_co, uint2(0), uint2(SHADOW_TILEMAP_RES - 1));
  uint2 texel = shadow_tile_coord_in_atlas(tile_co, tilemap_index);
  uint tile_data = texelFetch(tilemaps_tx, int2(texel), 0).x;
  return shadow_sampling_tile_unpack(tile_data);
}

#if 0 /* TODO(fclem): Finish. We can simplify sampling logic and only tag radially. */

/**
 * Return the tilemap at a given point.
 *
 * This function should be the inverse of ShadowDirectional::coverage_get().
 *
 * \a lP shading point position in light space, relative to the to camera position snapped to
 * the smallest clip-map level (`shadow_world_to_local(light, P) - light_position_get(light)`).
 */
int shadow_directional_tilemap_index(LightData light, float3 lP)
{
  LightSunData sun = light_sun_data_get(light);
  int lvl;
  if (light.type == LIGHT_SUN) {
    /* We need to hide one tile worth of data to hide the moving transition. */
    constexpr float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 1.0001f);
    /* Avoid using log2 when we can just get the exponent from the floating point. */
    frexp(reduce_max(abs(lP)) * narrowing * 2.0f, lvl);
  }
  else {
    /* Since we want half of the size, bias the level by -1. */
    /* TODO(fclem): Precompute. */
    float lod_min_half_size = exp2(float(sun.clipmap_lod_min - 1));
    lvl = reduce_max(lP.xy) / lod_min_half_size;
  }
  return light.clamp(lvl, sun.clipmap_lod_min, sun.clipmap_lod_max);
}

#endif

/**
 * This function should be the inverse of ShadowDirectional::coverage_get().
 *
 * \a lP shading point position in light space, relative to the to camera position snapped to
 * the smallest clip-map level (`shadow_world_to_local(light, P) - light_position_get(light)`).
 */
float shadow_directional_level_fractional(LightData light, float3 lP)
{
  float lod;
  if (light.type == LIGHT_SUN) {
    /* We need to hide one tile worth of data to hide the moving transition. */
    constexpr float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 1.0001f);
    /* Since the distance is centered around the camera (and thus by extension the tile-map),
     * we need to multiply by 2 to get the lod level which covers the following range:
     * [-coverage_get(lod)/2..coverage_get(lod)/2] */
    lod = log2(length(lP) * narrowing * 2.0f);
    /* Apply light LOD bias. */
    lod = max(lod + light.lod_bias, light.lod_min);
  }
  else {
    /* The narrowing need to be stronger since the tile-map position is not rounded but floored. */
    constexpr float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 2.5001f);
    /* Since we want half of the size, bias the level by -1. */
    float clipmap_lod_min_minus_one = float(light_sun_data_get(light).clipmap_lod_min - 1);
    float lod_min_half_size = exp2(clipmap_lod_min_minus_one);
    lod = length(lP.xy) * narrowing / lod_min_half_size;
    /* Apply cascade lod bias. Light bias is not supported here. */
    lod += clipmap_lod_min_minus_one;
  }
  return clamp(lod,
               float(light_sun_data_get(light).clipmap_lod_min),
               float(light_sun_data_get(light).clipmap_lod_max));
}

int shadow_directional_level(LightData light, float3 lP)
{
  /* The level can be negative and is increasing with the distance.
   * So we have to ceil instead of flooring. */
  return int(ceil(shadow_directional_level_fractional(light, lP)));
}

/**
 * Returns the ratio of radius between shadow map pixels and screen pixels.
 * `distance_to_camera` is Z distance to the camera origin.
 */
float shadow_punctual_pixel_ratio(LightData light,
                                  float3 lP,
                                  bool is_perspective,
                                  float distance_to_camera,
                                  float film_pixel_radius)
{
  film_pixel_radius *= exp2(light.lod_bias);

  float distance_to_light = reduce_max(abs(lP));
  /* We project a shadow map pixel (as a sphere for simplicity) to the receiver plane.
   * We then reproject this sphere onto the camera screen and compare it to the film pixel size.
   * This gives a good approximation of what LOD to select to get a somewhat uniform shadow map
   * resolution in screen space. */
  float film_pixel_footprint = (is_perspective) ? film_pixel_radius * distance_to_camera :
                                                  film_pixel_radius;
  /* Clamp in world space. */
  film_pixel_footprint = max(film_pixel_footprint, light.lod_min);
  /* Project onto light's unit plane (per cubeface). */
  film_pixel_footprint /= distance_to_light;
  /* Clamp in shadow space. */
  film_pixel_footprint = max(film_pixel_footprint, -light.lod_min);
  /* Cube-face diagonal divided by LOD0 resolution. */
  constexpr float shadow_pixel_radius = (2.0f * M_SQRT2) / SHADOW_MAP_MAX_RES;
  return saturate(shadow_pixel_radius / film_pixel_footprint);
}

/**
 * Returns the LOD for a given shadow space position.
 * `distance_to_camera` is Z distance to the camera origin.
 */
float shadow_punctual_level_fractional(LightData light,
                                       float3 lP,
                                       bool is_perspective,
                                       float distance_to_camera,
                                       float film_pixel_radius)
{
  float ratio = shadow_punctual_pixel_ratio(
      light, lP, is_perspective, distance_to_camera, film_pixel_radius);
  return clamp(-log2(ratio), 0.0f, float(SHADOW_TILEMAP_LOD));
}

int shadow_punctual_level(LightData light,
                          float3 lP,
                          bool is_perspective,
                          float distance_to_camera,
                          float film_pixel_radius)
{
  /* Conversion to positive int is the same as floor. */
  return int(shadow_punctual_level_fractional(
      light, lP, is_perspective, distance_to_camera, film_pixel_radius));
}

struct ShadowCoordinates {
  /* Index of the tile-map to containing the tile. */
  int tilemap_index;
  /* Texel coordinates in [0..SHADOW_MAP_MAX_RES) range. */
  uint2 tilemap_texel;
  /* Tile coordinate in [0..SHADOW_TILEMAP_RES) range. */
  uint2 tilemap_tile;
};

/* Assumes tilemap_uv is already saturated. */
ShadowCoordinates shadow_coordinate_from_uvs(int tilemap_index, float2 tilemap_uv)
{
  ShadowCoordinates ret;
  ret.tilemap_index = tilemap_index;
  ret.tilemap_texel = uint2(tilemap_uv * (float(SHADOW_MAP_MAX_RES) - 1e-2f));
  ret.tilemap_tile = ret.tilemap_texel >> uint(SHADOW_PAGE_LOD);
  return ret;
}

/* Retain sign bit and avoid costly int division. */
int2 shadow_decompress_grid_offset(eLightType light_type,
                                   int2 offset_neg,
                                   int2 offset_pos,
                                   int level_relative)
{
  if (light_type == LIGHT_SUN_ORTHO) {
    return shadow_cascade_grid_offset(offset_pos, level_relative);
  }
  else {
    return (offset_pos >> level_relative) - (offset_neg >> level_relative);
  }
}

/**
 * \a lP shading point position in light space (`shadow_world_to_local(light, P)`).
 */
ShadowCoordinates shadow_directional_coordinates_at_level(LightData light, float3 lP, int level)
{
  /* This difference needs to be less than 32 for the later shift to be valid.
   * This is ensured by `ShadowDirectional::clipmap_level_range()`. */
  int level_relative = level - light_sun_data_get(light).clipmap_lod_min;
  int lod_relative = (light.type == LIGHT_SUN_ORTHO) ? light_sun_data_get(light).clipmap_lod_min :
                                                       level;
  /* Compute offset in tile. */
  int2 clipmap_offset = shadow_decompress_grid_offset(
      light.type,
      light_sun_data_get(light).clipmap_base_offset_neg,
      light_sun_data_get(light).clipmap_base_offset_pos,
      level_relative);
  /* UV in [0..1] range over the tilemap. */
  float2 tilemap_uv = lP.xy - light_sun_data_get(light).clipmap_origin;
  tilemap_uv *= exp2(float(-lod_relative));
  tilemap_uv -= float2(clipmap_offset) * (1.0f / float(SHADOW_TILEMAP_RES));
  tilemap_uv = saturate(tilemap_uv + 0.5f);

  return shadow_coordinate_from_uvs(light.tilemap_index + level_relative, tilemap_uv);
}

/**
 * \a lP shading point position in light space (`shadow_world_to_local(light, P)`).
 */
ShadowCoordinates shadow_directional_coordinates(LightData light, float3 lP)
{
  int level = shadow_directional_level(light, lP - light_position_get(light));
  return shadow_directional_coordinates_at_level(light, lP, level);
}

/* Transform vector to face local coordinate. */
float3 shadow_punctual_local_position_to_face_local(int face_id, float3 lL)
{
  switch (face_id) {
    case 1:
      return float3(-lL.y, lL.z, -lL.x);
    case 2:
      return float3(lL.y, lL.z, lL.x);
    case 3:
      return float3(lL.x, lL.z, -lL.y);
    case 4:
      return float3(-lL.x, lL.z, lL.y);
    case 5:
      return float3(lL.x, -lL.y, -lL.z);
    default:
      return lL;
  }
}

float3 shadow_punctual_face_local_to_local_position(int face_id, float3 fL)
{
  switch (face_id) {
    case 1:
      return float3(-fL.z, -fL.x, fL.y);
    case 2:
      return float3(fL.z, fL.x, fL.y);
    case 3:
      return float3(fL.x, -fL.z, fL.y);
    case 4:
      return float3(-fL.x, fL.z, fL.y);
    case 5:
      return float3(fL.x, -fL.y, -fL.z);
    default:
      return fL;
  }
}

/* Turns local light coordinate into shadow region index. Matches eCubeFace order.
 * \note lL does not need to be normalized. */
int shadow_punctual_face_index_get(float3 lL)
{
  float3 aP = abs(lL);
  if (all(greaterThan(aP.xx, aP.yz))) {
    return (lL.x > 0.0f) ? 1 : 2;
  }
  else if (all(greaterThan(aP.yy, aP.xz))) {
    return (lL.y > 0.0f) ? 3 : 4;
  }
  else {
    return (lL.z > 0.0f) ? 5 : 0;
  }
}

/**
 * \a lP shading point position in face local space (world unit).
 * \a face_id is the one used to rotate lP using shadow_punctual_local_position_to_face_local().
 */
ShadowCoordinates shadow_punctual_coordinates(LightData light, float3 lP, int face_id)
{
  /* UVs in [-1..+1] range. */
  float2 tilemap_uv = lP.xy / abs(lP.z);
  /* UVs in [0..1] range. */
  tilemap_uv = saturate(tilemap_uv * 0.5f + 0.5f);

  return shadow_coordinate_from_uvs(light.tilemap_index + face_id, tilemap_uv);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Frustum shapes.
 * \{ */

float3 shadow_tile_corner_persp(ShadowTileMapData tilemap, int2 tile)
{
  return tilemap.corners[1].xyz + tilemap.corners[2].xyz * float(tile.x) +
         tilemap.corners[3].xyz * float(tile.y);
}

Pyramid shadow_tilemap_cubeface_bounds(ShadowTileMapData tilemap,
                                       int2 tile_start,
                                       const int2 extent)
{
  Pyramid shape;
  shape.corners[0] = tilemap.corners[0].xyz;
  shape.corners[1] = shadow_tile_corner_persp(tilemap, tile_start + int2(0, 0));
  shape.corners[2] = shadow_tile_corner_persp(tilemap, tile_start + int2(extent.x, 0));
  shape.corners[3] = shadow_tile_corner_persp(tilemap, tile_start + extent);
  shape.corners[4] = shadow_tile_corner_persp(tilemap, tile_start + int2(0, extent.y));
  return shape;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Render map layout.
 *
 * Since a view can cover at most the number of tile contained in LOD0,
 * index every LOD like they were LOD0.
 * \{ */

int shadow_render_page_index_get(int view_index, int2 tile_coordinate_in_lod)
{
  return view_index * SHADOW_TILEMAP_LOD0_LEN + tile_coordinate_in_lod.y * SHADOW_TILEMAP_RES +
         tile_coordinate_in_lod.x;
}

/** \} */
