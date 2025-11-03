/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This ray-marches the current fragment along the bounds depth and tags all the intersected shadow
 * tiles.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_shadow_tag_usage_transparent)

#include "draw_model_lib.glsl"
#include "eevee_shadow_tag_usage_lib.glsl"

float ray_aabb(float3 ray_origin, float3 ray_direction, float3 aabb_min, float3 aabb_max)
{
  /* https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html */
  float3 t_mins = (aabb_min - ray_origin) / ray_direction;
  float3 t_maxs = (aabb_max - ray_origin) / ray_direction;

  float t_min = reduce_max(min(t_mins, t_maxs));
  float t_max = reduce_min(max(t_mins, t_maxs));

  /* AABB is in the opposite direction. */
  if (t_max < 0.0f) {
    return -1.0f;
  }
  /* No intersection. */
  if (t_min > t_max) {
    return -1.0f;
  }
  /* The ray origin is inside the aabb. */
  if (t_min < 0.0f) {
    /* For regular ray casting we would return t_max here,
     * but we want to ray cast against the box volume, not just the surface. */
    return 0.0f;
  }
  return t_min;
}

float pixel_size_at(float linear_depth)
{
  float pixel_size = uniform_buf.shadow.film_pixel_radius;
  bool is_persp = (drw_view().winmat[3][3] == 0.0f);
  if (is_persp) {
    pixel_size *= max(0.01f, linear_depth);
  }
  return pixel_size * exp2(float(fb_lod));
}

void step_bounding_sphere(float3 vs_near_plane,
                          float3 vs_view_direction,
                          float near_t,
                          float far_t,
                          out float3 sphere_center,
                          out float sphere_radius)
{
  float near_pixel_size = pixel_size_at(near_t);
  float3 near_center = vs_near_plane + vs_view_direction * near_t;

  float far_pixel_size = pixel_size_at(far_t);
  float3 far_center = vs_near_plane + vs_view_direction * far_t;

  sphere_center = mix(near_center, far_center, 0.5f);
  sphere_radius = 0;

  for (int x = -1; x <= 1; x += 2) {
    for (int y = -1; y <= 1; y += 2) {
      float3 near_corner = near_center + (near_pixel_size * 0.5f * float3(x, y, 0));
      sphere_radius = max(sphere_radius, length_squared(near_corner - sphere_center));

      float3 far_corner = far_center + (far_pixel_size * 0.5f * float3(x, y, 0));
      sphere_radius = max(sphere_radius, length_squared(far_corner - sphere_center));
    }
  }

  sphere_center = drw_point_view_to_world(sphere_center);
  sphere_radius = sqrt(sphere_radius);
}

/* Warning: Only works for valid, finite, positive floats. */
float nextafter(float value)
{
  return uintBitsToFloat(floatBitsToUint(value) + 1);
}

void main()
{
  float2 screen_uv = gl_FragCoord.xy / float2(fb_resolution);

  float opaque_depth = texelFetch(hiz_tx, int2(gl_FragCoord.xy), fb_lod).r;
  float3 ws_opaque = drw_point_screen_to_world(float3(screen_uv, opaque_depth));

  float3 ws_near_plane = drw_point_screen_to_world(float3(screen_uv, 0.0f));
  float3 ws_view_direction = normalize(interp.P - ws_near_plane);
  float3 vs_near_plane = drw_point_screen_to_view(float3(screen_uv, 0.0f));
  float3 vs_view_direction = normalize(interp.vP - vs_near_plane);
  float3 ls_near_plane = drw_point_world_to_object(ws_near_plane);
  float3 ls_view_direction = normalize(drw_point_world_to_object(interp.P) - ls_near_plane);

  /* TODO (Miguel Pozo): We could try to ray-cast against the non-inflated bounds first,
   * and fall back to the inflated ones if there is no hit.
   * The inflated bounds can cause unnecessary extra steps. */
  float ls_near_box_t = ray_aabb(
      ls_near_plane, ls_view_direction, interp_flat.ls_aabb_min, interp_flat.ls_aabb_max);

  if (ls_near_box_t < 0.0f) {
    /* The ray cast can fail in ortho mode due to numerical precision. (See #121629) */
    return;
  }

  float3 ls_near_box = ls_near_plane + ls_view_direction * ls_near_box_t;
  float3 ws_near_box = drw_point_object_to_world(ls_near_box);

  float near_box_t = distance(ws_near_plane, ws_near_box);
  float far_box_t = distance(ws_near_plane, interp.P);
  /* Depth test. */
  far_box_t = min(far_box_t, distance(ws_near_plane, ws_opaque));

  /* Ray march from the front to the back of the bbox, and tag shadow usage along the way. */
  float step_size;
  /* In extreme cases, step_size can be smaller than the next representable float delta, so we use
   * nextafter to prevent infinite loops. (See #137566) */
  for (float t = near_box_t; t <= far_box_t; t = max(t + step_size, nextafter(t))) {
    /* Ensure we don't get past far_box_t. */
    t = min(t, far_box_t);
    step_size = pixel_size_at(t);

    float3 P = ws_near_plane + (ws_view_direction * t);
    float step_radius;
    step_bounding_sphere(vs_near_plane, vs_view_direction, t, t + step_size, P, step_radius);
    float3 vP = drw_point_world_to_view(P);

    shadow_tag_usage(
        vP, P, ws_view_direction, step_radius, gl_FragCoord.xy * exp2(float(fb_lod)), 0);
  }
}
