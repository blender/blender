/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

#define EEVEE_SHADOW_LIB

#ifdef SHADOW_READ_ATOMIC
#  define SHADOW_ATLAS_TYPE usampler2DArrayAtomic
#else
#  define SHADOW_ATLAS_TYPE usampler2DArray
#endif

float shadow_read_depth(SHADOW_ATLAS_TYPE atlas_tx,
                        usampler2D tilemaps_tx,
                        ShadowCoordinates coord)
{
  ShadowSamplingTile tile = shadow_tile_load(tilemaps_tx, coord.tilemap_tile, coord.tilemap_index);
  if (!tile.is_valid) {
    return -1.0;
  }
  /* Using bitwise ops is way faster than integer ops. */
  const uint page_shift = uint(SHADOW_PAGE_LOD);
  const uint page_mask = ~(0xFFFFFFFFu << uint(SHADOW_PAGE_LOD));

  uvec2 texel = coord.tilemap_texel;
  /* Shift LOD0 pixels so that they get wrapped at the right position for the given LOD. */
  texel += uvec2(tile.lod_offset << SHADOW_PAGE_LOD);
  /* Scale to LOD pixels (merge LOD0 pixels together) then mask to get pixel in page. */
  uvec2 texel_page = (texel >> tile.lod) & page_mask;
  texel = (uvec2(tile.page.xy) << page_shift) | texel_page;

  return uintBitsToFloat(texelFetch(atlas_tx, ivec3(ivec2(texel), tile.page.z), 0).r);
}

/* ---------------------------------------------------------------------- */
/** \name Shadow Sampling Functions
 * \{ */

float shadow_punctual_sample_get(SHADOW_ATLAS_TYPE atlas_tx,
                                 usampler2D tilemaps_tx,
                                 LightData light,
                                 vec3 P)
{
  vec3 shadow_position = light_local_data_get(light).shadow_position;
  vec3 lP = transform_point_inversed(light.object_to_world, P);
  lP -= shadow_position;
  int face_id = shadow_punctual_face_index_get(lP);
  lP = shadow_punctual_local_position_to_face_local(face_id, lP);
  ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);

  float radial_dist = shadow_read_depth(atlas_tx, tilemaps_tx, coord);
  if (radial_dist == -1.0) {
    return 1e10;
  }
  float receiver_dist = length(lP);
  float occluder_dist = radial_dist;
  return receiver_dist - occluder_dist;
}

float shadow_directional_sample_get(SHADOW_ATLAS_TYPE atlas_tx,
                                    usampler2D tilemaps_tx,
                                    LightData light,
                                    vec3 P)
{
  vec3 lP = transform_direction_transposed(light.object_to_world, P);
  ShadowCoordinates coord = shadow_directional_coordinates(light, lP);

  float depth = shadow_read_depth(atlas_tx, tilemaps_tx, coord);
  if (depth == -1.0) {
    return 1e10;
  }
  /* Use increasing distance from the light. */
  float receiver_dist = -lP.z - orderedIntBitsToFloat(light.clip_near);
  float occluder_dist = depth;
  return receiver_dist - occluder_dist;
}

float shadow_sample(const bool is_directional,
                    SHADOW_ATLAS_TYPE atlas_tx,
                    usampler2D tilemaps_tx,
                    LightData light,
                    vec3 P)
{
  if (is_directional) {
    return shadow_directional_sample_get(atlas_tx, tilemaps_tx, light, P);
  }
  else {
    return shadow_punctual_sample_get(atlas_tx, tilemaps_tx, light, P);
  }
}

/** \} */
