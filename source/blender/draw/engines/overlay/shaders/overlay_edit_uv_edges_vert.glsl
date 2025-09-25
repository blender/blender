/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_uv_edges)

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"

struct VertIn {
  float2 uv;
  uint flag;
};

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.uv = gpu_attr_load_float2(au, gpu_attr_0, v_i);
#ifdef WIREFRAME
  vert_in.flag = 0u;
#else
  vert_in.flag = gpu_attr_load_uchar4(data, gpu_attr_1, v_i).x;
#endif
  return vert_in;
}

struct VertOut {
  float4 hs_P;
  float2 stipple_start;
  float2 stipple_pos;
  bool selected;
};

VertOut vertex_main(VertIn v_in)
{
  VertOut vert_out;

  float3 world_pos = float3(v_in.uv, 0.0f);
  vert_out.hs_P = drw_point_world_to_homogenous(world_pos);
  /* Snap vertices to the pixel grid to reduce artifacts. */
  float2 half_viewport_res = uniform_buf.size_viewport * 0.5f;
  float2 half_pixel_offset = uniform_buf.size_viewport_inv * 0.5f;
  vert_out.hs_P.xy = floor(vert_out.hs_P.xy * half_viewport_res) / half_viewport_res +
                     half_pixel_offset;

  const uint selection_flag = use_edge_select ? uint(EDGE_UV_SELECT) : uint(VERT_UV_SELECT);
  vert_out.selected = flag_test(v_in.flag, selection_flag);

  /* Move selected edges to the top so that they occlude unselected edges.
   * - Vertices are between 0.0 and 0.2 depth.
   * - Edges between 0.2 and 0.4 depth.
   * - Image pixels are at 0.75 depth.
   * - 1.0 is used for the background. */
  vert_out.hs_P.z = vert_out.selected ? 0.25f : 0.35f;

  /* Avoid precision loss. */
  vert_out.stipple_pos = 500.0f + 500.0f * (vert_out.hs_P.xy / vert_out.hs_P.w);
  vert_out.stipple_start = vert_out.stipple_pos;

  return vert_out;
}

struct GeomOut {
  float4 gpu_position;
  float2 stipple_start;
  float2 stipple_pos;
  float edge_coord;
  bool selected;
};

void export_vertex(GeomOut geom_out)
{
  selection_fac = float(geom_out.selected);
  stipple_start = geom_out.stipple_start;
  stipple_pos = geom_out.stipple_pos;
  edge_coord = geom_out.edge_coord;
  gl_Position = geom_out.gpu_position;
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

void geometry_main(VertOut geom_in[2],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  float2 ss_pos0 = drw_perspective_divide(geom_in[0].hs_P).xy;
  float2 ss_pos1 = drw_perspective_divide(geom_in[1].hs_P).xy;

  float half_size = theme.sizes.edge;
  /* Enlarge edge for outline drawing. */
  /* Factor of 3.0 out of nowhere! Seems to fix issues with float imprecision. */
  half_size += (OVERLAY_UVLineStyle(line_style) == OVERLAY_UV_LINE_STYLE_OUTLINE) ?
                   max(theme.sizes.edge * (do_smooth_wire ? 1.0f : 3.0f), 1.0f) :
                   0.0f;
  /* Add 1 PX for AA. */
  if (do_smooth_wire) {
    half_size += 0.5f;
  }

  float2 line_dir = normalize(ss_pos0 - ss_pos1);
  float2 line_perp = float2(-line_dir.y, line_dir.x);
  float2 edge_ofs = line_perp * uniform_buf.size_viewport_inv * ceil(half_size);
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  edge_ofs *= 2.0f;

  bool select_0 = geom_in[0].selected;
  /* No blending with edge selection. */
  bool select_1 = use_edge_select ? geom_in[0].selected : geom_in[1].selected;

  GeomOut geom_out;
  geom_out.stipple_start = geom_in[0].stipple_start;
  geom_out.stipple_pos = geom_in[0].stipple_pos;
  geom_out.gpu_position = geom_in[0].hs_P + float4(edge_ofs, 0.0f, 0.0f);
  geom_out.edge_coord = half_size;
  geom_out.selected = select_0;
  strip_EmitVertex(0, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in[0].hs_P - float4(edge_ofs, 0.0f, 0.0f);
  geom_out.edge_coord = -half_size;
  strip_EmitVertex(1, out_vertex_id, out_primitive_id, geom_out);

  geom_out.stipple_start = geom_in[1].stipple_start;
  geom_out.stipple_pos = geom_in[1].stipple_pos;
  geom_out.gpu_position = geom_in[1].hs_P + float4(edge_ofs, 0.0f, 0.0f);
  geom_out.edge_coord = half_size;
  geom_out.selected = select_1;
  strip_EmitVertex(2, out_vertex_id, out_primitive_id, geom_out);

  geom_out.gpu_position = geom_in[1].hs_P - float4(edge_ofs, 0.0f, 0.0f);
  geom_out.edge_coord = -half_size;
  strip_EmitVertex(3, out_vertex_id, out_primitive_id, geom_out);
}

void main()
{
  /* Line list primitive. */
  constexpr uint input_primitive_vertex_count = 2u;
  /* Triangle list primitive. */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 2u;
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

  drw_ResourceID_iface.resource_index = drw_resource_id_raw();

  /* Discard by default. */
  gl_Position = float4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
