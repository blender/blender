/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Setup tilemap positionning for each shadow casting light.
 * Dispatched one thread per light.
 */

#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)

int shadow_directional_coverage_get(int level)
{
  return 1 << level;
}

void orthographic_sync(int tilemap_id,
                       Transform light_tx,
                       int2 origin_offset,
                       int clipmap_level,
                       eShadowProjectionType projection_type)
{
  if (all(equal(tilemaps_buf[tilemap_id].grid_shift, int2(0)))) {
    /* Only replace shift if it is not already dirty. */
    tilemaps_buf[tilemap_id].grid_shift = tilemaps_buf[tilemap_id].grid_offset - origin_offset;
  }
  tilemaps_buf[tilemap_id].grid_offset = origin_offset;

  mat3x3 object_to_world_transposed = mat3x3(tilemaps_buf[tilemap_id].viewmat);

  if (!all(equal(object_to_world_transposed[0], light_tx.x.xyz)) ||
      !all(equal(object_to_world_transposed[1], light_tx.y.xyz)) ||
      !all(equal(object_to_world_transposed[2], light_tx.z.xyz)))
  {
    tilemaps_buf[tilemap_id].grid_shift = int2(SHADOW_TILEMAP_RES);
  }

  float level_size = shadow_directional_coverage_get(clipmap_level);
  float half_size = level_size / 2.0;
  float tile_size = level_size / float(SHADOW_TILEMAP_RES);
  vec2 center_offset = vec2(origin_offset) * tile_size;

  /* object_mat is a rotation matrix. Reduce imprecision by taking the transpose which is also the
   * inverse in this particular case. */
  tilemaps_buf[tilemap_id].viewmat[0] = vec4(light_tx.x.xyz, 0.0);
  tilemaps_buf[tilemap_id].viewmat[1] = vec4(light_tx.y.xyz, 0.0);
  tilemaps_buf[tilemap_id].viewmat[2] = vec4(light_tx.z.xyz, 0.0);
  tilemaps_buf[tilemap_id].viewmat[3] = vec4(0.0, 0.0, 0.0, 1.0);

  tilemaps_buf[tilemap_id].projection_type = projection_type;
  tilemaps_buf[tilemap_id].half_size = half_size;
  tilemaps_buf[tilemap_id].center_offset = center_offset;
  tilemaps_buf[tilemap_id].winmat = projection_orthographic(
      -half_size + center_offset.x,
      half_size + center_offset.x,
      -half_size + center_offset.y,
      half_size + center_offset.y,
      /* Near/far is computed on GPU using casters bounds. */
      -1.0,
      1.0);
}

void cascade_sync(inout LightData light)
{
  int level_min = light_sun_data_get(light).clipmap_lod_min;
  int level_max = light_sun_data_get(light).clipmap_lod_max;
  int level_range = level_max - level_min;
  int level_len = level_range + 1;

  vec3 ws_camera_position = uniform_buf.camera.viewinv[3].xyz;
  vec3 ws_camera_forward = uniform_buf.camera.viewinv[2].xyz;
  float camera_clip_near = uniform_buf.camera.clip_near;
  float camera_clip_far = uniform_buf.camera.clip_far;

  /* All tile-maps use the first level size. */
  float level_size = shadow_directional_coverage_get(level_min);
  float half_size = level_size / 2.0;
  float tile_size = level_size / float(SHADOW_TILEMAP_RES);

  /* Ideally we should only take the intersection with the scene bounds. */
  vec3 ws_far_point = ws_camera_position - ws_camera_forward * camera_clip_far;
  vec3 ws_near_point = ws_camera_position - ws_camera_forward * camera_clip_near;

  vec3 ls_far_point = transform_direction_transposed(light.object_to_world, ws_far_point);
  vec3 ls_near_point = transform_direction_transposed(light.object_to_world, ws_near_point);

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

  vec2 clipmap_origin = vec2(origin_offset) * tile_size;

#if USE_LIGHT_UNION
  /* Used as origin for the clipmap_base_offset trick. */
  light.sun.clipmap_origin = clipmap_origin;
  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  light.sun.clipmap_base_offset_pos = base_offset_pos;
  light.sun.clipmap_base_offset_neg = ivec2(0);
#else
  /* Used as origin for the clipmap_base_offset trick. */
  light.do_not_access_directly._pad3 = clipmap_origin;
  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  light.do_not_access_directly._pad0_reserved = intBitsToFloat(base_offset_pos.x);
  light.do_not_access_directly._pad1_reserved = intBitsToFloat(base_offset_pos.y);
  light.do_not_access_directly._pad7 = intBitsToFloat(0);
  light.do_not_access_directly.shadow_projection_shift = intBitsToFloat(0);
#endif
}

void clipmap_sync(inout LightData light)
{
  vec3 ws_camera_position = uniform_buf.camera.viewinv[3].xyz;
  vec3 ls_camera_position = transform_direction_transposed(light.object_to_world,
                                                           ws_camera_position);

  int level_min = light_sun_data_get(light).clipmap_lod_min;
  int level_max = light_sun_data_get(light).clipmap_lod_max;
  int level_len = level_max - level_min + 1;

  vec2 clipmap_origin;
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

    clipmap_origin = vec2(level_offset) * tile_size;
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
    int2 lvl_delta = lvl_offset - (lvl_offset_next << 1);
    pos_offset |= max(lvl_delta, int2(0)) << lod;
    neg_offset |= max(-lvl_delta, int2(0)) << lod;
  }

  /* Used for selecting the clipmap level. */
  light.object_to_world.x.w = ls_camera_position.x;
  light.object_to_world.y.w = ls_camera_position.y;
  light.object_to_world.z.w = ls_camera_position.z;
#if USE_LIGHT_UNION
  /* Used as origin for the clipmap_base_offset trick. */
  light.sun.clipmap_origin = clipmap_origin;
  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  light.sun.clipmap_base_offset_pos = pos_offset;
  light.sun.clipmap_base_offset_neg = neg_offset;
#else
  /* Used as origin for the clipmap_base_offset trick. */
  light.do_not_access_directly._pad3 = clipmap_origin;
  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  light.do_not_access_directly._pad0_reserved = intBitsToFloat(pos_offset.x);
  light.do_not_access_directly._pad1_reserved = intBitsToFloat(pos_offset.y);
  light.do_not_access_directly._pad7 = intBitsToFloat(neg_offset.x);
  light.do_not_access_directly.shadow_projection_shift = intBitsToFloat(neg_offset.y);
#endif
}

#if 0
void cubeface_sync(int tilemap_id, vec3 jitter_offset)
{
  /* Update corners. */
  viewmat = shadow_face_mat[cubeface] * from_location<float4x4>(float3(0.0, 0.0, -shift)) *
            invert(object_mat);

  /* Update corners. */
  corners[0] += jitter_offset;
  corners[1] += jitter_offset;
  corners[2] += jitter_offset;
  corners[3] += jitter_offset;

  /* Set dirty. */
  grid_shift = int2(SHADOW_TILEMAP_RES);
}
#endif

void main()
{
  uint l_idx = gl_GlobalInvocationID.x;
  if (l_idx >= light_cull_buf.items_count) {
    return;
  }

  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  if (is_sun_light(light.type)) {
    /* Distant lights. */

#if 0 /* Jittered shadows. */
    vec3 position_on_light = random_position_on_light(light);
    vec3 light_direction = normalize(position_on_light);
    float3x3 object_to_world_transposed = transpose(from_up_axis(light_direction));

    light.object_to_world.x.xyz = object_to_world_transposed[0];
    light.object_to_world.y.xyz = object_to_world_transposed[1];
    light.object_to_world.z.xyz = object_to_world_transposed[2];
#endif

    if (light.type == LIGHT_SUN_ORTHO) {
      cascade_sync(light);
    }
    else {
      clipmap_sync(light);
    }
  }
#if 0   /* Jittered shadows. */
  else {
    /* Local lights. */
#  if 0 /* Jittered shadows. */
    vec3 position_on_light = random_position_on_light(light);
    light_buf[l_idx].shadow_position = position_on_light;

    int tilemap_count = 0;
    if (is_area_light(light.type)) {
      tilemap_count = 5;
    }
    else if (is_spot_light(light.type)) {
      tilemap_count = (spot_angle > M_PI * 0.25) ? 5 : 1;
    }
    else {
      tilemap_count = 6;
    }

    for (int i = 0; i < tilemap_count; i++) {
      cubeface_sync(light.tilemap_id + i, position_on_light);
    }
#  endif
  }
#endif

  light_buf[l_idx] = light;
}
