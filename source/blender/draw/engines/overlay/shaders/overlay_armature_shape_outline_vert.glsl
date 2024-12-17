/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

struct VertIn {
  vec3 ls_P;
  mat4 inst_matrix;
};

VertIn input_assembly(uint in_vertex_id, mat4x4 inst_matrix)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  vert_in.inst_matrix = inst_matrix;
  return vert_in;
}

struct VertOut {
  vec3 vs_P;
  vec3 ws_P;
  vec4 hs_P;
  vec2 ss_P;
  vec4 color_size;
  int inverted;
};

VertOut vertex_main(VertIn v_in)
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(v_in.inst_matrix, state_color, bone_color);

  VertOut v_out;
  v_out.ws_P = transform_point(model_mat, v_in.ls_P);
  v_out.vs_P = drw_point_world_to_view(v_out.ws_P);
  v_out.hs_P = drw_point_view_to_homogenous(v_out.vs_P);
  v_out.ss_P = drw_perspective_divide(v_out.hs_P).xy * sizeViewport;
  v_out.inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0);
  v_out.color_size = bone_color;

  return v_out;
}

void emit_vertex(const uint strip_index,
                 uint out_vertex_id,
                 uint out_primitive_id,
                 vec4 color,
                 vec4 hs_P,
                 vec3 ws_P,
                 vec2 offset,
                 bool is_persp)
{
  bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
  /* Maps triangle list primitives to triangle strip indices. */
  uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                         out_primitive_id;

  if (out_strip_index != strip_index) {
    return;
  }

  finalColor = color;

  gl_Position = hs_P;
  /* Offset away from the center to avoid overlap with solid shape. */
  gl_Position.xy += offset * sizeViewportInv * gl_Position.w;
  /* Improve AA bleeding inside bone silhouette. */
  gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;

  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport;

  view_clipping_distances(ws_P);
}

void geometry_main(VertOut geom_in[4],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  bool is_persp = (drw_view.winmat[3][3] == 0.0);

  vec3 view_vec = (is_persp) ? normalize(geom_in[1].vs_P) : vec3(0.0, 0.0, -1.0);
  vec3 v10 = geom_in[0].vs_P - geom_in[1].vs_P;
  vec3 v12 = geom_in[2].vs_P - geom_in[1].vs_P;
  vec3 v13 = geom_in[3].vs_P - geom_in[1].vs_P;

  vec3 n0 = cross(v12, v10);
  vec3 n3 = cross(v13, v12);

  float fac0 = dot(view_vec, n0);
  float fac3 = dot(view_vec, n3);

  /* If one of the face is perpendicular to the view,
   * consider it and outline edge. */
  if (abs(fac0) > 1e-5 && abs(fac3) > 1e-5) {
    /* If both adjacent verts are facing the camera the same way,
     * then it isn't an outline edge. */
    if (sign(fac0) == sign(fac3)) {
      return;
    }
  }

  n0 = (geom_in[0].inverted == 1) ? -n0 : n0;
  /* Don't outline if concave edge. */
  if (dot(n0, v13) > 0.0001) {
    return;
  }

  vec2 perp = normalize(geom_in[2].ss_P - geom_in[1].ss_P);
  vec2 edge_dir = vec2(-perp.y, perp.x);

  vec2 hidden_point;
  /* Take the farthest point to compute edge direction
   * (avoid problems with point behind near plane).
   * If the chosen point is parallel to the edge in screen space,
   * choose the other point anyway.
   * This fixes some issue with cubes in orthographic views. */
  if (geom_in[0].vs_P.z < geom_in[3].vs_P.z) {
    hidden_point = (abs(fac0) > 1e-5) ? geom_in[0].ss_P : geom_in[3].ss_P;
  }
  else {
    hidden_point = (abs(fac3) > 1e-5) ? geom_in[3].ss_P : geom_in[0].ss_P;
  }
  vec2 hidden_dir = normalize(hidden_point - geom_in[1].ss_P);

  float fac = dot(-hidden_dir, edge_dir);
  edge_dir *= (fac < 0.0) ? -1.0 : 1.0;

  emit_vertex(0,
              out_vertex_id,
              out_primitive_id,
              vec4(geom_in[0].color_size.rgb, 1.0),
              geom_in[1].hs_P,
              geom_in[1].ws_P,
              edge_dir - perp,
              is_persp);

  emit_vertex(1,
              out_vertex_id,
              out_primitive_id,
              vec4(geom_in[0].color_size.rgb, 1.0),
              geom_in[2].hs_P,
              geom_in[2].ws_P,
              edge_dir + perp,
              is_persp);
}

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  /* Line Adjacency primitive. */
  const uint input_primitive_vertex_count = 4u;
  /* Line list primitive. */
  const uint ouput_primitive_vertex_count = 2u;
  const uint ouput_primitive_count = 1u;
  const uint ouput_invocation_count = 1u;
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

  mat4x4 inst_matrix = data_buf[gl_InstanceID];

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u, inst_matrix);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u, inst_matrix);
  vert_in[2] = input_assembly(in_primitive_first_vertex + 2u, inst_matrix);
  vert_in[3] = input_assembly(in_primitive_first_vertex + 3u, inst_matrix);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);
  vert_out[2] = vertex_main(vert_in[2]);
  vert_out[3] = vertex_main(vert_in[3]);

  /* Discard by default. */
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
