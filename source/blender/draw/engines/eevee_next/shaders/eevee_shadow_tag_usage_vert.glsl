/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This renders the bounding boxes for transparent objects in order to tag the correct shadows.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_shape_lib.glsl)

/* Inflate bounds by half a pixel as a conservative rasterization alternative,
 * to ensure the tiles needed by all LOD0 pixels get tagged */
void inflate_bounds(vec3 ls_center, inout vec3 P, inout vec3 lP)
{
  vec3 vP = point_world_to_view(P);

  float inflate_scale = pixel_world_radius * exp2(float(fb_lod));
  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  if (is_persp) {
    inflate_scale *= -vP.z;
  }
  /* Half-pixel. */
  inflate_scale *= 0.5;

  vec3 vs_inflate_vector = normal_object_to_view(sign(lP - ls_center));
  vs_inflate_vector.z = 0;
  /* Scale the vector so the largest axis length is 1 */
  vs_inflate_vector /= max_v2(abs(vs_inflate_vector.xy));
  vs_inflate_vector *= inflate_scale;

  vP += vs_inflate_vector;
  P = point_view_to_world(vP);
  lP = point_world_to_object(P);
}

void main()
{
  PASS_RESOURCE_ID

  const ObjectBounds bounds = bounds_buf[resource_id];

  Box box = shape_box(bounds.bounding_corners[0].xyz,
                      bounds.bounding_corners[0].xyz + bounds.bounding_corners[1].xyz,
                      bounds.bounding_corners[0].xyz + bounds.bounding_corners[2].xyz,
                      bounds.bounding_corners[0].xyz + bounds.bounding_corners[3].xyz);

  vec3 ws_aabb_min = bounds.bounding_corners[0].xyz;
  vec3 ws_aabb_max = bounds.bounding_corners[0].xyz + bounds.bounding_corners[1].xyz +
                     bounds.bounding_corners[2].xyz + bounds.bounding_corners[3].xyz;

  vec3 ls_center = point_world_to_object((ws_aabb_min + ws_aabb_max) / 2.0);

  vec3 ls_conservative_min = vec3(FLT_MAX);
  vec3 ls_conservative_max = vec3(-FLT_MAX);

  for (int i = 0; i < 8; i++) {
    vec3 P = box.corners[i];
    vec3 lP = point_world_to_object(P);
    inflate_bounds(ls_center, P, lP);

    ls_conservative_min = min(ls_conservative_min, lP);
    ls_conservative_max = max(ls_conservative_max, lP);
  }

  interp_flat.ls_aabb_min = ls_conservative_min;
  interp_flat.ls_aabb_max = ls_conservative_max;

  vec3 lP = mix(ls_conservative_min, ls_conservative_max, max(vec3(0), pos));

  interp.P = point_object_to_world(lP);
  interp.vP = point_world_to_view(interp.P);

  gl_Position = point_world_to_ndc(interp.P);

#if 0
  if (gl_VertexID == 0) {
    Box debug_box = shape_box(
        ls_conservative_min,
        ls_conservative_min + (ls_conservative_max - ls_conservative_min) * vec3(1, 0, 0),
        ls_conservative_min + (ls_conservative_max - ls_conservative_min) * vec3(0, 1, 0),
        ls_conservative_min + (ls_conservative_max - ls_conservative_min) * vec3(0, 0, 1));
    for (int i = 0; i < 8; i++) {
      debug_box.corners[i] = point_object_to_world(debug_box.corners[i]);
    }
    drw_debug(debug_box);
  }
#endif
}
