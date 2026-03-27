/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 */

#pragma once
#pragma create_info

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "eevee_shadow_tag_usage.bsl.hh"

namespace eevee::shadow::usage {

struct VertIn {
  [[attribute(0)]] float3 pos;
};

struct VertOut {
  [[smooth]] float3 P;
  [[smooth]] float3 vP;
  [[flat]] float3 ls_aabb_min;
  [[flat]] float3 ls_aabb_max;
};

struct TagUsageTransparent {
  [[legacy_info]] ShaderCreateInfo draw_resource_id_varying;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo draw_modelmat;

  [[storage(4, read)]] const ObjectBounds (&bounds_buf)[];

  [[push_constant]] const int2 fb_resolution;
  [[push_constant]] const int fb_lod;

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
                            float3 &sphere_center,
                            float &sphere_radius)
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

  /* Inflate bounds by half a pixel as a conservative rasterization alternative,
   * to ensure the tiles needed by all LOD0 pixels get tagged */
  void inflate_bounds(float3 ls_center, float3 &P, float3 &lP)
  {
    float3 vP = drw_point_world_to_view(P);

    float inflate_scale = uniform_buf.shadow.film_pixel_radius * exp2(float(fb_lod));
    if (drw_view_is_perspective()) {
      inflate_scale *= -vP.z;
    }
    /* Half-pixel. */
    inflate_scale *= 0.5f;

    float3 vs_inflate_vector = drw_normal_object_to_view(sign(lP - ls_center));
    vs_inflate_vector.z = 0;
    /* Scale the vector so the largest axis length is 1 */
    vs_inflate_vector /= reduce_max(abs(vs_inflate_vector.xy));
    vs_inflate_vector *= inflate_scale;

    vP += vs_inflate_vector;
    P = drw_point_view_to_world(vP);
    lP = drw_point_world_to_object(P);
  }
};

/* Warning: Only works for valid, finite, positive floats. */
float nextafter(float value)
{
  return uintBitsToFloat(floatBitsToUint(value) + 1);
}

[[vertex]]
void tag_usage_vert([[resource_table]] TagUsageTransparent &srt,
                    [[resource_table]] TagUsage &tag,
                    [[in]] const VertIn &v_in,
                    [[out]] VertOut &v_out,
                    [[position]] float4 &out_position)
{
  drw_ResourceID_iface.resource_id = drw_resource_id_raw();

  ObjectBounds bounds = srt.bounds_buf[drw_resource_id()];
  if (!drw_bounds_are_valid(bounds)) {
    /* Discard. */
    out_position = float4(NAN_FLT);
    return;
  }

  Box box = shape_box(bounds.bounding_corners[0].xyz,
                      bounds.bounding_corners[0].xyz + bounds.bounding_corners[1].xyz,
                      bounds.bounding_corners[0].xyz + bounds.bounding_corners[2].xyz,
                      bounds.bounding_corners[0].xyz + bounds.bounding_corners[3].xyz);

  float3 ws_aabb_min = bounds.bounding_corners[0].xyz;
  float3 ws_aabb_max = bounds.bounding_corners[0].xyz + bounds.bounding_corners[1].xyz +
                       bounds.bounding_corners[2].xyz + bounds.bounding_corners[3].xyz;

  float3 ls_center = drw_point_world_to_object(midpoint(ws_aabb_min, ws_aabb_max));

  float3 ls_conservative_min = float3(FLT_MAX);
  float3 ls_conservative_max = float3(-FLT_MAX);

  for (int i = 0; i < 8; i++) {
    float3 P = box.corners[i];
    float3 lP = drw_point_world_to_object(P);
    srt.inflate_bounds(ls_center, P, lP);

    ls_conservative_min = min(ls_conservative_min, lP);
    ls_conservative_max = max(ls_conservative_max, lP);
  }

  v_out.ls_aabb_min = ls_conservative_min;
  v_out.ls_aabb_max = ls_conservative_max;

  float3 lP = mix(ls_conservative_min, ls_conservative_max, max(float3(0), v_in.pos));

  v_out.P = drw_point_object_to_world(lP);
  v_out.vP = drw_point_world_to_view(v_out.P);

  out_position = drw_point_world_to_homogenous(v_out.P);

#if 0
  if (gl_VertexID == 0) {
    Box debug_box = shape_box(
        ls_conservative_min,
        ls_conservative_min + (ls_conservative_max - ls_conservative_min) * float3(1, 0, 0),
        ls_conservative_min + (ls_conservative_max - ls_conservative_min) * float3(0, 1, 0),
        ls_conservative_min + (ls_conservative_max - ls_conservative_min) * float3(0, 0, 1));
    for (int i = 0; i < 8; i++) {
      debug_box.corners[i] = drw_point_object_to_world(debug_box.corners[i]);
    }
    drw_debug(debug_box);
  }
#endif
}

[[fragment]]
void tag_usage_frag([[resource_table]] TagUsageTransparent &srt,
                    [[resource_table]] TagUsage &tag,
                    [[in]] const VertOut interp,
                    [[frag_coord]] const float4 frag_co)
{
  float2 screen_uv = frag_co.xy / float2(srt.fb_resolution);

  float opaque_depth = texelFetch(hiz_tx, int2(frag_co.xy), srt.fb_lod).r;
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
  float ls_near_box_t = srt.ray_aabb(
      ls_near_plane, ls_view_direction, interp.ls_aabb_min, interp.ls_aabb_max);

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
    step_size = srt.pixel_size_at(t);

    float3 P = ws_near_plane + (ws_view_direction * t);
    float step_radius;
    srt.step_bounding_sphere(vs_near_plane, vs_view_direction, t, t + step_size, P, step_radius);
    float3 vP = drw_point_world_to_view(P);

    tag.tag_pixel(vP, P, frag_co.xy * exp2(float(srt.fb_lod)), ws_view_direction, step_radius, 0);
  }
}

}  // namespace eevee::shadow::usage

namespace eevee::shadow {

PipelineGraphic tag_usage_transparent(usage::tag_usage_vert, usage::tag_usage_frag);

}  // namespace eevee::shadow
