/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"
#include "overlay_common.bsl.hh"

namespace overlay::edit_mesh {

struct EditVertData {
  uint face_uv_flag;
  uint vert_edge_flag;
  float vertex_crease;
  float edge_crease;
  float bevel_weight;
};

struct Resources {
  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;

  [[sampler(0)]] sampler2DDepth depth_tx;

  /* Per view factor. */
  [[push_constant]] float ndc_offset_factor;

  /* Per pass factor. */
  [[push_constant]] float ndc_offset;
  [[push_constant]] bool wire_shading;
  [[push_constant]] bool select_face;
  [[push_constant]] bool select_edge;
  [[push_constant]] float alpha;
  [[push_constant]] float retopology_offset;
  [[push_constant]] int4 data_mask;

  float4 edge_color_outer(EditVertData edit) const
  {
    const UniformData &theme = uni.uniform_buf;
    float4 color = float4(0.0f);
    color = flag_test(edit.vert_edge_flag, EDGE_FREESTYLE) ? theme.colors.edge_freestyle : color;
    color = flag_test(edit.vert_edge_flag, EDGE_SHARP) ? theme.colors.edge_sharp : color;
    color = (edit.edge_crease > 0.0f) ? float4(theme.colors.edge_crease.rgb, edit.edge_crease) :
                                        color;
    color = (edit.bevel_weight > 0.0f) ? float4(theme.colors.edge_bweight.rgb, edit.bevel_weight) :
                                         color;
    color = flag_test(edit.vert_edge_flag, EDGE_SEAM) ? theme.colors.edge_seam : color;
    return color;
  }

  float4 edge_color_inner(EditVertData edit) const
  {
    const UniformData &theme = uni.uniform_buf;
    float4 color = theme.colors.wire_edit;
    float4 selected_edge_col = (select_edge) ? theme.colors.edge_mode_select :
                                               theme.colors.edge_select;
    color = flag_test(edit.vert_edge_flag, EDGE_SELECTED) ? selected_edge_col : color;
    color = flag_test(edit.vert_edge_flag, EDGE_ACTIVE) ? theme.colors.edit_mesh_active : color;
    color.a = 1.0f;
    return color;
  }

  float4 edge_vertex_color(EditVertData edit) const
  {
    const UniformData &theme = uni.uniform_buf;
    /* Edge color in vertex selection mode. */
    float4 selected_edge_col = (select_edge) ? theme.colors.edge_mode_select :
                                               theme.colors.edge_select;
    bool edge_selected = flag_test(edit.vert_edge_flag, (VERT_ACTIVE | VERT_SELECTED));
    float4 color = (edge_selected) ? selected_edge_col : theme.colors.wire_edit;
    color.a = 1.0f;
    return color;
  }

  float4 vertex_color(EditVertData edit) const
  {
    const UniformData &theme = uni.uniform_buf;
    if (flag_test(edit.vert_edge_flag, VERT_ACTIVE)) {
      return float4(theme.colors.edit_mesh_active.xyz, 1.0f);
    }
    if (flag_test(edit.vert_edge_flag, VERT_SELECTED)) {
      return theme.colors.vert_select;
    }
    /* Full crease color if not selected nor active. */
    if (edit.vertex_crease > 0.0f) {
      return mix(theme.colors.vert, theme.colors.edge_crease, edit.vertex_crease);
    }
    return theme.colors.vert;
  }

  float4 face_color(EditVertData edit) const
  {
    const UniformData &theme = uni.uniform_buf;
    bool face_freestyle = flag_test(edit.face_uv_flag, FACE_FREESTYLE);
    bool face_selected = flag_test(edit.face_uv_flag, FACE_SELECTED);
    bool face_active = flag_test(edit.face_uv_flag, FACE_ACTIVE);
    bool face_retopo = (retopology_offset > 0.0f);
    float4 selected_face_col = (select_face) ? theme.colors.face_mode_select :
                                               theme.colors.face_select;
    float4 color = theme.colors.face;
    color = face_retopo ? theme.colors.face_retopology : color;
    color = face_freestyle ? theme.colors.face_freestyle : color;
    color = face_selected ? selected_face_col : color;
    if (select_face && face_active) {
      color = mix(selected_face_col, theme.colors.edit_mesh_active, 0.5f);
      color.a = selected_face_col.a;
    }
    if (wire_shading) {
      /* Lower face selection opacity for better wireframe visibility. */
      color.a = (face_selected) ? color.a * 0.6f : color.a;
    }
    else {
      /* Don't always fill 'theme.colors.face'. */
      color.a = (select_face || face_selected || face_active || face_freestyle || face_retopo) ?
                    color.a :
                    0.0f;
    }
    return color;
  }

  float4 facedot_color(float facedot_flag) const
  {
    const UniformData &theme = uni.uniform_buf;
    if (facedot_flag < 0.0f) {
      return float4(theme.colors.edit_mesh_active.xyz, 1.0f);
    }
    if (facedot_flag > 0.0f) {
      return theme.colors.facedot;
    }
    return theme.colors.vert;
  }

  bool test_occlusion(float4 hs_P) const
  {
    float3 ndc = (hs_P.xyz / hs_P.w) * 0.5f + 0.5f;
    float4 depths = textureGather(depth_tx, ndc.xy);
    return all(greaterThan(float4(ndc.z), depths));
  }

  bool test_fragment_occlusion(float4 frag_co) const
  {
    return frag_co.z > texelFetch(depth_tx, int2(frag_co.xy), 0).r;
  }

  float4 facing_coloring(float4 color, float3 vs_N, float3 vs_V) const
  {
    const UniformData &theme = uni.uniform_buf;
    /* Facing based color blend */
    vs_N = normalize(vs_N + 1e-4f);
    float facing = 1.0f - abs(dot(vs_N, vs_V)) * 0.2f;

    /* Do interpolation in a non-linear space to have a better visual result. */
    color.rgb = mix(color.rgb,
                    non_linear_blend_color(theme.colors.edit_mesh_middle.rgb, color.rgb, facing),
                    theme.fresnel_mix_edit);

    return color;
  }

  EditVertData unpack_vert_data(uint4 in_data) const
  {
    uint4 m_data = in_data & uint4(data_mask);
    return {
        .face_uv_flag = m_data.x,
        .vert_edge_flag = m_data.y,
        .vertex_crease = float(m_data.z >> 4) / 15.0f,
        .edge_crease = float(m_data.z & 0xFu) / 15.0f,
        .bevel_weight = float(m_data.w) / 255.0f,
    };
  }
};

/* -------------------------------------------------------------------- */
/** \name Vertices
 * \{ */

namespace vertex {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] uint4 data;
  [[attribute(2)]] float3 vnor;
};

struct VertOut {
  [[flat]] float4 color;
  [[flat]] float vertex_crease;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[instance_index]] const int inst_index,
               [[in]] const VertIn &v_in,
               [[out]] VertOut &v_out,
               [[point_size]] float &out_point_size,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[position]] float4 &out_pos)
{
  const UniformData &theme = srt.uni.uniform_buf;

  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);
  EditVertData edit = srt.unpack_vert_data(v_in.data);

  float3 ws_N = model.normal_object_to_world(v_in.vnor);
  float3 ws_P = model.point_object_to_world(v_in.pos);
  float3 vs_N = view.normal_world_to_view(ws_N);
  float3 vs_P = view.point_world_to_view(ws_P);
  out_pos = view.point_view_to_homogenous(vs_P);
  /* Offset Z position for retopology overlay. */
  out_pos.z += get_homogenous_z_offset(view.winmat, vs_P.z, out_pos.w, srt.retopology_offset);
  /* Make selected and active vertex always on top. */
  if (flag_test(edit.vert_edge_flag, VERT_SELECTED)) {
    out_pos.z -= 5e-7f * abs(out_pos.w);
  }
  if (flag_test(edit.vert_edge_flag, VERT_ACTIVE)) {
    out_pos.z -= 5e-7f * abs(out_pos.w);
  }
  /* NDC offset to avoid Z fighting. */
  out_pos.z -= srt.ndc_offset_factor * srt.ndc_offset;

  v_out.vertex_crease = edit.vertex_crease;
  v_out.color = srt.vertex_color(edit);
  /* Occluded geometry fading. */
  v_out.color.a *= srt.test_occlusion(out_pos) ? srt.alpha : 1.0f;
  /* Viewing angle dependent (aka "Fresnel") coloring. */
  v_out.color = srt.facing_coloring(v_out.color, vs_N, view.view_incident_vector(vs_P));

  /* Make point bigger when crease is visible. */
  out_point_size = theme.sizes.vert * ((edit.vertex_crease > 0.0f) ? 3.0f : 2.0f);

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

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out,
               [[point_coord]] const float2 point_co)
{
  const UniformData &theme = srt.uni.uniform_buf;

  float2 centered = point_co - float2(0.5f);
  float dist_squared = dot(centered, centered);
  constexpr float rad_squared = 0.25f;
  /* Round point with jagged edges. */
  if (dist_squared > rad_squared) {
    gpu_discard_fragment();
    return;
  }

  frag_out.color = v_out.color;
  float midStroke = 0.5f * rad_squared;
  if (v_out.vertex_crease > 0.0f && dist_squared > midStroke) {
    frag_out.color.rgb = mix(
        frag_out.color.rgb, theme.colors.edge_crease.rgb, v_out.vertex_crease);
  }

  frag_out.line_output = float4(0.0f);
}

}  // namespace vertex

PipelineGraphic vert(vertex::vert_main,
                     vertex::frag_main,
                     ClippingConstant{.use_clipping = false});
PipelineGraphic vert_clipped(vertex::vert_main,
                             vertex::frag_main,
                             ClippingConstant{.use_clipping = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edges
 * \{ */

namespace edges {

struct EditEdge {
  [[resource_table]] IndexLoad indices;
  [[resource_table]] Resources res;

  [[storage(0, read), frequency(GEOMETRY)]] const float (&pos)[];
  [[storage(1, read), frequency(GEOMETRY)]] const uint (&vnor)[];
  [[storage(2, read), frequency(GEOMETRY)]] const uint (&data)[];

  [[push_constant]] const int2 gpu_attr_0;
  [[push_constant]] const int2 gpu_attr_1;
  [[push_constant]] const int2 gpu_attr_2;
  [[push_constant]] const bool do_smooth_wire;
  [[push_constant]] const bool use_vertex_selection;

  float edge_step(float dist) const
  {
    if (do_smooth_wire) {
      return smoothstep(LINE_SMOOTH_START, LINE_SMOOTH_END, dist);
    }
    return step(0.5f, dist);
  }

  struct VertIn {
    float3 lP;
    float3 lN;
    uint4 data;
  };

  VertIn input_assembly(uint in_vertex_id) const
  {
    uint v_i = indices.load(in_vertex_id);

    VertIn vert_in;
    {
      float3 lP = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
      vert_in.lP = lP;
    }
    if (gpu_attr_1.x == 1) {
      float4 lN = gpu_attr_load_uint_1010102_snorm(vnor, gpu_attr_1, v_i);
      vert_in.lN = lN.xyz;
    }
    else if (gpu_attr_1.x == 2) {
      float4 lN = gpu_attr_load_short4_snorm(vnor, gpu_attr_1, v_i);
      vert_in.lN = lN.xyz;
    }
    else {
      uint v = gpu_attr_load_index(v_i, gpu_attr_1);
      vert_in.lN = uintBitsToFloat(uint3(vnor[v + 0], vnor[v + 1], vnor[v + 2]));
    }
    {
      uint v = gpu_attr_load_index(v_i, gpu_attr_2);
      vert_in.data = gpu_attr_decode_uchar4_to_uint4(data[v]);
    }
    return vert_in;
  }

  struct VertOut {
    float4 color;
    float4 color_outer;
    float3 ws_P;
    float4 hs_P;
    bool select_override;
  };

  VertOut vertex_main(VertIn v_in, int inst_index) const
  {
    uint id = res.resources.get(inst_index).resource_id<1>();
    ObjectMatrices model = res.models.get(id);
    ViewMatrices view = res.views.get(0);
    EditVertData edit = res.unpack_vert_data(v_in.data);

    VertOut vert;

    vert.ws_P = model.point_object_to_world(v_in.lP);
    float3 ws_N = model.normal_object_to_world(v_in.lN);
    float3 vs_P = view.point_world_to_view(vert.ws_P);
    float3 vs_N = view.normal_world_to_view(ws_N);
    vert.hs_P = view.point_view_to_homogenous(vs_P);
    /* Offset Z position for retopology overlay. */
    vert.hs_P.z += get_homogenous_z_offset(
        view.winmat, vs_P.z, vert.hs_P.w, res.retopology_offset);
    if (vert.color.a > 0.0f) {
      vert.hs_P.z -= 5e-7f * abs(vert.hs_P.w);
    }
    vert.hs_P.z -= res.ndc_offset_factor * res.ndc_offset;

    if (use_vertex_selection) {
      vert.color = res.edge_vertex_color(edit);
      vert.select_override = flag_test(edit.vert_edge_flag, EDGE_SELECTED);
    }
    else {
      vert.color = res.edge_color_inner(edit);
      vert.select_override = true;
    }
    vert.color_outer = res.edge_color_outer(edit);
    vert.color = res.facing_coloring(vert.color, vs_N, view.view_incident_vector(vs_P));

    return vert;
  }

  struct GeomOut {
    float3 ws_P;
    float4 hs_P;
    float4 color;
    float4 color_outer;
    float edge_coord;
  };

  void strip_EmitVertex(const uint strip_index,
                        uint out_vertex_id,
                        uint out_primitive_id,
                        GeomOut geom,
                        GeomOut &out_geom) const
  {
    bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
    /* Maps triangle list primitives to triangle strip indices. */
    uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                           out_primitive_id;

    if (out_strip_index == strip_index) {
      out_geom = geom;
    }
  }

  void do_vertex(const uint strip_index,
                 uint out_vertex_id,
                 uint out_primitive_id,
                 float4 final_color,
                 float4 hs_P,
                 float3 ws_P,
                 float coord,
                 float2 offset,
                 GeomOut &out_geom) const
  {
    GeomOut geom_out;
    geom_out.ws_P = ws_P;
    geom_out.color = final_color;
    geom_out.edge_coord = coord;
    geom_out.hs_P = hs_P;
    /* Multiply offset by 2 because gl_Position range is [-1..1]. */
    geom_out.hs_P.xy += offset * 2.0f * hs_P.w;
    strip_EmitVertex(strip_index, out_vertex_id, out_primitive_id, geom_out, out_geom);
  }

  void geometry_main(VertOut geom_in[2],
                     uint out_vert_id,
                     uint out_prim_id,
                     uint /*out_invocation_id*/,
                     GeomOut &out_geom) const
  {
    const UniformData &theme = res.uni.uniform_buf;
    float2 ss_pos[2];

    /* Clip line against near plane to avoid deformed lines. */
    float4 pos0 = geom_in[0].hs_P;
    float4 pos1 = geom_in[1].hs_P;
    float2 pz_ndc = float2(pos0.z / pos0.w, pos1.z / pos1.w);
    bool2 clipped = lessThan(pz_ndc, float2(-1.0f));
    if (all(clipped)) {
      /* Totally clipped. */
      return;
    }

    float4 pos01 = pos0 - pos1;
    float ofs = abs((pz_ndc.y + 1.0f) / (pz_ndc.x - pz_ndc.y));
    if (clipped.y) {
      pos1 += pos01 * ofs;
    }
    else if (clipped.x) {
      pos0 -= pos01 * (1.0f - ofs);
    }

    ss_pos[0] = pos0.xy / pos0.w;
    ss_pos[1] = pos1.xy / pos1.w;

    float2 line = ss_pos[0] - ss_pos[1];
    line = abs(line) * res.uni.uniform_buf.size_viewport;

    out_geom.color_outer = geom_in[0].color_outer;
    float half_size = theme.sizes.edge;
    /* Enlarge edge for flag display. */
    half_size += (out_geom.color_outer.a > 0.0f) ? max(theme.sizes.edge, 1.0f) : 0.0f;

    if (do_smooth_wire) {
      /* Add 1px for AA */
      half_size += 0.5f;
    }

    float3 edge_ofs = float3(half_size * theme.size_viewport_inv, 0.0f);

    bool horizontal = line.x > line.y;
    edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

    float3 wpos0 = geom_in[0].ws_P;
    float3 wpos1 = geom_in[1].ws_P;

    float4 final_color1 = geom_in[0].color;
    float4 final_color2 = (geom_in[0].select_override) ? geom_in[0].color : geom_in[1].color;

    do_vertex(
        0, out_vert_id, out_prim_id, final_color1, pos0, wpos0, half_size, edge_ofs.xy, out_geom);
    do_vertex(1,
              out_vert_id,
              out_prim_id,
              final_color1,
              pos0,
              wpos0,
              -half_size,
              -edge_ofs.xy,
              out_geom);

    do_vertex(
        2, out_vert_id, out_prim_id, final_color2, pos1, wpos1, half_size, edge_ofs.xy, out_geom);
    do_vertex(3,
              out_vert_id,
              out_prim_id,
              final_color2,
              pos1,
              wpos1,
              -half_size,
              -edge_ofs.xy,
              out_geom);
  }
};

struct VertOut {
  [[smooth]] float4 color;
  [[flat]] float4 color_outer;
  [[no_perspective]] float edge_coord;
};

[[vertex]]
void vert_main([[resource_table]] const EditEdge &srt,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[out]] VertOut &v_out,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[position]] float4 &out_pos)
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

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(vert_id) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(vert_id) / output_primitive_vertex_count) % output_primitive_count;
  uint out_invocation_id = (uint(vert_id) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  EditEdge::VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u);

  EditEdge::VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = srt.vertex_main(vert_in[0], inst_index);
  vert_out[1] = srt.vertex_main(vert_in[1], inst_index);

  EditEdge::GeomOut geom = {};
  /* Discard by default. */
  geom.hs_P = float4(NAN_FLT);
  srt.geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id, geom);

  v_out.color = geom.color;
  v_out.color_outer = geom.color_outer;
  v_out.edge_coord = geom.edge_coord;
  out_pos = geom.hs_P;

  if (srt.res.clip.constants.use_clipping) [[static_branch]] {
    srt.res.clip.set_clipping_distances(geom.ws_P,
                                        clip_distances[0],
                                        clip_distances[1],
                                        clip_distances[2],
                                        clip_distances[3],
                                        clip_distances[4],
                                        clip_distances[5]);
  }
}

[[fragment]]
void frag_main([[resource_table]] const EditEdge &srt,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out,
               [[frag_coord]] const float4 frag_co)
{
  const UniformData &theme = srt.res.uni.uniform_buf;

  float dist = abs(v_out.edge_coord) - max(theme.sizes.edge - 0.5f, 0.0f);
  float dist_outer = dist - max(theme.sizes.edge, 1.0f);
  float mix_w = srt.edge_step(dist);
  float mix_w_outer = srt.edge_step(dist_outer);
  /* Line color & alpha. */
  frag_out.color = mix(v_out.color_outer, v_out.color, 1.0f - mix_w * v_out.color_outer.a);
  /* Line edges shape. */
  frag_out.color.a *= 1.0f - (v_out.color_outer.a > 0.0f ? mix_w_outer : mix_w);

  frag_out.color.a *= srt.res.test_fragment_occlusion(frag_co) ? srt.res.alpha : 1.0f;
  frag_out.line_output = pack_line_data_no_aa();
}

}  // namespace edges

PipelineGraphic edge(edges::vert_main, edges::frag_main, ClippingConstant{.use_clipping = false});
PipelineGraphic edge_clipped(edges::vert_main,
                             edges::frag_main,
                             ClippingConstant{.use_clipping = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face
 * \{ */

namespace faces {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] uint4 data;
};

struct VertOut {
  [[flat]] float4 color;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[instance_index]] const int inst_index,
               [[in]] const VertIn &v_in,
               [[out]] VertOut &v_out,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[position]] float4 &out_pos)
{
  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);
  EditVertData edit = srt.unpack_vert_data(v_in.data);

  float3 ws_P = model.point_object_to_world(v_in.pos);
  float3 vs_P = view.point_world_to_view(ws_P);
  out_pos = view.point_view_to_homogenous(vs_P);
  /* Offset Z position for retopology overlay. */
  out_pos.z += get_homogenous_z_offset(view.winmat, vs_P.z, out_pos.w, srt.retopology_offset);
#ifdef GPU_METAL
  /* Apply depth bias to overlay in order to prevent z-fighting on Apple Silicon GPUs. */
  out_pos.z -= 5e-5f;
#endif
  /* NDC offset to avoid Z fighting. */
  out_pos.z -= srt.ndc_offset_factor * srt.ndc_offset;

  v_out.color = srt.face_color(edit);
  v_out.color.a *= srt.alpha;

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

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out)
{
  frag_out.color = v_out.color;
  frag_out.line_output = float4(0.0f);
}

}  // namespace faces

PipelineGraphic face(faces::vert_main, faces::frag_main, ClippingConstant{.use_clipping = false});
PipelineGraphic face_clipped(faces::vert_main,
                             faces::frag_main,
                             ClippingConstant{.use_clipping = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Dot
 * \{ */

namespace face_dot {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] uint4 data;
  [[attribute(2)]] float4 norAndFlag;
};

struct VertOut {
  [[flat]] float4 color;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[instance_index]] const int inst_index,
               [[in]] const VertIn &v_in,
               [[out]] VertOut &v_out,
               [[point_size]] float &out_point_size,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[position]] float4 &out_pos)
{
  const UniformData &theme = srt.uni.uniform_buf;

  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);

  float3 ws_P = model.point_object_to_world(v_in.pos);
  float3 vs_P = view.point_world_to_view(ws_P);
  out_pos = view.point_view_to_homogenous(vs_P);
  /* Offset Z position for retopology overlay. */
  out_pos.z += get_homogenous_z_offset(view.winmat, vs_P.z, out_pos.w, srt.retopology_offset);
  /* Bias Face-dot Z position in clip-space. */
  out_pos.z -= (view.winmat[3][3] == 0.0f) ? 0.00035f : 1e-6f;
  /* NDC offset to avoid Z fighting. */
  out_pos.z -= srt.ndc_offset_factor * srt.ndc_offset;

  v_out.color = srt.facedot_color(v_in.norAndFlag.w);
  /* Occluded geometry fading. */
  v_out.color.a *= srt.test_occlusion(out_pos) ? srt.alpha : 1.0f;

  out_point_size = theme.sizes.face_dot;

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

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out,
               [[point_coord]] const float2 point_co)
{
  float2 centered = point_co - float2(0.5f);
  float dist_squared = dot(centered, centered);
  constexpr float rad_squared = 0.25f;

  /* Round point with jagged edges. */
  if (dist_squared > rad_squared) {
    gpu_discard_fragment();
    return;
  }

  frag_out.color = v_out.color;
  frag_out.line_output = float4(0.0f);
}

}  // namespace face_dot

PipelineGraphic facedot(face_dot::vert_main,
                        face_dot::frag_main,
                        ClippingConstant{.use_clipping = false});
PipelineGraphic facedot_clipped(face_dot::vert_main,
                                face_dot::frag_main,
                                ClippingConstant{.use_clipping = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth Pass
 * \{ */

struct VertIn {
  [[attribute(0)]] float3 pos;
};

[[vertex]]
void depth_pass_vert([[resource_table]] const Resources &srt,
                     [[instance_index]] const int inst_index,
                     [[in]] const VertIn &v_in,
                     [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                     [[position]] float4 &out_pos)
{
  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);

  float3 ws_P = model.point_object_to_world(v_in.pos);
  float3 vs_P = view.point_world_to_view(ws_P);
  out_pos = view.point_view_to_homogenous(vs_P);
  /* Offset Z position for retopology overlay. */
  out_pos.z += get_homogenous_z_offset(view.winmat, vs_P.z, out_pos.w, srt.retopology_offset);

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

[[fragment]]
void depth_pass_frag()
{
  /* NOOP. */
}

PipelineGraphic depth(depth_pass_vert, depth_pass_frag, ClippingConstant{.use_clipping = false});
PipelineGraphic depth_clipped(depth_pass_vert,
                              depth_pass_frag,
                              ClippingConstant{.use_clipping = true});

/** \} */

}  // namespace overlay::edit_mesh
