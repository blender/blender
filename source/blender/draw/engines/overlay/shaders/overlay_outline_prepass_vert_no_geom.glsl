/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma USE_SSBO_VERTEX_FETCH(LineList, 2)
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  return;

uint outline_colorid_get(void)
{
  int flag = int(abs(ObjectInfo.w));
  bool is_active = (flag & DRW_BASE_ACTIVE) != 0;

  if (isTransform) {
    return 0u; /* colorTransform */
  }
  else if (is_active) {
    return 3u; /* colorActive */
  }
  else {
    return 1u; /* colorSelect */
  }

  return 0u;
}

/* Replace top 2 bits (of the 16bit output) by outlineId.
 * This leaves 16K different IDs to create outlines between objects.
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  /* Outputs a singular vertex as part of a LineList primitive, however, requires access to
   * neighboring 4 vertices. */
  /* Fetch verts from input type lines adjacency. */
  int line_prim_id = (gl_VertexID / 2);
  int line_vertex_id = gl_VertexID % 2;
  int base_vertex_id = line_prim_id * 2;

  vec4 gl_pos[4];
  vec3 world_pos[4];
  vec3 view_pos[4];
  for (int i = 0; i < 4; i++) {
    vec3 in_pos = vertex_fetch_attribute_raw(
        vertex_id_from_index_id(4 * line_prim_id + i), pos, vec3);
    world_pos[i] = point_object_to_world(in_pos);
    view_pos[i] = point_world_to_view(world_pos[i]);
    gl_pos[i] = point_world_to_ndc(world_pos[i]);
    gl_pos[i].z -= 1e-3;
  }

  /* Perform geometry shader equivalent logic. */
  bool is_persp = (drw_view.winmat[3][3] == 0.0);

  vec3 view_vec = (is_persp) ? normalize(view_pos[1]) : vec3(0.0, 0.0, -1.0);

  vec3 v10 = view_pos[0] - view_pos[1];
  vec3 v12 = view_pos[2] - view_pos[1];
  vec3 v13 = view_pos[3] - view_pos[1];

  vec3 n0 = cross(v12, v10);
  vec3 n3 = cross(v13, v12);

  float fac0 = dot(view_vec, n0);
  float fac3 = dot(view_vec, n3);

  /* If both adjacent verts are facing the camera the same way,
   * then it isn't an outline edge. */
  if (sign(fac0) == sign(fac3)) {
    DISCARD_VERTEX
  }

  /* Output final position. */
  int output_vert_select = (line_vertex_id + 1);
  gl_Position = gl_pos[output_vert_select];

  /* ID 0 is nothing (background). */
  interp.ob_id = uint(resource_handle + 1);

  /* Should be 2 bits only [0..3]. */
  uint outline_id = outline_colorid_get();

  /* Combine for 16bit uint target. */
  interp.ob_id = (outline_id << 14u) | ((interp.ob_id << SHIFT) >> SHIFT);

  /* Clip final output position. */
  view_clipping_distances(world_pos[output_vert_select]);
}
