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

struct ShadowSampleParams {
  vec3 lP;
  vec3 uv;
  int tilemap_index;
  float z_range;
};

ShadowSamplingTile shadow_tile_data_get(usampler2D tilemaps_tx, ShadowSampleParams params)
{
  /* Prevent out of bound access. Assumes the input is already non negative. */
  vec2 tilemap_uv = min(params.uv.xy, vec2(0.99999));

  ivec2 texel_coord = ivec2(tilemap_uv * float(SHADOW_MAP_MAX_RES));
  /* Using bitwise ops is way faster than integer ops. */
  const int page_shift = SHADOW_PAGE_LOD;

  ivec2 tile_coord = texel_coord >> page_shift;
  return shadow_tile_load(tilemaps_tx, tile_coord, params.tilemap_index);
}

float shadow_read_depth(SHADOW_ATLAS_TYPE atlas_tx,
                        usampler2D tilemaps_tx,
                        ShadowSampleParams params)
{
  /* Prevent out of bound access. Assumes the input is already non negative. */
  vec2 tilemap_uv = min(params.uv.xy, vec2(0.99999));

  ivec2 texel_coord = ivec2(tilemap_uv * float(SHADOW_MAP_MAX_RES));
  /* Using bitwise ops is way faster than integer ops. */
  const int page_shift = SHADOW_PAGE_LOD;

  ivec2 tile_coord = texel_coord >> page_shift;
  ShadowSamplingTile tile = shadow_tile_load(tilemaps_tx, tile_coord, params.tilemap_index);

  if (!tile.is_valid) {
    return -1.0;
  }

  int page_mask = ~(0xFFFFFFFF << (SHADOW_PAGE_LOD + int(tile.lod)));
  ivec2 texel_page = (texel_coord & page_mask) >> int(tile.lod);
  ivec3 texel = ivec3((ivec2(tile.page.xy) << page_shift) | texel_page, tile.page.z);

  return uintBitsToFloat(texelFetch(atlas_tx, texel, 0).r);
}

struct ShadowEvalResult {
  /* Visibility of the light. */
  float light_visibilty;
  /* Average occluder distance. In world space linear distance. */
  float occluder_distance;
};

/* ---------------------------------------------------------------------- */
/** \name Shadow Sampling Functions
 * \{ */

mat4x4 shadow_projection_perspective(float side, float near_clip, float far_clip)
{
  float z_delta = far_clip - near_clip;

  mat4x4 mat = mat4x4(1.0);
  mat[0][0] = near_clip / side;
  mat[1][1] = near_clip / side;
  mat[2][0] = 0.0;
  mat[2][1] = 0.0;
  mat[2][2] = -(far_clip + near_clip) / z_delta;
  mat[2][3] = -1.0;
  mat[3][2] = (-2.0 * near_clip * far_clip) / z_delta;
  mat[3][3] = 0.0;
  return mat;
}

mat4x4 shadow_projection_perspective_inverse(float side, float near_clip, float far_clip)
{
  float z_delta = far_clip - near_clip;
  float d = 2.0 * near_clip * far_clip;

  mat4x4 mat = mat4x4(1.0);
  mat[0][0] = side / near_clip;
  mat[1][1] = side / near_clip;
  mat[2][0] = 0.0;
  mat[2][1] = 0.0;
  mat[2][2] = 0.0;
  mat[2][3] = (near_clip - far_clip) / d;
  mat[3][2] = -1.0;
  mat[3][3] = (near_clip + far_clip) / d;
  return mat;
}

/**
 * Convert occluder distance in shadow space to world space distance.
 * Assuming the occluder is above the shading point in direction to the shadow projection center.
 */
float shadow_linear_occluder_distance(LightData light,
                                      const bool is_directional,
                                      vec3 lP,
                                      float occluder)
{
  float near = orderedIntBitsToFloat(light.clip_near);
  float far = orderedIntBitsToFloat(light.clip_far);

  float occluder_z = (is_directional) ? (occluder * (far - near) + near) :
                                        ((near * far) / (occluder * (near - far) + far));
  float receiver_z = (is_directional) ? -lP.z : reduce_max(abs(lP));
  if (!is_directional) {
    float lP_len = length(lP);
    return lP_len - lP_len * (occluder_z / receiver_z);
  }
  return receiver_z - occluder_z;
}

mat4 shadow_punctual_projection_perspective(LightData light)
{
  /* Face Local (View) Space > Clip Space. */
  float clip_far = intBitsToFloat(light.clip_far);
  float clip_near = intBitsToFloat(light.clip_near);
  float clip_side = light_local_data_get(light).clip_side;
  return shadow_projection_perspective(clip_side, clip_near, clip_far);
}

mat4 shadow_punctual_projection_perspective_inverse(LightData light)
{
  /* Face Local (View) Space > Clip Space. */
  float clip_far = intBitsToFloat(light.clip_far);
  float clip_near = intBitsToFloat(light.clip_near);
  float clip_side = light_local_data_get(light).clip_side;
  return shadow_projection_perspective_inverse(clip_side, clip_near, clip_far);
}

vec3 shadow_punctual_reconstruct_position(ShadowSampleParams params,
                                          mat4 wininv,
                                          LightData light,
                                          vec3 uvw)
{
  vec3 clip_P = uvw * 2.0 - 1.0;
  vec3 lP = project_point(wininv, clip_P);
  int face_id = params.tilemap_index - light.tilemap_index;
  lP = shadow_punctual_face_local_to_local_position(face_id, lP);
  return transform_point(light.object_to_world, lP);
}

ShadowSampleParams shadow_punctual_sample_params_get(LightData light, vec3 P)
{
  vec3 lP = transform_point_inversed(light.object_to_world, P);

  int face_id = shadow_punctual_face_index_get(lP);
  /* Local Light Space > Face Local (View) Space. */
  lP = shadow_punctual_local_position_to_face_local(face_id, lP);
  mat4 winmat = shadow_punctual_projection_perspective(light);
  vec3 clip_P = project_point(winmat, lP);
  /* Clip Space > UV Space. */
  vec3 uv_P = saturate(clip_P * 0.5 + 0.5);

  ShadowSampleParams result;
  result.lP = lP;
  result.uv = uv_P;
  result.tilemap_index = light.tilemap_index + face_id;
  result.z_range = 1.0;
  return result;
}

ShadowEvalResult shadow_punctual_sample_get(SHADOW_ATLAS_TYPE atlas_tx,
                                            usampler2D tilemaps_tx,
                                            LightData light,
                                            vec3 P)
{
  ShadowSampleParams params = shadow_punctual_sample_params_get(light, P);

  float depth = shadow_read_depth(atlas_tx, tilemaps_tx, params);

  ShadowEvalResult result;
  result.light_visibilty = float(params.uv.z < depth);
  result.occluder_distance = shadow_linear_occluder_distance(light, false, params.lP, depth);
  return result;
}

struct ShadowDirectionalSampleInfo {
  float clip_near;
  float clip_far;
  int level_relative;
  int lod_relative;
  ivec2 clipmap_offset;
  vec2 clipmap_origin;
};

ShadowDirectionalSampleInfo shadow_directional_sample_info_get(LightData light, vec3 lP)
{
  ShadowDirectionalSampleInfo info;
  info.clip_near = orderedIntBitsToFloat(light.clip_near);
  info.clip_far = orderedIntBitsToFloat(light.clip_far);

  int level = shadow_directional_level(light, lP - light_position_get(light));
  /* This difference needs to be less than 32 for the later shift to be valid.
   * This is ensured by ShadowDirectional::clipmap_level_range(). */
  info.level_relative = level - light_sun_data_get(light).clipmap_lod_min;
  info.lod_relative = (light.type == LIGHT_SUN_ORTHO) ? light_sun_data_get(light).clipmap_lod_min :
                                                        level;

  info.clipmap_offset = shadow_decompress_grid_offset(
      light.type,
      light_sun_data_get(light).clipmap_base_offset_neg,
      light_sun_data_get(light).clipmap_base_offset_pos,
      info.level_relative);
  info.clipmap_origin = light_sun_data_get(light).clipmap_origin;

  return info;
}

vec3 shadow_directional_reconstruct_position(ShadowSampleParams params, LightData light, vec3 uvw)
{
  ShadowDirectionalSampleInfo info = shadow_directional_sample_info_get(light, params.lP);

  vec2 tilemap_uv = uvw.xy;
  tilemap_uv += vec2(info.clipmap_offset) / float(SHADOW_TILEMAP_RES);
  vec2 clipmap_pos = (tilemap_uv - 0.5) / exp2(-float(info.lod_relative));

  vec3 lP;
  lP.xy = clipmap_pos + info.clipmap_origin;
  lP.z = (params.uv.z + info.clip_near) * -1.0;

  return transform_direction_transposed(light.object_to_world, lP);
}

ShadowSampleParams shadow_directional_sample_params_get(usampler2D tilemaps_tx,
                                                        LightData light,
                                                        vec3 P)
{
  vec3 lP = transform_direction(light.object_to_world, P);
  ShadowDirectionalSampleInfo info = shadow_directional_sample_info_get(light, lP);

  ShadowCoordinates coord = shadow_directional_coordinates(light, lP);

  /* Assumed to be non-null. */
  float z_range = info.clip_far - info.clip_near;
  float dist_to_near_plane = -lP.z - info.clip_near;

  vec2 clipmap_pos = lP.xy - info.clipmap_origin;
  vec2 tilemap_uv = clipmap_pos * exp2(-float(info.lod_relative)) + 0.5;

  /* Translate tilemap UVs to its origin. */
  tilemap_uv -= vec2(info.clipmap_offset) / float(SHADOW_TILEMAP_RES);
  /* Clamp to avoid out of tilemap access. */
  tilemap_uv = saturate(tilemap_uv);

  ShadowSampleParams result;
  result.lP = lP;
  result.uv = vec3(tilemap_uv, dist_to_near_plane);
  result.tilemap_index = light.tilemap_index + info.level_relative;
  result.z_range = z_range;
  return result;
}

ShadowEvalResult shadow_directional_sample_get(SHADOW_ATLAS_TYPE atlas_tx,
                                               usampler2D tilemaps_tx,
                                               LightData light,
                                               vec3 P)
{
  ShadowSampleParams params = shadow_directional_sample_params_get(tilemaps_tx, light, P);

  float depth = shadow_read_depth(atlas_tx, tilemaps_tx, params);

  ShadowEvalResult result;
  result.light_visibilty = float(params.uv.z < depth * params.z_range);
  result.occluder_distance = shadow_linear_occluder_distance(light, true, params.lP, depth);
  return result;
}

ShadowEvalResult shadow_sample(const bool is_directional,
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
