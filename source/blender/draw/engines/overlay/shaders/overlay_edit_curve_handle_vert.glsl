/* SPDX-FileCopyrightText: 2018-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_curve_handle)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

struct VertIn {
  /* Local Position. */
  float3 ls_P;
  /* Edit Flags and Data. */
  uint e_data;
};

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  vert_in.e_data = data[gpu_attr_load_index(v_i, gpu_attr_1)];
  return vert_in;
}

struct VertOut {
  float3 ws_P;
  float4 gpu_position;
  uint flag;
};

VertOut vertex_main(VertIn vert_in)
{
  VertOut vert;
  vert.flag = vert_in.e_data;
  vert.ws_P = drw_point_object_to_world(vert_in.ls_P);
  vert.gpu_position = drw_point_world_to_homogenous(vert.ws_P);
  return vert;
}

struct GeomOut {
  float4 gpu_position;
  float3 ws_P;
  float2 offset;
  float4 color;
};

void export_vertex(GeomOut geom_out)
{
  final_color = geom_out.color;
  gl_Position = geom_out.gpu_position;
  gl_Position.xy += geom_out.offset * geom_out.gpu_position.w;
  view_clipping_distances(geom_out.ws_P);
}

void strip_EmitVertex(const uint strip_index,
                      uint out_vertex_id,
                      uint out_primitive_id,
                      GeomOut geom_out)
{
  bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
  /* Maps triangle list primitives to triangle strip indices. */
  uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                         out_primitive_id;

  if (out_strip_index == strip_index) {
    export_vertex(geom_out);
  }
}

void output_vertex_pair(const uint line_id,
                        uint out_vertex_id,
                        uint out_primitive_id,
                        VertOut geom_in[2],
                        float2 offset,
                        float4 color)
{
  GeomOut geom_out;
  geom_out.color = color;
  geom_out.color.a *= alpha;
  geom_out.offset = offset;

  geom_out.gpu_position = geom_in[0].gpu_position;
  geom_out.ws_P = geom_in[0].ws_P;
  strip_EmitVertex(line_id * 2 + 0, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in[1].gpu_position;
  geom_out.ws_P = geom_in[1].ws_P;
  strip_EmitVertex(line_id * 2 + 1, out_vertex_id, out_primitive_id, geom_out);
}

void geometry_main(VertOut geom_in[2],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  float4 v1 = geom_in[0].gpu_position;
  float4 v2 = geom_in[1].gpu_position;

  uint is_active_nurb = (geom_in[1].flag & ACTIVE_NURB);
  uint color_id = (geom_in[1].flag >> COLOR_SHIFT);

  /* Don't output any edges if we don't show handles */
  if (!show_curve_handles && (color_id < 5u)) {
    return;
  }

  bool edge_selected = (((geom_in[1].flag | geom_in[0].flag) & VERT_SELECTED) != 0u);
  bool handle_selected = (show_curve_handles && (((geom_in[1].flag | geom_in[0].flag) &
                                                  VERT_SELECTED_BEZT_HANDLE) != 0u));

  bool is_gpencil = ((geom_in[1].flag & VERT_GPENCIL_BEZT_HANDLE) != 0u);

  /* If handle type is only selected and the edge is not selected, don't show. */
  if ((uint(curve_handle_display) != CURVE_HANDLE_ALL) && (!handle_selected)) {
    /* Nurbs must show the handles always. */
    bool is_u_segment = (((geom_in[1].flag ^ geom_in[0].flag) & EVEN_U_BIT) != 0u);
    if ((!is_u_segment) && (color_id <= 4u)) {
      return;
    }
    if (is_gpencil) {
      return;
    }
  }

  float4 inner_color;
  if (color_id == 0u) {
    inner_color = (edge_selected) ? theme.colors.handle_sel_free : theme.colors.handle_free;
  }
  else if (color_id == 1u) {
    inner_color = (edge_selected) ? theme.colors.handle_sel_auto : theme.colors.handle_auto;
  }
  else if (color_id == 2u) {
    inner_color = (edge_selected) ? theme.colors.handle_sel_vect : theme.colors.handle_vect;
  }
  else if (color_id == 3u) {
    inner_color = (edge_selected) ? theme.colors.handle_sel_align : theme.colors.handle_align;
  }
  else if (color_id == 4u) {
    inner_color = (edge_selected) ? theme.colors.handle_sel_autoclamp :
                                    theme.colors.handle_autoclamp;
  }
  else {
    bool is_selected = (((geom_in[1].flag & geom_in[0].flag) & VERT_SELECTED) != 0u);
    bool is_u_segment = (((geom_in[1].flag ^ geom_in[0].flag) & EVEN_U_BIT) != 0u);
    if (is_u_segment) {
      inner_color = (is_selected) ? theme.colors.nurb_sel_uline : theme.colors.nurb_uline;
    }
    else {
      inner_color = (is_selected) ? theme.colors.nurb_sel_vline : theme.colors.nurb_vline;
    }
  }

  /* Minimize active color bleeding on inner_color. */
  float4 active_color = mix(float4(0.0, 0.0, 0.0, 1.0), inner_color, 0.25f);
  float4 outer_color = (is_active_nurb != 0u) ? active_color : float4(inner_color.rgb, 0.0f);

  float2 v1_2 = (v2.xy / v2.w - v1.xy / v1.w);
  float2 offset = theme.sizes.edge * 4.0f * uniform_buf.size_viewport_inv; /* 4.0f is eyeballed */

  if (abs(v1_2.x * uniform_buf.size_viewport.x) < abs(v1_2.y * uniform_buf.size_viewport.y)) {
    offset.y = 0.0f;
  }
  else {
    offset.x = 0.0f;
  }

  float4 border_color = float4(0.0, 0.0, 0.0, 0.0);
  /* Draw the transparent border (AA). */
  if (is_active_nurb != 0u) {
    offset *= 0.75f; /* Don't make the active "halo" appear very thick. */
    output_vertex_pair(0, out_vertex_id, out_primitive_id, geom_in, offset * 2.0f, border_color);
  }
  /* Draw the outline. */
  output_vertex_pair(1, out_vertex_id, out_primitive_id, geom_in, offset, outer_color);
  /* Draw the core of the line. */
  output_vertex_pair(2, out_vertex_id, out_primitive_id, geom_in, float2(0.0f), inner_color);
  /* Draw the outline. */
  output_vertex_pair(3, out_vertex_id, out_primitive_id, geom_in, -offset, outer_color);
  /* Draw the transparent border (AA). */
  if (is_active_nurb != 0u) {
    output_vertex_pair(4, out_vertex_id, out_primitive_id, geom_in, -offset * 2.0f, border_color);
  }
}

void main()
{
  /* Line list primitive. */
  constexpr uint input_primitive_vertex_count = 2u;
  /* Triangle list primitive (emulating triangle strip). */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 8u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(gl_VertexID) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(gl_VertexID) / output_primitive_vertex_count) %
                          output_primitive_count;
  uint out_invocation_id = (uint(gl_VertexID) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);

  /* Discard by default. */
  gl_Position = float4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
