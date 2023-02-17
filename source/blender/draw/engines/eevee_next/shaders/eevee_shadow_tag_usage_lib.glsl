
/**
 * Virtual shadowmapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This pass scan the depth buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */

#pragma BLENDER_REQUIRE(common_intersect_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)

void shadow_tag_usage_tilemap(uint l_idx, vec3 P, float dist_to_cam, const bool is_directional)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  int lod = 0;
  ivec2 tile_co;
  int tilemap_index = light.tilemap_index;
  if (is_directional) {
    vec3 lP = shadow_world_to_local(light, P);

    ShadowCoordinates coord = shadow_directional_coordinates(light, lP);

    tile_co = coord.tile_coord;
    tilemap_index = coord.tilemap_index;
  }
  else {
    vec3 lP = light_world_to_local(light, P - light._position);
    float dist_to_light = length(lP);
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
      if (lP.z > 0.0) {
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

    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);

    ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);
    tile_co = coord.tile_coord;
    tilemap_index = coord.tilemap_index;

    lod = int(ceil(-log2(footprint_ratio) + tilemaps_buf[tilemap_index].lod_bias));
    lod = clamp(lod, 0, SHADOW_TILEMAP_LOD);
  }
  tile_co >>= lod;

  if (tilemap_index > light_tilemap_max_get(light)) {
    return;
  }

  int tile_index = shadow_tile_offset(tile_co, tilemaps_buf[tilemap_index].tiles_index, lod);
  atomicOr(tiles_buf[tile_index], SHADOW_IS_USED);
}

void shadow_tag_usage(vec3 vP, vec3 P, vec2 pixel)
{
  float dist_to_cam = length(vP);

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    shadow_tag_usage_tilemap(l_idx, P, dist_to_cam, true);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx) {
    shadow_tag_usage_tilemap(l_idx, P, dist_to_cam, false);
  }
  LIGHT_FOREACH_END
}
