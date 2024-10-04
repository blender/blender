/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Extrude shadow casters along their silhouette edge.
 * Manifold meshes only generate one quad per silhouette edge.
 * Non-Manifold meshes generate one quad on their non manifold edges (border edges) and and two
 * quad on their silhouette edge (non-border edges) which we consider "manifold".
 *
 * This shader uses line adjacency primitive to know the geometric normals of neighbor faces.
 * A quad is generated if the faces on each sides of the edge are not facing the light the same
 * way.
 *
 * This vertex shader emulates a geometry shader. The draw call generate enough triangle for one or
 * two quads per input primitive. Each vertex shader invocation reads the whole input primitive and
 * execute the vertex shader code on each of the input primitive's vertices.
 */

#include "workbench_shadow_lib.glsl"

void extrude_edge(
    bool invert, VertOut geom_in_1, VertOut geom_in_2, uint out_vertex_id, uint out_primitive_id)
{
  /* Reverse order if back-facing the light. */
  if (invert) {
    VertOut geom_in_tmp = geom_in_1;
    geom_in_1 = geom_in_2;
    geom_in_2 = geom_in_tmp;
  }

  GeomOut geom_out;
  geom_out.gpu_position = geom_in_2.frontPosition;
  strip_EmitVertex(0, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in_1.frontPosition;
  strip_EmitVertex(1, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in_2.backPosition;
  strip_EmitVertex(2, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in_1.backPosition;
  strip_EmitVertex(3, out_vertex_id, out_primitive_id, geom_out);
}

void geometry_main(VertOut geom_in[4],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  vec3 v10 = geom_in[0].lP - geom_in[1].lP;
  vec3 v12 = geom_in[2].lP - geom_in[1].lP;
  vec3 v13 = geom_in[3].lP - geom_in[1].lP;

  vec3 n1 = cross(v12, v10);
  vec3 n2 = cross(v13, v12);

#ifdef DEGENERATE_TRIS_WORKAROUND
  /* Check if area is null */
  vec2 faces_area = vec2(length_squared(n1), length_squared(n2));
  bvec2 degen_faces = lessThan(abs(faces_area), vec2(DEGENERATE_TRIS_AREA_THRESHOLD));

  /* Both triangles are degenerate, abort. */
  if (all(degen_faces)) {
    return;
  }
#endif

  vec3 ls_light_direction = normal_world_to_object(vec3(pass_data.light_direction_ws));

  vec2 facing = vec2(dot(n1, ls_light_direction), dot(n2, ls_light_direction));

  /* WATCH: maybe unpredictable in some cases. */
  bool is_manifold = any(notEqual(geom_in[0].lP, geom_in[3].lP));

  bvec2 backface = greaterThan(facing, vec2(0.0));

#ifdef DEGENERATE_TRIS_WORKAROUND
#  ifndef DOUBLE_MANIFOLD
  /* If the mesh is known to be manifold and we don't use double count,
   * only create an quad if the we encounter a facing geom. */
  if ((degen_faces.x && backface.y) || (degen_faces.y && backface.x)) {
    return;
  }
#  endif

  /* If one of the 2 triangles is degenerate, replace edge by a non-manifold one. */
  backface.x = (degen_faces.x) ? !backface.y : backface.x;
  backface.y = (degen_faces.y) ? !backface.x : backface.y;
  is_manifold = (any(degen_faces)) ? false : is_manifold;
#endif

  /* If both faces face the same direction it's not an outline edge. */
  if (backface.x == backface.y) {
    return;
  }

#ifdef DOUBLE_MANIFOLD
  if (out_invocation_id != 0u && !is_manifold) {
    /* Only Increment/Decrement twice for manifold edges. */
    return;
  }
#endif

  extrude_edge(backface.x, geom_in[1], geom_in[2], out_vertex_id, out_primitive_id);
}

void main()
{
  /* Line adjacency primitive. */
  const uint input_primitive_vertex_count = 4u;
  /* Triangle list primitive. */
  const uint ouput_primitive_vertex_count = 3u;
  const uint ouput_primitive_count = 2u;
#ifdef DOUBLE_MANIFOLD
  const uint ouput_invocation_count = 2u;
#else
  const uint ouput_invocation_count = 1u;
#endif
  const uint output_vertex_count_per_invocation = ouput_primitive_count *
                                                  ouput_primitive_vertex_count;
  const uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                       ouput_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(gl_VertexID) % ouput_primitive_vertex_count;
  uint out_primitive_id = (uint(gl_VertexID) / ouput_primitive_vertex_count) %
                          ouput_primitive_count;
  uint out_invocation_id = (uint(gl_VertexID) / output_vertex_count_per_invocation) %
                           ouput_invocation_count;

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u);
  vert_in[2] = input_assembly(in_primitive_first_vertex + 2u);
  vert_in[3] = input_assembly(in_primitive_first_vertex + 3u);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);
  vert_out[2] = vertex_main(vert_in[2]);
  vert_out[3] = vertex_main(vert_in[3]);

  /* Discard by default. */
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
