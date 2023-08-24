/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadowmapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This ray-marches the current fragment along the bounds depth and tags all the intersected shadow
 * tiles.
 */

#pragma BLENDER_REQUIRE(eevee_shadow_tag_usage_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#pragma BLENDER_REQUIRE(common_debug_shape_lib.glsl)

float ray_aabb(vec3 ray_origin, vec3 ray_direction, vec3 aabb_min, vec3 aabb_max)
{
  /* https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html */
  vec3 t_mins = (aabb_min - ray_origin) / ray_direction;
  vec3 t_maxs = (aabb_max - ray_origin) / ray_direction;

  float t_min = max_v3(min(t_mins, t_maxs));
  float t_max = min_v3(max(t_mins, t_maxs));

  /* AABB is in the opposite direction. */
  if (t_max < 0.0) {
    return -1.0;
  }
  /* No intersection. */
  if (t_min > t_max) {
    return -1.0;
  }
  /* The ray origin is inside the aabb. */
  if (t_min < 0.0) {
    /* For regular ray casting we would return t_max here,
     * but we want to ray cast against the box volume, not just the surface. */
    return 0.0;
  }
  return t_min;
}

float pixel_size_at(float linear_depth)
{
  float pixel_size = pixel_world_radius;
  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  if (is_persp) {
    pixel_size *= max(0.01, linear_depth);
  }
  return pixel_size * exp2(float(fb_lod));
}

void step_bounding_sphere(vec3 vs_near_plane,
                          vec3 vs_view_direction,
                          float near_t,
                          float far_t,
                          out vec3 sphere_center,
                          out float sphere_radius)
{
  float near_pixel_size = pixel_size_at(near_t);
  vec3 near_center = vs_near_plane + vs_view_direction * near_t;

  float far_pixel_size = pixel_size_at(far_t);
  vec3 far_center = vs_near_plane + vs_view_direction * far_t;

  sphere_center = mix(near_center, far_center, 0.5);
  sphere_radius = 0;

  for (int x = -1; x <= 1; x += 2) {
    for (int y = -1; y <= 1; y += 2) {
      vec3 near_corner = near_center + (near_pixel_size * 0.5 * vec3(x, y, 0));
      sphere_radius = max(sphere_radius, len_squared(near_corner - sphere_center));

      vec3 far_corner = far_center + (far_pixel_size * 0.5 * vec3(x, y, 0));
      sphere_radius = max(sphere_radius, len_squared(far_corner - sphere_center));
    }
  }

  sphere_center = point_view_to_world(sphere_center);
  sphere_radius = sqrt(sphere_radius);
}

void main()
{
  vec2 screen_uv = gl_FragCoord.xy / vec2(fb_resolution);

  float opaque_depth = texelFetch(hiz_tx, ivec2(gl_FragCoord.xy), fb_lod).r;
  vec3 ws_opaque = get_world_space_from_depth(screen_uv, opaque_depth);

  vec3 ws_near_plane = get_world_space_from_depth(screen_uv, 0);
  vec3 ws_view_direction = normalize(interp.P - ws_near_plane);
  vec3 vs_near_plane = get_view_space_from_depth(screen_uv, 0);
  vec3 vs_view_direction = normalize(interp.vP - vs_near_plane);
  vec3 ls_near_plane = point_world_to_object(ws_near_plane);
  vec3 ls_view_direction = normalize(point_world_to_object(interp.P) - ls_near_plane);

  /* TODO (Miguel Pozo): We could try to ray-cast against the non-inflated bounds first,
   * and fallback to the inflated ones if theres no hit.
   * The inflated bounds can cause unnecesary extra steps. */
  float ls_near_box_t = ray_aabb(
      ls_near_plane, ls_view_direction, interp_flat.ls_aabb_min, interp_flat.ls_aabb_max);
  vec3 ls_near_box = ls_near_plane + ls_view_direction * ls_near_box_t;
  vec3 ws_near_box = point_object_to_world(ls_near_box);

  float near_box_t = distance(ws_near_plane, ws_near_box);
  float far_box_t = distance(ws_near_plane, interp.P);
  /* Depth test. */
  far_box_t = min(far_box_t, distance(ws_near_plane, ws_opaque));

  /* Ray march from the front to the back of the bbox, and tag shadow usage along the way. */
  float step_size;
  for (float t = near_box_t; t <= far_box_t; t += step_size) {
    /* Ensure we don't get past far_box_t. */
    t = min(t, far_box_t);
    step_size = pixel_size_at(t);

    vec3 P = ws_near_plane + (ws_view_direction * t);
    float step_radius;
    step_bounding_sphere(vs_near_plane, vs_view_direction, t, t + step_size, P, step_radius);
    vec3 vP = point_world_to_view(P);

    shadow_tag_usage(
        vP, P, ws_view_direction, step_radius, t, gl_FragCoord.xy * exp2(float(fb_lod)));
  }
}
