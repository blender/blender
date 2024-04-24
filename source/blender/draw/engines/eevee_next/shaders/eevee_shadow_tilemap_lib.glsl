/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_shape_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Tile-map data
 * \{ */

int shadow_tile_index(ivec2 tile)
{
  return tile.x + tile.y * SHADOW_TILEMAP_RES;
}

ivec2 shadow_tile_coord(int tile_index)
{
  return ivec2(tile_index % SHADOW_TILEMAP_RES, tile_index / SHADOW_TILEMAP_RES);
}

/* Return bottom left pixel position of the tile-map inside the tile-map atlas. */
ivec2 shadow_tilemap_start(int tilemap_index)
{
  return SHADOW_TILEMAP_RES *
         ivec2(tilemap_index % SHADOW_TILEMAP_PER_ROW, tilemap_index / SHADOW_TILEMAP_PER_ROW);
}

ivec2 shadow_tile_coord_in_atlas(ivec2 tile, int tilemap_index)
{
  return shadow_tilemap_start(tilemap_index) + tile;
}

/**
 * Return tile index inside `tiles_buf` for a given tile coordinate inside a specific LOD.
 * `tiles_index` should be `ShadowTileMapData.tiles_index`.
 */
int shadow_tile_offset(ivec2 tile, int tiles_index, int lod)
{
#if SHADOW_TILEMAP_LOD > 5
#  error This needs to be adjusted
#endif
  const int lod0_width = SHADOW_TILEMAP_RES / 1;
  const int lod1_width = SHADOW_TILEMAP_RES / 2;
  const int lod2_width = SHADOW_TILEMAP_RES / 4;
  const int lod3_width = SHADOW_TILEMAP_RES / 8;
  const int lod4_width = SHADOW_TILEMAP_RES / 16;
  const int lod5_width = SHADOW_TILEMAP_RES / 32;
  const int lod0_size = lod0_width * lod0_width;
  const int lod1_size = lod1_width * lod1_width;
  const int lod2_size = lod2_width * lod2_width;
  const int lod3_size = lod3_width * lod3_width;
  const int lod4_size = lod4_width * lod4_width;
  const int lod5_size = lod5_width * lod5_width;

  int offset = tiles_index;
  switch (lod) {
    case 5:
      offset += lod0_size + lod1_size + lod2_size + lod3_size + lod4_size;
      offset += tile.y * lod5_width;
      break;
    case 4:
      offset += lod0_size + lod1_size + lod2_size + lod3_size;
      offset += tile.y * lod4_width;
      break;
    case 3:
      offset += lod0_size + lod1_size + lod2_size;
      offset += tile.y * lod3_width;
      break;
    case 2:
      offset += lod0_size + lod1_size;
      offset += tile.y * lod2_width;
      break;
    case 1:
      offset += lod0_size;
      offset += tile.y * lod1_width;
      break;
    case 0:
    default:
      offset += tile.y * lod0_width;
      break;
  }
  offset += tile.x;
  return offset;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Load / Store functions.
 * \{ */

/** \note Will clamp if out of bounds. */
ShadowSamplingTile shadow_tile_load(usampler2D tilemaps_tx, ivec2 tile_co, int tilemap_index)
{
  /* NOTE(@fclem): This clamp can hide some small imprecision at clip-map transition.
   * Can be disabled to check if the clip-map is well centered. */
  tile_co = clamp(tile_co, ivec2(0), ivec2(SHADOW_TILEMAP_RES - 1));
  ivec2 texel = shadow_tile_coord_in_atlas(tile_co, tilemap_index);
  uint tile_data = texelFetch(tilemaps_tx, texel, 0).x;
  return shadow_sampling_tile_unpack(tile_data);
}

/**
 * This function should be the inverse of ShadowDirectional::coverage_get().
 *
 * \a lP shading point position in light space, relative to the to camera position snapped to
 * the smallest clip-map level (`shadow_world_to_local(light, P) - light._position`).
 */

float shadow_directional_level_fractional(LightData light, vec3 lP)
{
  float lod;
  if (light.type == LIGHT_SUN) {
    /* We need to hide one tile worth of data to hide the moving transition. */
    const float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 1.0001);
    /* Since the distance is centered around the camera (and thus by extension the tile-map),
     * we need to multiply by 2 to get the lod level which covers the following range:
     * [-coverage_get(lod)/2..coverage_get(lod)/2] */
    lod = log2(length(lP) * narrowing * 2.0);
  }
  else {
    /* The narrowing need to be stronger since the tile-map position is not rounded but floored. */
    const float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 2.5001);
    /* Since we want half of the size, bias the level by -1. */
    float lod_min_half_size = exp2(float(light_sun_data_get(light).clipmap_lod_min - 1));
    lod = length(lP.xy) * narrowing / lod_min_half_size;
  }
  float clipmap_lod = lod + light.lod_bias;
  return clamp(clipmap_lod,
               float(light_sun_data_get(light).clipmap_lod_min),
               float(light_sun_data_get(light).clipmap_lod_max));
}

int shadow_directional_level(LightData light, vec3 lP)
{
  return int(ceil(shadow_directional_level_fractional(light, lP)));
}

/* How much a tilemap pixel covers a final image pixel. */
float shadow_punctual_footprint_ratio(LightData light,
                                      vec3 P,
                                      bool is_perspective,
                                      float dist_to_cam,
                                      float tilemap_projection_ratio)
{
  /* We project a shadow map pixel (as a sphere for simplicity) to the receiver plane.
   * We then reproject this sphere onto the camera screen and compare it to the film pixel size.
   * This gives a good approximation of what LOD to select to get a somewhat uniform shadow map
   * resolution in screen space. */

  float dist_to_light = distance(P, light._position);
  float footprint_ratio = dist_to_light;
  /* Project the radius to the screen. 1 unit away from the camera the same way
   * pixel_world_radius_inv was computed. Not needed in orthographic mode. */
  if (is_perspective) {
    footprint_ratio /= dist_to_cam;
  }
  /* Apply resolution ratio. */
  footprint_ratio *= tilemap_projection_ratio;
  /* Take the frustum padding into account. */
  footprint_ratio *= light_local_data_get(light).clip_side /
                     orderedIntBitsToFloat(light.clip_near);
  return footprint_ratio;
}

struct ShadowCoordinates {
  /* Index of the tile-map to containing the tile. */
  int tilemap_index;
  /* LOD of the tile to load relative to the min level. Always positive. */
  int lod_relative;
  /* Tile coordinate inside the tile-map. */
  ivec2 tile_coord;
  /* UV coordinates in [0..SHADOW_TILEMAP_RES) range. */
  vec2 uv;
};

/* Retain sign bit and avoid costly int division. */
ivec2 shadow_decompress_grid_offset(eLightType light_type,
                                    ivec2 offset_neg,
                                    ivec2 offset_pos,
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
ShadowCoordinates shadow_directional_coordinates_at_level(LightData light, vec3 lP, int level)
{
  ShadowCoordinates ret;
  /* This difference needs to be less than 32 for the later shift to be valid.
   * This is ensured by `ShadowDirectional::clipmap_level_range()`. */
  int level_relative = level - light_sun_data_get(light).clipmap_lod_min;

  ret.tilemap_index = light.tilemap_index + level_relative;

  ret.lod_relative = (light.type == LIGHT_SUN_ORTHO) ? light_sun_data_get(light).clipmap_lod_min :
                                                       level;

  /* Compute offset in tile. */
  ivec2 clipmap_offset = shadow_decompress_grid_offset(
      light.type,
      light_sun_data_get(light).clipmap_base_offset_neg,
      light_sun_data_get(light).clipmap_base_offset_pos,
      level_relative);

  ret.uv = lP.xy - light_sun_data_get(light).clipmap_origin;
  ret.uv /= exp2(float(ret.lod_relative));
  ret.uv = ret.uv * float(SHADOW_TILEMAP_RES) + float(SHADOW_TILEMAP_RES / 2);
  ret.uv -= vec2(clipmap_offset);
  /* Clamp to avoid out of tile-map access. */
  ret.tile_coord = clamp(ivec2(ret.uv), ivec2(0.0), ivec2(SHADOW_TILEMAP_RES - 1));
  return ret;
}

/**
 * \a lP shading point position in light space (`shadow_world_to_local(light, P)`).
 */
ShadowCoordinates shadow_directional_coordinates(LightData light, vec3 lP)
{
  int level = shadow_directional_level(light, lP - light._position);
  return shadow_directional_coordinates_at_level(light, lP, level);
}

/* Transform vector to face local coordinate. */
vec3 shadow_punctual_local_position_to_face_local(int face_id, vec3 lL)
{
  switch (face_id) {
    case 1:
      return vec3(-lL.y, lL.z, -lL.x);
    case 2:
      return vec3(lL.y, lL.z, lL.x);
    case 3:
      return vec3(lL.x, lL.z, -lL.y);
    case 4:
      return vec3(-lL.x, lL.z, lL.y);
    case 5:
      return vec3(lL.x, -lL.y, -lL.z);
    default:
      return lL;
  }
}

vec3 shadow_punctual_face_local_to_local_position(int face_id, vec3 fL)
{
  switch (face_id) {
    case 1:
      return vec3(-fL.z, -fL.x, fL.y);
    case 2:
      return vec3(fL.z, fL.x, fL.y);
    case 3:
      return vec3(fL.x, -fL.z, fL.y);
    case 4:
      return vec3(-fL.x, fL.z, fL.y);
    case 5:
      return vec3(fL.x, -fL.y, -fL.z);
    default:
      return fL;
  }
}

/* Turns local light coordinate into shadow region index. Matches eCubeFace order.
 * \note lL does not need to be normalized. */
int shadow_punctual_face_index_get(vec3 lL)
{
  vec3 aP = abs(lL);
  if (all(greaterThan(aP.xx, aP.yz))) {
    return (lL.x > 0.0) ? 1 : 2;
  }
  else if (all(greaterThan(aP.yy, aP.xz))) {
    return (lL.y > 0.0) ? 3 : 4;
  }
  else {
    return (lL.z > 0.0) ? 5 : 0;
  }
}

/**
 * \a lP shading point position in face local space (world unit).
 * \a face_id is the one used to rotate lP using shadow_punctual_local_position_to_face_local().
 */
ShadowCoordinates shadow_punctual_coordinates(LightData light, vec3 lP, int face_id)
{
  float clip_near = intBitsToFloat(light.clip_near);
  float clip_side = light_local_data_get(light).clip_side;

  ShadowCoordinates ret;
  ret.tilemap_index = light.tilemap_index + face_id;
  /* UVs in [-1..+1] range. */
  ret.uv = (lP.xy * clip_near) / abs(lP.z * clip_side);
  /* UVs in [0..SHADOW_TILEMAP_RES] range. */
  ret.uv = ret.uv * float(SHADOW_TILEMAP_RES / 2) + float(SHADOW_TILEMAP_RES / 2);
  /* Clamp to avoid out of tile-map access. */
  ret.tile_coord = clamp(ivec2(ret.uv), ivec2(0), ivec2(SHADOW_TILEMAP_RES - 1));
  return ret;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Frustum shapes.
 * \{ */

vec3 shadow_tile_corner_persp(ShadowTileMapData tilemap, ivec2 tile)
{
  return tilemap.corners[1].xyz + tilemap.corners[2].xyz * float(tile.x) +
         tilemap.corners[3].xyz * float(tile.y);
}

Pyramid shadow_tilemap_cubeface_bounds(ShadowTileMapData tilemap,
                                       ivec2 tile_start,
                                       const ivec2 extent)
{
  Pyramid shape;
  shape.corners[0] = tilemap.corners[0].xyz;
  shape.corners[1] = shadow_tile_corner_persp(tilemap, tile_start + ivec2(0, 0));
  shape.corners[2] = shadow_tile_corner_persp(tilemap, tile_start + ivec2(extent.x, 0));
  shape.corners[3] = shadow_tile_corner_persp(tilemap, tile_start + extent);
  shape.corners[4] = shadow_tile_corner_persp(tilemap, tile_start + ivec2(0, extent.y));
  return shape;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Render map layout.
 *
 * Since a view can cover at most the number of tile contained in LOD0,
 * index every LOD like they were LOD0.
 * \{ */

int shadow_render_page_index_get(int view_index, ivec2 tile_coordinate_in_lod)
{
  return view_index * SHADOW_TILEMAP_LOD0_LEN + tile_coordinate_in_lod.y * SHADOW_TILEMAP_RES +
         tile_coordinate_in_lod.x;
}

/** \} */
