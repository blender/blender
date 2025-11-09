/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This contains the common logic used for tagging shadows for opaque and transparent receivers.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(eevee_shadow_tag_usage_surfels)
#endif

#include "draw_intersect_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_shadow_lib.glsl"

void shadow_tag_usage_tile(LightData light, uint2 tile_co, int lod, int tilemap_index)
{
  if (tilemap_index > light_tilemap_max_get(light)) {
    return;
  }

  tile_co >>= uint(lod);
  int tile_index = shadow_tile_offset(tile_co, tilemaps_buf[tilemap_index].tiles_index, lod);
  atomicOr(tiles_buf[tile_index], uint(SHADOW_IS_USED));
}

void shadow_tag_usage_tilemap_directional_at_level(uint l_idx, float3 P, int level)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  float3 lP = light_world_to_local_direction(light, P);

  level = clamp(
      level, light_sun_data_get(light).clipmap_lod_min, light_sun_data_get(light).clipmap_lod_max);

  ShadowCoordinates coord = shadow_directional_coordinates_at_level(light, lP, level);
  shadow_tag_usage_tile(light, coord.tilemap_tile, 0, coord.tilemap_index);
}

void shadow_tag_usage_tilemap_directional(
    uint l_idx, float3 P, float3 V, float radius, int lod_bias)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  float3 lP = light_world_to_local_direction(light, P);

  LightSunData sun = light_sun_data_get(light);

  if (radius == 0.0f) {
    int level = shadow_directional_level(light, lP - light_position_get(light));
    level = clamp(level + lod_bias, sun.clipmap_lod_min, sun.clipmap_lod_max);
    ShadowCoordinates coord = shadow_directional_coordinates_at_level(light, lP, level);
    shadow_tag_usage_tile(light, coord.tilemap_tile, 0, coord.tilemap_index);
  }
  else {
    float3 start_lP = light_world_to_local_direction(light, P - V * radius);
    float3 end_lP = light_world_to_local_direction(light, P + V * radius);
    int min_level = shadow_directional_level(light, start_lP - light_position_get(light));
    int max_level = shadow_directional_level(light, end_lP - light_position_get(light));
    min_level = clamp(min_level + lod_bias, sun.clipmap_lod_min, sun.clipmap_lod_max);
    max_level = clamp(max_level + lod_bias, sun.clipmap_lod_min, sun.clipmap_lod_max);

    for (int level = min_level; level <= max_level; level++) {
      ShadowCoordinates coord_min = shadow_directional_coordinates_at_level(
          light, lP - float3(radius, radius, 0.0f), level);
      ShadowCoordinates coord_max = shadow_directional_coordinates_at_level(
          light, lP + float3(radius, radius, 0.0f), level);

      for (uint x = coord_min.tilemap_tile.x; x <= coord_max.tilemap_tile.x; x++) {
        for (uint y = coord_min.tilemap_tile.y; y <= coord_max.tilemap_tile.y; y++) {
          shadow_tag_usage_tile(light, uint2(x, y), 0, coord_min.tilemap_index);
        }
      }
    }
  }
}

void shadow_tag_usage_tilemap_punctual(uint l_idx, float3 P, float radius, int lod_bias)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  float3 lP = light_world_to_local_point(light, P);
  float dist_to_light = max(length(lP) - radius, 1e-5f);
  if (dist_to_light > light_local_data_get(light).influence_radius_max) {
    return;
  }
  if (is_spot_light(light.type)) {
    /* Early out if out of cone. */
    float angle_tan = length(lP.xy / dist_to_light);
    if (angle_tan > light_spot_data_get(light).spot_tan) {
      return;
    }
  }
  else if (is_area_light(light.type)) {
    /* Early out if on the wrong side. */
    if (lP.z - radius > 0.0f) {
      return;
    }
  }

  /* Transform to shadow local space. */
  lP -= light_local_data_get(light).shadow_position;

  int lod = shadow_punctual_level(light,
                                  lP,
                                  drw_view_is_perspective(),
                                  drw_view_z_distance(P),
                                  uniform_buf.shadow.film_pixel_radius);
  lod = clamp(lod + lod_bias, 0, SHADOW_TILEMAP_LOD);

  if (radius == 0) {
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);
    shadow_tag_usage_tile(light, coord.tilemap_tile, lod, coord.tilemap_index);
  }
  else {
    uint faces = 0u;
    for (int x = -1; x <= 1; x += 2) {
      for (int y = -1; y <= 1; y += 2) {
        for (int z = -1; z <= 1; z += 2) {
          float3 _lP = lP + float3(x, y, z) * radius;
          faces |= 1u << shadow_punctual_face_index_get(_lP);
        }
      }
    }

    for (int face_id = 0; face_id < 6; face_id++) {
      if ((faces & (1u << uint(face_id))) == 0u) {
        continue;
      }

      int tilemap_index = light.tilemap_index + face_id;
      float3 _lP = shadow_punctual_local_position_to_face_local(face_id, lP);

      float3 offset = float3(radius, radius, 0);
      ShadowCoordinates coord_min = shadow_punctual_coordinates(light, _lP - offset, face_id);
      ShadowCoordinates coord_max = shadow_punctual_coordinates(light, _lP + offset, face_id);

      for (uint x = coord_min.tilemap_tile.x; x <= coord_max.tilemap_tile.x; x++) {
        for (uint y = coord_min.tilemap_tile.y; y <= coord_max.tilemap_tile.y; y++) {
          shadow_tag_usage_tile(light, uint2(x, y), lod, tilemap_index);
        }
      }
    }
  }
}

/**
 * \a radius Radius of the tagging area in world space.
 * Used for downsampled/ray-marched tagging, so all the shadow-map texels covered get correctly
 * tagged.
 */
void shadow_tag_usage(float3 vP, float3 P, float3 V, float radius, float2 pixel, int lod_bias)
{
  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    shadow_tag_usage_tilemap_directional(l_idx, P, V, radius, lod_bias);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx) {
    shadow_tag_usage_tilemap_punctual(l_idx, P, radius, lod_bias);
  }
  LIGHT_FOREACH_END
}

void shadow_tag_usage(float3 vP, float3 P, float2 pixel)
{
  shadow_tag_usage(vP, P, float3(0), 0, pixel, 0);
}

void shadow_tag_usage_surfel(Surfel surfel, int directional_lvl)
{
  float3 P = surfel.position;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    shadow_tag_usage_tilemap_directional_at_level(l_idx, P, directional_lvl);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    shadow_tag_usage_tilemap_punctual(l_idx, P, 0, 0);
  }
  LIGHT_FOREACH_END
}
