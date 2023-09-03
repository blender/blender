/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Two variants, split pass, generates either 2 triangles or 6 triangles depending on input
 * geometry manifold type */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#ifdef DOUBLE_MANIFOLD
#  define vert_len 12 /* Triangle Strip with 6 verts = 4 triangles = 12 verts. */
#  define emit_triangle_count 4
#else
#  define vert_len 6 /* Triangle Strip with 4 verts = 2 triangles = 6 verts. */
#  define emit_triangle_count 2
#endif

#define DEGENERATE_TRIS_WORKAROUND
#define DEGENERATE_TRIS_AREA_THRESHOLD 4e-15

#define len_sqr(a) dot(a, a)

struct VertexData {
  vec3 pos;           /* local position */
  vec4 frontPosition; /* final ndc position */
  vec4 backPosition;
};

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  return;

/* Extra positions for primary triangle */
VertexData vData[4];

void extrude_edge(bool invert, int output_vertex_id)
{
  /* Reverse order if back-facing the light. */
  ivec2 idx = (invert) ? ivec2(1, 2) : ivec2(2, 1);

  /* Either outputs first or second quad, depending on double manifold status. */
  int triangle_vertex_id = output_vertex_id % 6;

  switch (triangle_vertex_id) {
    case 0:
      gl_Position = vData[idx.x].frontPosition;
      break;
    case 1:
    case 4:
      gl_Position = vData[idx.y].frontPosition;
      break;
    case 2:
    case 3:
      gl_Position = vData[idx.x].backPosition;
      break;
    case 5:
      gl_Position = vData[idx.y].backPosition;
      break;
  }

  /* Apply depth bias. Prevents Z-fighting artifacts when fast-math is enabled. */
  gl_Position.z += 0.00005;
}

vec3 extrude_offset(vec3 ls_P)
{
#ifdef WORKBENCH_NEXT
  vec3 ws_P = point_object_to_world(ls_P);
  float extrude_distance = 1e5f;
  float L_dot_FP = dot(pass_data.light_direction_ws, pass_data.far_plane.xyz);
  if (L_dot_FP > 0.0) {
    float signed_distance = dot(pass_data.far_plane.xyz, ws_P) - pass_data.far_plane.w;
    extrude_distance = -signed_distance / L_dot_FP;
  }
  vec3 ls_light_direction = normal_world_to_object(vec3(pass_data.light_direction_ws));
  return ls_light_direction * extrude_distance;
#else
  return lightDirection * lightDistance;
#endif
}

void main()
{
  /* Output Data indexing. */
#ifdef DOUBLE_MANIFOLD
  int input_prim_index = int(gl_VertexID / 12);
  int output_vertex_id = gl_VertexID % 12;
  int output_quad_id = (output_vertex_id >= 6) ? 1 : 0;
#else
  int input_prim_index = int(gl_VertexID / 6);
  int output_vertex_id = gl_VertexID % 6;
  int output_quad_id = 0;
#endif

  /* Source primitive data location derived from output primitive. */
  int input_base_vertex_id = (input_prim_index * 4);

  /* IN DATA is lines_adjacency - Should be guaranteed. */
  /* Read input position data. */
  vData[0].pos = vertex_fetch_attribute(input_base_vertex_id + 0, pos, vec3);
  vData[1].pos = vertex_fetch_attribute(input_base_vertex_id + 1, pos, vec3);
  vData[2].pos = vertex_fetch_attribute(input_base_vertex_id + 2, pos, vec3);
  vData[3].pos = vertex_fetch_attribute(input_base_vertex_id + 3, pos, vec3);

  /* Calculate front/back Positions. */
  vData[0].frontPosition = point_object_to_ndc(vData[0].pos);
  vData[0].backPosition = point_object_to_ndc(vData[0].pos + extrude_offset(vData[0].pos));

  vData[1].frontPosition = point_object_to_ndc(vData[1].pos);
  vData[1].backPosition = point_object_to_ndc(vData[1].pos + extrude_offset(vData[1].pos));

  vData[2].frontPosition = point_object_to_ndc(vData[2].pos);
  vData[2].backPosition = point_object_to_ndc(vData[2].pos + extrude_offset(vData[2].pos));

  vData[3].frontPosition = point_object_to_ndc(vData[3].pos);
  vData[3].backPosition = point_object_to_ndc(vData[3].pos + extrude_offset(vData[3].pos));

  /* Geometry shader equivalent path. */
  vec3 v10 = vData[0].pos - vData[1].pos;
  vec3 v12 = vData[2].pos - vData[1].pos;
  vec3 v13 = vData[3].pos - vData[1].pos;
  vec3 n1 = cross(v12, v10);
  vec3 n2 = cross(v13, v12);

#ifdef DEGENERATE_TRIS_WORKAROUND
  /* Check if area is null. */
  vec2 faces_area = vec2(len_sqr(n1), len_sqr(n2));
  bvec2 degen_faces = lessThan(abs(faces_area), vec2(DEGENERATE_TRIS_AREA_THRESHOLD));

  /* Both triangles are degenerate, abort. */
  if (all(degen_faces)) {
    DISCARD_VERTEX
  }
#endif

#ifdef WORKBENCH_NEXT
  vec3 lightDirection = normal_world_to_object(vec3(pass_data.light_direction_ws));
#endif
  vec2 facing = vec2(dot(n1, lightDirection), dot(n2, lightDirection));

  /* WATCH: maybe unpredictable in some cases. */
  bool is_manifold = any(notEqual(vData[0].pos, vData[3].pos));

  bvec2 backface = greaterThan(facing, vec2(0.0));

#ifdef DEGENERATE_TRIS_WORKAROUND
#  ifndef DOUBLE_MANIFOLD
  /* If the mesh is known to be manifold and we don't use double count,
   * only create an quad if the we encounter a facing geom. */
  if ((degen_faces.x && backface.y) || (degen_faces.y && backface.x)) {
    DISCARD_VERTEX
  }
#  endif

  /* If one of the 2 triangles is degenerate, replace edge by a non-manifold one. */
  backface.x = (degen_faces.x) ? !backface.y : backface.x;
  backface.y = (degen_faces.y) ? !backface.x : backface.y;
  is_manifold = (any(degen_faces)) ? false : is_manifold;
#endif

  /* If both faces face the same direction it's not an outline edge. */
  if (backface.x == backface.y) {
    DISCARD_VERTEX
  }

  /* Output correct output vertex depending on */
  if (output_vertex_id < 6) {
    /* Always output if first 6 vertices. */
    extrude_edge(backface.x, output_vertex_id);
  }
  else {
#ifdef DOUBLE_MANIFOLD
    /* If part of double manifold, only output this vertex if correctly contributing */
    if (is_manifold) {
      extrude_edge(backface.x, output_vertex_id);
    }
    else {
      DISCARD_VERTEX
    }
#else
    DISCARD_VERTEX
#endif
  }
}
