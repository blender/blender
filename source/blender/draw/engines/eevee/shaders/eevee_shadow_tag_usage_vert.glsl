/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This renders the bounding boxes for transparent objects in order to tag the correct shadows.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_shadow_tag_usage_transparent)

#include "draw_model_lib.glsl"
#include "draw_shape_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* Inflate bounds by half a pixel as a conservative rasterization alternative,
 * to ensure the tiles needed by all LOD0 pixels get tagged */
void inflate_bounds(float3 ls_center, inout float3 P, inout float3 lP)
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

void main()
{
  drw_ResourceID_iface.resource_index = drw_resource_id_raw();

  ObjectBounds bounds = bounds_buf[drw_resource_id()];
  if (!drw_bounds_are_valid(bounds)) {
    /* Discard. */
    gl_Position = float4(NAN_FLT);
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
    inflate_bounds(ls_center, P, lP);

    ls_conservative_min = min(ls_conservative_min, lP);
    ls_conservative_max = max(ls_conservative_max, lP);
  }

  interp_flat.ls_aabb_min = ls_conservative_min;
  interp_flat.ls_aabb_max = ls_conservative_max;

  float3 lP = mix(ls_conservative_min, ls_conservative_max, max(float3(0), pos));

  interp.P = drw_point_object_to_world(lP);
  interp.vP = drw_point_world_to_view(interp.P);

  gl_Position = drw_point_world_to_homogenous(interp.P);

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
