
/**
 * Virtual shadowmapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This contains the common logic used for tagging shadows for opaque and transparent receivers.
 */

#pragma BLENDER_REQUIRE(common_intersect_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)

void shadow_tag_usage_tile(LightData light, ivec2 tile_co, int lod, int tilemap_index)
{
  if (tilemap_index > light_tilemap_max_get(light)) {
    return;
  }

  tile_co >>= lod;
  int tile_index = shadow_tile_offset(tile_co, tilemaps_buf[tilemap_index].tiles_index, lod);
  atomicOr(tiles_buf[tile_index], uint(SHADOW_IS_USED));
}

void shadow_tag_usage_tilemap_directional_at_level(uint l_idx, vec3 P, int level)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  vec3 lP = shadow_world_to_local(light, P);

  level = clamp(level, light.clipmap_lod_min, light.clipmap_lod_max);

  ShadowCoordinates coord = shadow_directional_coordinates_at_level(light, lP, level);
  shadow_tag_usage_tile(light, coord.tile_coord, 0, coord.tilemap_index);
}

void shadow_tag_usage_tilemap_directional(uint l_idx, vec3 P, vec3 V, float radius)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  vec3 lP = shadow_world_to_local(light, P);

  if (radius == 0.0) {
    ShadowCoordinates coord = shadow_directional_coordinates(light, lP);
    shadow_tag_usage_tile(light, coord.tile_coord, 0, coord.tilemap_index);
  }
  else {
    vec3 start_lP = shadow_world_to_local(light, P - V * radius);
    vec3 end_lP = shadow_world_to_local(light, P + V * radius);
    int min_level = shadow_directional_level(light, start_lP - light._position);
    int max_level = shadow_directional_level(light, end_lP - light._position);

    for (int level = min_level; level <= max_level; level++) {
      ShadowCoordinates coord_min = shadow_directional_coordinates_at_level(
          light, lP - vec3(radius, radius, 0.0), level);
      ShadowCoordinates coord_max = shadow_directional_coordinates_at_level(
          light, lP + vec3(radius, radius, 0.0), level);

      for (int x = coord_min.tile_coord.x; x <= coord_max.tile_coord.x; x++) {
        for (int y = coord_min.tile_coord.y; y <= coord_max.tile_coord.y; y++) {
          shadow_tag_usage_tile(light, ivec2(x, y), 0, coord_min.tilemap_index);
        }
      }
    }
  }
}

void shadow_tag_usage_tilemap_punctual(uint l_idx, vec3 P, float dist_to_cam, float radius)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  vec3 lP = light_world_to_local(light, P - light._position);
  float dist_to_light = length(lP) - radius;
  if (dist_to_light > light.influence_radius_max) {
    return;
  }
  if (light.type == LIGHT_SPOT) {
    /* Early out if out of cone. */
    float angle_tan = length(lP.xy / dist_to_light);
    if (angle_tan > light.spot_tan) {
      return;
    }
  }
  else if (is_area_light(light.type)) {
    /* Early out if on the wrong side. */
    if (lP.z - radius > 0.0) {
      return;
    }
  }

  /* How much a shadow map pixel covers a final image pixel.
   * We project a shadow map pixel (as a sphere for simplicity) to the receiver plane.
   * We then reproject this sphere onto the camera screen and compare it to the film pixel size.
   * This gives a good approximation of what LOD to select to get a somewhat uniform shadow map
   * resolution in screen space. */
  float footprint_ratio = dist_to_light;
  /* Project the radius to the screen. 1 unit away from the camera the same way
   * pixel_world_radius_inv was computed. Not needed in orthographic mode. */
  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  if (is_persp) {
    footprint_ratio /= dist_to_cam;
  }
  /* Apply resolution ratio. */
  footprint_ratio *= tilemap_projection_ratio;

  if (radius == 0) {
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);

    int lod = int(ceil(-log2(footprint_ratio) + tilemaps_buf[coord.tilemap_index].lod_bias));
    lod = clamp(lod, 0, SHADOW_TILEMAP_LOD);

    shadow_tag_usage_tile(light, coord.tile_coord, lod, coord.tilemap_index);
  }
  else {
    uint faces = 0u;
    for (int x = -1; x <= 1; x += 2) {
      for (int y = -1; y <= 1; y += 2) {
        for (int z = -1; z <= 1; z += 2) {
          vec3 _lP = lP + vec3(x, y, z) * radius;
          faces |= 1u << shadow_punctual_face_index_get(_lP);
        }
      }
    }

    for (int face_id = 0; face_id < 6; face_id++) {
      if ((faces & (1u << uint(face_id))) == 0u) {
        continue;
      }

      int tilemap_index = light.tilemap_index + face_id;
      int lod = int(ceil(-log2(footprint_ratio) + tilemaps_buf[tilemap_index].lod_bias));
      lod = clamp(lod, 0, SHADOW_TILEMAP_LOD);

      vec3 _lP = shadow_punctual_local_position_to_face_local(face_id, lP);

      vec3 offset = vec3(radius, radius, 0);
      ShadowCoordinates coord_min = shadow_punctual_coordinates(light, _lP - offset, face_id);
      ShadowCoordinates coord_max = shadow_punctual_coordinates(light, _lP + offset, face_id);

      for (int x = coord_min.tile_coord.x; x <= coord_max.tile_coord.x; x++) {
        for (int y = coord_min.tile_coord.y; y <= coord_max.tile_coord.y; y++) {
          shadow_tag_usage_tile(light, ivec2(x, y), lod, tilemap_index);
        }
      }
    }
  }
}

/**
 * \a radius Radius of the tagging area in world space.
 * Used for downsampled/ray-marched tagging, so all the shadowmap texels covered get correctly
 * tagged.
 */
void shadow_tag_usage(vec3 vP, vec3 P, vec3 V, float radius, float dist_to_cam, vec2 pixel)
{
  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    shadow_tag_usage_tilemap_directional(l_idx, P, V, radius);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx) {
    shadow_tag_usage_tilemap_punctual(l_idx, P, dist_to_cam, radius);
  }
  LIGHT_FOREACH_END
}

void shadow_tag_usage(vec3 vP, vec3 P, vec2 pixel)
{
  float dist_to_cam = length(vP);

  shadow_tag_usage(vP, P, vec3(0), 0, dist_to_cam, pixel);
}

void shadow_tag_usage_surfel(Surfel surfel, int directional_lvl)
{
  vec3 P = surfel.position;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    shadow_tag_usage_tilemap_directional_at_level(l_idx, P, directional_lvl);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    /* Set distance to camera to 1 to avoid changing footprint_ratio. */
    float dist_to_cam = 1.0;
    shadow_tag_usage_tilemap_punctual(l_idx, P, dist_to_cam, 0);
  }
  LIGHT_FOREACH_END
}
