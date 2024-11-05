/* SPDX-FileCopyrightText: 2018-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#define M_TAN_PI_BY_8 tan(M_PI / 8)
#define M_TAN_3_PI_BY_8 tan(3 * M_PI / 8)
#define M_SQRT2_BY_2 (M_SQRT2 / 2)

struct VertIn {
  /* Local Position. */
  vec3 ls_P;
  /* Edit Flags and Data. */
  uint e_data;
  float sel;
};

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  vert_in.e_data = data[gpu_attr_load_index(v_i, gpu_attr_1)];
  vert_in.sel = selection[gpu_attr_load_index(v_i, gpu_attr_2)];
  return vert_in;
}

struct VertOut {
  vec3 ws_P;
  vec4 gpu_position;
  uint flag;
  float sel;
};

VertOut vertex_main(VertIn vert_in)
{
  VertOut vert;
  vert.flag = vert_in.e_data;
  vert.ws_P = point_object_to_world(vert_in.ls_P);
  vert.gpu_position = point_world_to_ndc(vert.ws_P);
  vert.sel = vert_in.sel;
  return vert;
}

struct GeomOut {
  vec4 gpu_position;
  vec3 ws_P;
  vec2 offset;
  vec4 color;
};

void export_vertex(GeomOut geom_out)
{
  finalColor = geom_out.color;
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
                        vec2 offset,
                        vec4 color)
{
  GeomOut geom_out;
  geom_out.color = color;
  geom_out.offset = offset;

  geom_out.gpu_position = geom_in[0].gpu_position;
  geom_out.ws_P = geom_in[0].ws_P;
  strip_EmitVertex(line_id * 2 + 0, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in[1].gpu_position;
  geom_out.ws_P = geom_in[1].ws_P;
  strip_EmitVertex(line_id * 2 + 1, out_vertex_id, out_primitive_id, geom_out);
}

float4 get_bezier_handle_color(uint color_id, float sel)
{
  switch (color_id) {
    case 0u: /* BEZIER_HANDLE_FREE */
      return mix(globalsBlock.color_handle_free, globalsBlock.color_handle_sel_free, sel);
    case 1u: /* BEZIER_HANDLE_AUTO */
      return mix(globalsBlock.color_handle_auto, globalsBlock.color_handle_sel_auto, sel);
    case 2u: /* BEZIER_HANDLE_VECTOR */
      return mix(globalsBlock.color_handle_vect, globalsBlock.color_handle_sel_vect, sel);
    case 3u: /* BEZIER_HANDLE_ALIGN */
      return mix(globalsBlock.color_handle_align, globalsBlock.color_handle_sel_align, sel);
  }
  return mix(globalsBlock.color_handle_autoclamp, globalsBlock.color_handle_sel_autoclamp, sel);
}

void geometry_main(VertOut geom_in[2],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  vec4 v1 = geom_in[0].gpu_position;
  vec4 v2 = geom_in[1].gpu_position;

  bool is_active = (geom_in[0].flag & EDIT_CURVES_ACTIVE_HANDLE) != 0u;
  uint color_id = (geom_in[0].flag >> EDIT_CURVES_HANDLE_TYPES_SHIFT) & 7u;

  bool is_bezier_handle = (geom_in[0].flag & EDIT_CURVES_BEZIER_HANDLE) != 0;
  /* Don't output any edges if we don't show handles */
  if ((uint(curveHandleDisplay) == CURVE_HANDLE_NONE) && is_bezier_handle) {
    return;
  }

  /* If handle type is only selected and the edge is not selected, don't show.
   * Nurbs and other curves must show the handles always. */
  if ((uint(curveHandleDisplay) == CURVE_HANDLE_SELECTED) && is_bezier_handle && !is_active) {
    return;
  }

  bool is_odd_vertex = (out_vertex_id & 1u) != 0u;
  bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
  uint line_end_point = (is_odd_primitive && !is_odd_vertex) ||
                                (!is_odd_primitive && is_odd_vertex) ?
                            1 :
                            0;
  vec4 inner_color;
  if ((geom_in[line_end_point].flag & (EDIT_CURVES_BEZIER_HANDLE | EDIT_CURVES_BEZIER_KNOT)) != 0u)
  {
    inner_color = get_bezier_handle_color(color_id, geom_in[line_end_point].sel);
  }
  else if ((geom_in[line_end_point].flag & EDIT_CURVES_NURBS_CONTROL_POINT) != 0u) {
    inner_color = mix(globalsBlock.color_nurb_uline,
                      globalsBlock.color_nurb_sel_uline,
                      geom_in[line_end_point].sel);
  }
  else {
    inner_color = mix(
        globalsBlock.color_wire, globalsBlock.color_vertex_select, geom_in[line_end_point].sel);
  }

  /* Minimize active color bleeding on inner_color. */
  vec4 active_color = mix(colorActiveSpline, inner_color, 0.25);
  vec4 outer_color = is_active ? active_color : vec4(inner_color.rgb, 0.0);

  vec2 v1_2 = (v2.xy / v2.w - v1.xy / v1.w);
  vec2 offset = sizeEdge * 4.0 * sizeViewportInv; /* 4.0 is eyeballed */

  if (abs(v1_2.x) <= M_TAN_PI_BY_8 * abs(v1_2.y)) {
    offset.y = 0.0;
  }
  else if (abs(v1_2.x) <= M_TAN_3_PI_BY_8 * abs(v1_2.y)) {
    offset = offset * vec2(-M_SQRT2_BY_2 * sign(v1_2.x), M_SQRT2_BY_2 * sign(v1_2.y));
  }
  else {
    offset.x = 0.0;
  }

  vec4 border_color = vec4(colorActiveSpline.rgb, 0.0);
  /* Draw the transparent border (AA). */
  if (is_active) {
    offset *= 0.75; /* Don't make the active "halo" appear very thick. */
    output_vertex_pair(0, out_vertex_id, out_primitive_id, geom_in, offset * 2.0, border_color);
  }
  /* Draw the outline. */
  output_vertex_pair(1, out_vertex_id, out_primitive_id, geom_in, offset, outer_color);
  /* Draw the core of the line. */
  output_vertex_pair(2, out_vertex_id, out_primitive_id, geom_in, vec2(0.0), inner_color);
  /* Draw the outline. */
  output_vertex_pair(3, out_vertex_id, out_primitive_id, geom_in, -offset, outer_color);
  /* Draw the transparent border (AA). */
  if (is_active) {
    output_vertex_pair(4, out_vertex_id, out_primitive_id, geom_in, -offset * 2.0, border_color);
  }
}

void main()
{
  /* Line list primitive. */
  const uint input_primitive_vertex_count = 2u;
  /* Triangle list primitive (emulating triangle strip). */
  const uint ouput_primitive_vertex_count = 3u;
  const uint ouput_primitive_count = 8u;
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

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);

  /* Discard by default. */
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
