/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Setup tile-map positioning for each shadow casting light.
 * Dispatched one thread per light.
 */

#include "infos/eevee_light_culling_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_light_shadow_setup)

#include "eevee_sampling_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_matrix_projection_lib.glsl"

int shadow_directional_coverage_get(int level)
{
  return 1 << level;
}

void orthographic_sync(int tilemap_id,
                       Transform object_to_world,
                       int2 origin_offset,
                       int clipmap_level,
                       eShadowProjectionType projection_type)
{
  /* Do not check translation. */
  object_to_world.x.w = 0.0f;
  object_to_world.y.w = 0.0f;
  object_to_world.z.w = 0.0f;

  int clip_index = tilemaps_buf[tilemap_id].clip_data_index;
  /* Avoid qualifier problems on NVidia (see #121968). */
  Transform object_to_world_history = tilemaps_clip_buf[clip_index].object_to_world;

  if (tilemaps_buf[tilemap_id].is_dirty ||
      !transform_equal(object_to_world_history, object_to_world))
  {
    /* Set dirty as the light direction changed. */
    tilemaps_buf[tilemap_id].grid_shift = int2(SHADOW_TILEMAP_RES);
    tilemaps_clip_buf[clip_index].object_to_world = object_to_world;
  }
  else {
    /* Same light direction but camera might have moved. Shift tilemap grid. */
    tilemaps_buf[tilemap_id].grid_shift = origin_offset -
                                          tilemaps_clip_buf[clip_index].grid_offset;
  }
  tilemaps_clip_buf[clip_index].grid_offset = origin_offset;
  /* TODO(fclem): Remove this duplicate. Only needed because of the base offset packing. */
  tilemaps_buf[tilemap_id].grid_offset = origin_offset;

  float level_size = shadow_directional_coverage_get(clipmap_level);
  float half_size = level_size / 2.0f;
  float tile_size = level_size / float(SHADOW_TILEMAP_RES);
  float2 center_offset = float2(origin_offset) * tile_size;

  /* object_mat is a rotation matrix. Reduce imprecision by taking the transpose which is also the
   * inverse in this particular case. */
  tilemaps_buf[tilemap_id].viewmat[0] = float4(object_to_world.x.xyz, 0.0f);
  tilemaps_buf[tilemap_id].viewmat[1] = float4(object_to_world.y.xyz, 0.0f);
  tilemaps_buf[tilemap_id].viewmat[2] = float4(object_to_world.z.xyz, 0.0f);
  tilemaps_buf[tilemap_id].viewmat[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);

  tilemaps_buf[tilemap_id].projection_type = projection_type;
  tilemaps_buf[tilemap_id].half_size = half_size;
  tilemaps_buf[tilemap_id].center_offset = center_offset;
  tilemaps_buf[tilemap_id].winmat = projection_orthographic(
      -half_size + center_offset.x,
      half_size + center_offset.x,
      -half_size + center_offset.y,
      half_size + center_offset.y,
      /* Near/far is computed on GPU using casters bounds. */
      -1.0f,
      1.0f);
}

void cascade_sync(inout LightData light)
{
  int level_min = light_sun_data_get(light).clipmap_lod_min;
  int level_max = light_sun_data_get(light).clipmap_lod_max;
  int level_range = level_max - level_min;
  int level_len = level_range + 1;

  float3 ws_camera_position = uniform_buf.camera.viewinv[3].xyz;
  float3 ws_camera_forward = uniform_buf.camera.viewinv[2].xyz;
  float camera_clip_near = uniform_buf.camera.clip_near;
  float camera_clip_far = uniform_buf.camera.clip_far;

  /* All tile-maps use the first level size. */
  float level_size = shadow_directional_coverage_get(level_min);
  float half_size = level_size / 2.0f;
  float tile_size = level_size / float(SHADOW_TILEMAP_RES);

  /* Ideally we should only take the intersection with the scene bounds. */
  float3 ws_far_point = ws_camera_position - ws_camera_forward * camera_clip_far;
  float3 ws_near_point = ws_camera_position - ws_camera_forward * camera_clip_near;

  float3 ls_far_point = transform_direction_transposed(light.object_to_world, ws_far_point);
  float3 ls_near_point = transform_direction_transposed(light.object_to_world, ws_near_point);

  float2 local_view_direction = normalize(ls_far_point.xy - ls_near_point.xy);
  float2 farthest_tilemap_center = local_view_direction * half_size * level_range;

  /* Offset for smooth level transitions. */
  light.object_to_world.x.w = ls_near_point.x;
  light.object_to_world.y.w = ls_near_point.y;
  light.object_to_world.z.w = ls_near_point.z;

  /* Offset in tiles from the scene origin to the center of the first tile-maps. */
  int2 origin_offset = int2(round(ls_near_point.xy / tile_size));
  /* Offset in tiles between the first and the last tile-maps. */
  int2 offset_vector = int2(round(farthest_tilemap_center / tile_size));

  int2 base_offset_pos = (offset_vector * (1 << 16)) / max(level_range, 1);

  /* \note cascade_level_range starts the range at the unique LOD to apply to all tile-maps. */
  for (int i = 0; i < level_len; i++) {
    /* Equal spacing between cascades layers since we want uniform shadow density. */
    int2 level_offset = origin_offset + shadow_cascade_grid_offset(base_offset_pos, i);

    orthographic_sync(light.tilemap_index + i,
                      light.object_to_world,
                      level_offset,
                      level_min,
                      SHADOW_PROJECTION_CASCADE);
  }

  float2 clipmap_origin = float2(origin_offset) * tile_size;

  LightSunData sun_data = light_sun_data_get(light);
  /* Used as origin for the clipmap_base_offset trick. */
  sun_data.clipmap_origin = clipmap_origin;
  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  sun_data.clipmap_base_offset_pos = base_offset_pos;
  sun_data.clipmap_base_offset_neg = int2(0);

  light = light_sun_data_set(light, sun_data);
}

void clipmap_sync(inout LightData light)
{
  float3 ws_camera_position = uniform_buf.camera.viewinv[3].xyz;
  float3 ls_camera_position = transform_direction_transposed(light.object_to_world,
                                                             ws_camera_position);

  int level_min = light_sun_data_get(light).clipmap_lod_min;
  int level_max = light_sun_data_get(light).clipmap_lod_max;
  int level_len = level_max - level_min + 1;

  float2 clipmap_origin;
  for (int lod = 0; lod < level_len; lod++) {
    int level = level_min + lod;
    /* Compute full offset from world origin to the smallest clipmap tile centered around the
     * camera position. The offset is computed in smallest tile unit. */
    float tile_size = float(1 << level) / float(SHADOW_TILEMAP_RES);
    int2 level_offset = int2(round(ls_camera_position.xy / tile_size));

    orthographic_sync(light.tilemap_index + lod,
                      light.object_to_world,
                      level_offset,
                      level,
                      SHADOW_PROJECTION_CLIPMAP);

    clipmap_origin = float2(level_offset) * tile_size;
  }

  int2 pos_offset = int2(0);
  int2 neg_offset = int2(0);
  for (int lod = 0; lod < level_len - 1; lod++) {
    /* Since offset can only differ by one tile from the higher level, we can compress that as a
     * single integer where one bit contains offset between 2 levels. Then a single bit shift in
     * the shader gives the number of tile to offset in the given tile-map space. However we need
     * also the sign of the offset for each level offset. To this end, we split the negative
     * offsets to a separate int. */
    int2 lvl_offset_next = tilemaps_buf[light.tilemap_index + lod + 1].grid_offset;
    int2 lvl_offset = tilemaps_buf[light.tilemap_index + lod].grid_offset;
    int2 lvl_delta = lvl_offset - (lvl_offset_next * 2);
    pos_offset |= max(lvl_delta, int2(0)) << lod;
    neg_offset |= max(-lvl_delta, int2(0)) << lod;
  }

  /* Used for selecting the clipmap level. */
  light.object_to_world.x.w = ls_camera_position.x;
  light.object_to_world.y.w = ls_camera_position.y;
  light.object_to_world.z.w = ls_camera_position.z;

  LightSunData sun_data = light_sun_data_get(light);
  /* Used as origin for the clipmap_base_offset trick. */
  sun_data.clipmap_origin = clipmap_origin;
  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  sun_data.clipmap_base_offset_pos = pos_offset;
  sun_data.clipmap_base_offset_neg = neg_offset;

  light = light_sun_data_set(light, sun_data);
}

void cubeface_sync(int tilemap_id,
                   Transform object_to_world,
                   eCubeFace cubeface,
                   float3 jitter_offset)
{
  float3 world_jitter_offset = transform_point(object_to_world, jitter_offset);
  object_to_world.x.w = world_jitter_offset.x;
  object_to_world.y.w = world_jitter_offset.y;
  object_to_world.z.w = world_jitter_offset.z;

  int clip_index = tilemaps_buf[tilemap_id].clip_data_index;
  /* Avoid qualifier problems on NVidia (see #121968). */
  Transform object_to_world_history = tilemaps_clip_buf[clip_index].object_to_world;
  if (tilemaps_buf[tilemap_id].is_dirty ||
      !transform_equal(object_to_world_history, object_to_world))
  {
    /* Set dirty as the light direction changed. */
    tilemaps_buf[tilemap_id].grid_shift = int2(SHADOW_TILEMAP_RES);
    tilemaps_clip_buf[clip_index].object_to_world = object_to_world;
  }

  /* Update View Matrix. */
  /* TODO(fclem): Could avoid numerical inversion since the transform is a unit matrix. */
  float4x4 viewmat = invert(transform_to_matrix(object_to_world));

  /* Use switch instead of inline array of float3x3. */
  switch (cubeface) {
    case Z_NEG:
      viewmat = to_float4x4(float3x3(+1, +0, +0, +0, +1, +0, +0, +0, +1)) * viewmat;
      break;
    case X_POS:
      viewmat = to_float4x4(float3x3(+0, +0, -1, -1, +0, +0, +0, +1, +0)) * viewmat;
      break;
    case X_NEG:
      viewmat = to_float4x4(float3x3(+0, +0, +1, +1, +0, +0, +0, +1, +0)) * viewmat;
      break;
    case Y_POS:
      viewmat = to_float4x4(float3x3(+1, +0, +0, +0, +0, -1, +0, +1, +0)) * viewmat;
      break;
    case Y_NEG:
      viewmat = to_float4x4(float3x3(-1, +0, +0, +0, +0, +1, +0, +1, +0)) * viewmat;
      break;
    case Z_POS:
      viewmat = to_float4x4(float3x3(+1, +0, +0, +0, -1, +0, +0, +0, -1)) * viewmat;
      break;
  }

  tilemaps_buf[tilemap_id].viewmat = viewmat;

  /* Update corners. */
  tilemaps_buf[tilemap_id].corners[0].xyz += jitter_offset;
  tilemaps_buf[tilemap_id].corners[1].xyz += jitter_offset;
  /* Other corners are deltas. They do not change after jitter. */
}

void main()
{
  uint l_idx = gl_GlobalInvocationID.x;
  /* We are dispatching (with padding) over the culled light array.
   * This array is not contiguous. Visible local lights are packed at the beginning
   * and directional lights at the end. There is a range of uninitialized value in between that
   * needs to be avoided. */
  bool valid_local = (l_idx < light_cull_buf.visible_count);
  bool valid_directional = (l_idx >= light_cull_buf.local_lights_len) &&
                           (l_idx < light_cull_buf.items_count);
  if (!valid_local && !valid_directional) {
    return;
  }

  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  if (is_sun_light(light.type)) {
    /* Distant lights. */

    if (light.shadow_jitter && uniform_buf.shadow.use_jitter) {
      /* TODO(fclem): Remove atan here. We only need the cosine of the angle. */
      float shape_angle = atan_fast(light_sun_data_get(light).shape_radius);

      /* Reverse to that first sample is straight up. */
      float2 rand = 1.0f - sampling_rng_2D_get(SAMPLING_SHADOW_I);
      float3 shadow_direction = sample_uniform_cone(rand, cos(shape_angle));

      shadow_direction = transform_direction(light.object_to_world, shadow_direction);

      if (light_sun_data_get(light).shadow_angle == 0.0f) {
        /* The shape is a point. There is nothing to jitter.
         * `shape_radius` is clamped to a minimum for precision reasons, so `shadow_angle` is
         * set to 0 only when the light radius is also 0 to detect this case. */
      }
      else {
        light.object_to_world = transform_from_matrix(to_float4x4(from_up_axis(shadow_direction)));
      }
    }

    if (light.type == LIGHT_SUN_ORTHO) {
      cascade_sync(light);
    }
    else {
      clipmap_sync(light);
    }
  }
  else {
    /* Local lights. */
    float3 position_on_light = float3(0.0f);

    if (light.shadow_jitter && uniform_buf.shadow.use_jitter) {
      float3 rand = sampling_rng_3D_get(SAMPLING_SHADOW_I);

      if (is_area_light(light.type)) {
        float2 point_on_unit_shape = (light.type == LIGHT_RECT) ? rand.xy * 2.0f - 1.0f :
                                                                  sample_disk(rand.xy);
        position_on_light = float3(point_on_unit_shape * light_area_data_get(light).size, 0.0f);
      }
      else {
        if (light_local_data_get(light).shadow_radius == 0.0f) {
          /* The shape is a point. There is nothing to jitter.
           * `shape_radius` is clamped to a minimum for precision reasons, so `shadow_radius` is
           * set to 0 only when the light radius is also 0 to detect this case. */
        }
        else {
          position_on_light = sample_ball(rand) * light_local_data_get(light).shape_radius;
        }
      }
    }

    int tilemap_count = light_local_tilemap_count(light);
    for (int i = 0; i < tilemap_count; i++) {
      cubeface_sync(
          light.tilemap_index + i, light.object_to_world, eCubeFace(i), position_on_light);
    }

    LightSpotData local_data = light_local_data_get(light);
    local_data.shadow_position = position_on_light;

    light = light_local_data_set(light, local_data);
  }

  light_buf[l_idx] = light;
}
