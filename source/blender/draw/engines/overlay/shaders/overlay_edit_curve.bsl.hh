/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "overlay_common.bsl.hh"

namespace overlay::edit_curve {

namespace handle_point {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] uint data;
};

struct Resources {
  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;

  [[push_constant]] bool show_curve_handles;
  [[push_constant]] int curve_handle_display;
};

[[vertex]]
void handle_points_vert([[resource_table]] const Resources &srt,
                        [[instance_index]] const int inst_index,
                        [[in]] const VertIn &v_in,
                        [[out]] VertOut &v_out,
                        [[point_size]] float &out_point_size,
                        [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                        [[position]] float4 &out_pos)
{
  const UniformData &theme = srt.uni.uniform_buf;

  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices obj = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);

  /* Reuse the FREESTYLE flag to determine is GPencil. */
  bool is_gpencil = (v_in.data & EDGE_FREESTYLE) != 0;

  if ((v_in.data & VERT_SELECTED) == 0) {
    v_out.final_color = is_gpencil ? theme.colors.gpencil_vertex : theme.colors.vert;
  }
  else if ((v_in.data & VERT_ACTIVE) == 0) {
    v_out.final_color = is_gpencil ? theme.colors.gpencil_vertex_select : theme.colors.vert_select;
  }
  else {
    v_out.final_color = theme.colors.edit_mesh_active;
  }

  float3 ws_P = obj.point_object_to_world(v_in.pos);
  out_pos = view.point_world_to_homogenous(ws_P);
  out_point_size = 2.0f * (is_gpencil ? theme.sizes.vertex_gpencil : theme.sizes.vert);

  bool show_handle = srt.show_curve_handles;
  if ((uint(srt.curve_handle_display) == CURVE_HANDLE_SELECTED) &&
      ((v_in.data & VERT_SELECTED_BEZT_HANDLE) == 0u))
  {
    show_handle = false;
  }

  if (!show_handle && ((v_in.data & BEZIER_HANDLE) != 0u)) {
    /* We set the vertex at the camera origin to generate 0 fragments. */
    out_pos = float4(0.0f, 0.0f, -3e36f, 0.0f);
  }

  if (srt.clip.constants.use_clipping) [[static_branch]] {
    srt.clip.set_clipping_distances(ws_P,
                                    clip_distances[0],
                                    clip_distances[1],
                                    clip_distances[2],
                                    clip_distances[3],
                                    clip_distances[4],
                                    clip_distances[5]);
  }
}

}  // namespace handle_point

/* clang-format off */
PipelineGraphic point(        handle_point::handle_points_vert, point_frag, ClippingConstant{.use_clipping = false});
PipelineGraphic point_clipped(handle_point::handle_points_vert, point_frag, ClippingConstant{.use_clipping = true});
/* clang-format on */

namespace handle_line {

struct Handles {
  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;
  [[resource_table]] IndexLoad indices;

  [[storage(0, read), frequency(GEOMETRY)]] float (&pos)[];
  [[storage(1, read), frequency(GEOMETRY)]] uint (&data)[];

  [[push_constant]] int2 gpu_attr_0;
  [[push_constant]] int2 gpu_attr_1;

  [[push_constant]] bool show_curve_handles;
  [[push_constant]] int curve_handle_display;
  [[push_constant]] float alpha;

  struct VertIn {
    /* Local Position. */
    float3 ls_P;
    /* Edit Flags and Data. */
    uint e_data;
  };

  VertIn input_assembly(uint in_vertex_id) const
  {
    uint v_i = indices.load(in_vertex_id);
    float3 ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
    return {
        .ls_P = ls_P,
        .e_data = data[gpu_attr_load_index(v_i, gpu_attr_1)],
    };
  }

  struct VertOut {
    float3 ws_P;
    float4 gpu_position;
    uint flag;
  };

  VertOut vertex_main(ObjectMatrices obj, ViewMatrices view, VertIn vert_in) const
  {
    VertOut vert;
    vert.flag = vert_in.e_data;
    vert.ws_P = obj.point_object_to_world(vert_in.ls_P);
    vert.gpu_position = view.point_world_to_homogenous(vert.ws_P);
    return vert;
  }

  struct GeomOut {
    float4 gpu_position;
    float3 ws_P;
    float2 offset;
    float4 color;
  };

  void strip_EmitVertex(const uint strip_index,
                        uint out_vertex_id,
                        uint out_primitive_id,
                        GeomOut geom_out,
                        GeomOut &out_geom) const
  {
    bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
    /* Maps triangle list primitives to triangle strip indices. */
    uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                           out_primitive_id;

    if (out_strip_index == strip_index) {
      out_geom = geom_out;
    }
  }

  void output_vertex_pair(const uint line_id,
                          uint out_vertex_id,
                          uint out_primitive_id,
                          VertOut geom_in[2],
                          float2 offset,
                          float4 color,
                          GeomOut &out_geom) const
  {
    GeomOut geom_out;
    geom_out.color = color;
    geom_out.color.a *= alpha;
    geom_out.offset = offset;

    geom_out.gpu_position = geom_in[0].gpu_position;
    geom_out.ws_P = geom_in[0].ws_P;
    strip_EmitVertex(line_id * 2 + 0, out_vertex_id, out_primitive_id, geom_out, out_geom);

    geom_out.gpu_position = geom_in[1].gpu_position;
    geom_out.ws_P = geom_in[1].ws_P;
    strip_EmitVertex(line_id * 2 + 1, out_vertex_id, out_primitive_id, geom_out, out_geom);
  }

  void geometry_main(VertOut geom_in[2],
                     uint out_vert,
                     uint out_prim,
                     uint /*out_invocation_id*/,
                     GeomOut &out_geom) const
  {
    const UniformData &theme = uni.uniform_buf;

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
    float4 active_color = mix(float4(0.0f, 0.0f, 0.0f, 1.0f), inner_color, 0.25f);
    float4 outer_color = (is_active_nurb != 0u) ? active_color : float4(inner_color.rgb, 0.0f);

    float2 v1_2 = (v2.xy / v2.w - v1.xy / v1.w);
    float2 offset = theme.sizes.edge * 4.0f * theme.size_viewport_inv; /* 4.0f is eyeballed */

    if (abs(v1_2.x * theme.size_viewport.x) < abs(v1_2.y * theme.size_viewport.y)) {
      offset.y = 0.0f;
    }
    else {
      offset.x = 0.0f;
    }

    float4 border_color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    /* Draw the transparent border (AA). */
    if (is_active_nurb != 0u) {
      offset *= 0.75f; /* Don't make the active "halo" appear very thick. */
      output_vertex_pair(0, out_vert, out_prim, geom_in, offset * 2.0f, border_color, out_geom);
    }
    /* Draw the outline. */
    output_vertex_pair(1, out_vert, out_prim, geom_in, offset, outer_color, out_geom);
    /* Draw the core of the line. */
    output_vertex_pair(2, out_vert, out_prim, geom_in, float2(0.0f), inner_color, out_geom);
    /* Draw the outline. */
    output_vertex_pair(3, out_vert, out_prim, geom_in, -offset, outer_color, out_geom);
    /* Draw the transparent border (AA). */
    if (is_active_nurb != 0u) {
      output_vertex_pair(4, out_vert, out_prim, geom_in, -offset * 2.0f, border_color, out_geom);
    }
  }
};

[[vertex]]
void handle_lines_vert([[resource_table]] const Handles &srt,
                       [[instance_index]] const int inst_index,
                       [[vertex_id]] const int vert_id,
                       [[out]] VertOutSmooth &v_out,
                       [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                       [[position]] float4 &out_pos)
{
  const uint id = srt.resources.get(inst_index).resource_id<1>();
  const ObjectMatrices obj = srt.models.get(id);
  const ViewMatrices view = srt.views.get(0);

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

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(vert_id) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(vert_id) / output_primitive_vertex_count) % output_primitive_count;
  uint out_invocation_id = (uint(vert_id) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  Handles::VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u);

  Handles::VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = srt.vertex_main(obj, view, vert_in[0]);
  vert_out[1] = srt.vertex_main(obj, view, vert_in[1]);

  Handles::GeomOut geom_out = {
      /* Discard by default. */
      .gpu_position = float4(NAN_FLT),
      .ws_P = float3(0.0f),
      .offset = float2(0.0f),
      .color = float4(0.0f),
  };
  srt.geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id, geom_out);

  v_out.final_color = geom_out.color;
  /* Not used. But needs to be written. */
  v_out.edge_pos = v_out.edge_start = float2(0.0f);

  out_pos = geom_out.gpu_position;
  out_pos.xy += geom_out.offset * out_pos.w;

  if (srt.clip.constants.use_clipping) [[static_branch]] {
    srt.clip.set_clipping_distances(geom_out.ws_P,
                                    clip_distances[0],
                                    clip_distances[1],
                                    clip_distances[2],
                                    clip_distances[3],
                                    clip_distances[4],
                                    clip_distances[5]);
  }
}

}  // namespace handle_line

/* clang-format off */
PipelineGraphic handle(        handle_line::handle_lines_vert, wire_frag_smooth, ClippingConstant{.use_clipping = false});
PipelineGraphic handle_clipped(handle_line::handle_lines_vert, wire_frag_smooth, ClippingConstant{.use_clipping = true});
/* clang-format on */

namespace shape_wire {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float3 nor;
  [[attribute(2)]] float3 tangent;
  [[attribute(3)]] float rad;
};

struct Resources {
  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;

  [[push_constant]] float normal_size;
};

[[vertex]]
void wire_vert([[resource_table]] const Resources &srt,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[in]] const VertIn &v_in,
               [[out]] VertOut &v_out,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[position]] float4 &out_pos)
{
  const UniformData &theme = srt.uni.uniform_buf;

  const uint id = srt.resources.get(inst_index).resource_id<1>();
  const ObjectMatrices obj = srt.models.get(id);
  const ViewMatrices view = srt.views.get(0);

  const float flip = inst_index != 0 ? -1.0f : 1.0f;

  const float3 final_pos = (vert_id % 2 != 0) ? v_in.pos :
                                                v_in.pos + srt.normal_size * v_in.rad *
                                                               (flip * v_in.nor - v_in.tangent);

  const float3 ws_P = obj.point_object_to_world(final_pos);
  out_pos = view.point_world_to_homogenous(ws_P);

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      theme.size_viewport;

  v_out.final_color = theme.colors.wire_edit;

  if (srt.clip.constants.use_clipping) [[static_branch]] {
    srt.clip.set_clipping_distances(ws_P,
                                    clip_distances[0],
                                    clip_distances[1],
                                    clip_distances[2],
                                    clip_distances[3],
                                    clip_distances[4],
                                    clip_distances[5]);
  }
}

}  // namespace shape_wire

/* clang-format off */
PipelineGraphic wire(        shape_wire::wire_vert, wire_frag, ClippingConstant{.use_clipping = false});
PipelineGraphic wire_clipped(shape_wire::wire_vert, wire_frag, ClippingConstant{.use_clipping = true});
/* clang-format on */

namespace normal {

struct Normals {
  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;
  [[resource_table]] IndexLoad indices;

  [[storage(0, read), frequency(GEOMETRY)]] float (&pos)[];
  [[storage(1, read), frequency(GEOMETRY)]] float (&rad)[];
  [[storage(2, read), frequency(GEOMETRY)]] uint (&nor)[];
  [[storage(3, read), frequency(GEOMETRY)]] uint (&tangent)[];

  [[push_constant]] int2 gpu_attr_0;
  [[push_constant]] int2 gpu_attr_1;
  [[push_constant]] int2 gpu_attr_2;
  [[push_constant]] int2 gpu_attr_3;

  [[push_constant]] float normal_size;
  [[push_constant]] bool use_hq_normals;

  float4 load_normal(uint index) const
  {
    if (use_hq_normals) {
      return gpu_attr_load_short4_snorm(nor, gpu_attr_2, index);
    }
    return gpu_attr_load_uint_1010102_snorm(nor, gpu_attr_2, index);
  }

  float4 load_tangent(uint index) const
  {
    if (use_hq_normals) {
      return gpu_attr_load_short4_snorm(tangent, gpu_attr_3, index);
    }
    return gpu_attr_load_uint_1010102_snorm(tangent, gpu_attr_3, index);
  }
};

/* Draw arrows */
[[vertex]]
void normal_vert([[resource_table]] const Normals &srt,
                 [[instance_index]] const int inst_index,
                 [[vertex_id]] const int vert_id,
                 [[out]] VertOut &v_out,
                 [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                 [[position]] float4 &out_pos)
{
  const UniformData &theme = srt.uni.uniform_buf;

  const uint id = srt.resources.get(inst_index).resource_id<1>();
  const ObjectMatrices obj = srt.models.get(id);
  const ViewMatrices view = srt.views.get(0);

  /* Line list primitive. */
  constexpr uint input_primitive_vertex_count = 2u;
  /* Line list primitive. */
  constexpr uint output_primitive_vertex_count = 2u;
  constexpr uint output_primitive_count = 2u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint v_i = srt.indices.load(in_primitive_first_vertex);
  float3 ls_P = gpu_attr_load_float3(srt.pos, srt.gpu_attr_0, v_i);
  float radius = srt.rad[gpu_attr_load_index(v_i, srt.gpu_attr_1)];
  float3 ls_N = srt.load_normal(v_i).xyz;
  float3 ls_T = srt.load_tangent(v_i).xyz;

  if ((vert_id & 1) == 0) {
    float flip = ((vert_id & 2) == 0) ? -1.0f : 1.0f;
    ls_P += srt.normal_size * radius * (flip * ls_N - ls_T);
  }

  float3 ws_P = obj.point_object_to_world(ls_P);
  out_pos = view.point_world_to_homogenous(ws_P);

  /* Convert to screen position [0..sizeVp]. */
  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      theme.size_viewport;

  v_out.final_color = theme.colors.wire_edit;

  if (srt.clip.constants.use_clipping) [[static_branch]] {
    srt.clip.set_clipping_distances(ws_P,
                                    clip_distances[0],
                                    clip_distances[1],
                                    clip_distances[2],
                                    clip_distances[3],
                                    clip_distances[4],
                                    clip_distances[5]);
  }
}

}  // namespace normal

/* clang-format off */
PipelineGraphic normals(        normal::normal_vert, wire_frag_smooth, ClippingConstant{.use_clipping = false});
PipelineGraphic normals_clipped(normal::normal_vert, wire_frag_smooth, ClippingConstant{.use_clipping = true});
/* clang-format on */

}  // namespace overlay::edit_curve
