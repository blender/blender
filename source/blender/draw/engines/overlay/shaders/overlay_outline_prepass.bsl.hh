/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_curves.bsl.hh"
#include "draw_grease_pencil.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_pointcloud.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"
#include "overlay_common.bsl.hh"
#include "overlay_shader_shared.hh"

/* TODO(fclem): Move it to Gpencil lib. */
float3 ray_plane_intersection(float3 ray_ori, float3 ray_dir, float4 plane)
{
  float d = dot(plane.xyz, ray_dir);
  float3 plane_co = plane.xyz * (-plane.w / dot(plane.xyz, plane.xyz));
  float3 h = ray_ori - plane_co;
  float lambda = -dot(plane.xyz, h) / ((abs(d) < 1e-8f) ? 1e-8f : d);
  return ray_ori + ray_dir * lambda;
}

namespace overlay::outline {

struct VertOut {
  [[flat]] uint ob_id;
};

struct FragOut {
  /* Using uint because 16bit uint can contain more ids than int. */
  [[frag_color(0)]] uint object_id;
};

enum PrepassType : int {
  MESH = 0,
  CURVES = 1,
  POINTCLOUD = 2,
  GREASE_PENCIL = 3,
  WIRE = 4,
};

struct Prepass {
  [[compilation_constant]] const int object_type;

  /* Global state. True during modal operator. */
  [[push_constant]] const bool is_transform;

  uint outline_colorid_get(eObjectInfoFlag ob_flag) const
  {
    bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);

    if (is_transform) {
      return 0u; /* theme.colors.transform */
    }
    if (is_active) {
      return 3u; /* theme.colors.active */
    }
    return 1u; /* theme.colors.object_select */
  }
};

struct VertIn {
  [[attribute(0)]] float3 pos;
};

[[vertex]]
void vert_main([[resource_table]] const Prepass &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table]] const draw::Resource &resources,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[in, condition(object_type == 0 /*MESH*/)]] const VertIn &v_in,
               [[resource_table,
                 condition(object_type == 1 /*CURVES*/)]] const draw::Curves &curves,
               [[resource_table,
                 condition(object_type == 2 /*POINTCLOUD*/)]] const draw::PointCloud &pt_cloud,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] VertOut &v_out,
               [[position]] float4 &out_pos)
{
  const draw::ID id = resources.get(inst_index);
  const uint resource_id = id.resource_id<1>();
  const ViewMatrices view = views.get(0);
  const ObjectMatrices obj = models.get(resource_id);
  const eObjectInfoFlag ob_flag = infos.get(resource_id).flag;

  float3 world_pos;
  if (srt.object_type == 0 /*MESH*/) [[static_branch]] {
    world_pos = obj.point_object_to_world(v_in.pos);
  }
  else if (srt.object_type == 1 /*CURVES*/) [[static_branch]] {
    const draw::curves::Point ls_pt = curves.point_get(uint(vert_id));
    draw::curves::Point ws_pt = curves.object_to_world(ls_pt, obj.model);

    const float min_width_px = 1.25f;
    const float min_width_ws = view.point_world_to_homogenous(ws_pt.P).w * min_width_px *
                               uni.uniform_buf.size_viewport_inv.y * view.wininv[1][1];
    /* Make sure that the ribbon and cylinder topology span at least a small amount of pixel to
     * avoid aliasing artifacts. */
    ws_pt.radius = max(ws_pt.radius, min_width_ws);

    const draw::curves::ShapePoint pt = curves.shape_point_get(
        ws_pt, view.world_incident_vector(ws_pt.P));
    world_pos = pt.P;
  }
  else if (srt.object_type == 2 /*POINTCLOUD*/) [[static_branch]] {
    const draw::pointcloud::Point ls_pt = pt_cloud.point_get(uint(vert_id));
    const draw::pointcloud::Point ws_pt = draw::pointcloud::object_to_world(ls_pt, obj.model);
    const draw::pointcloud::ShapePoint pt = draw::pointcloud::shape_point_get(
        ws_pt, view.world_incident_vector(ws_pt.P), view.up(), ob_flag);

    world_pos = pt.P;
  }

  out_pos = view.point_world_to_homogenous(world_pos);
  /* Small bias to always be on top of the geom. */
  out_pos.z -= 1e-3f;

  /* ID 0 is nothing (background). */
  const uint ob_id = resource_id + 1;
  /* Should be 2 bits only [0..3]. */
  const uint outline_id = srt.outline_colorid_get(ob_flag);
  /* Combine for 16bit uint target. */
  v_out.ob_id = outline_id_pack(outline_id, ob_id);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

[[fragment]]
void frag_main([[in]] const VertOut &v_out, [[out]] FragOut &frag_out)
{
  frag_out.object_id = v_out.ob_id;
}

namespace gpencil {

struct VertOut {
  [[flat]] float4 aspect;
  [[flat]] float2 sspos_0;
  [[flat]] float4 sspos_1;
  [[flat]] float4 sspos_2;
  [[flat]] float2 sspos_3;
  [[flat]] float3 point_length;
  [[no_perspective]] float4 thickness;
  [[no_perspective]] float hardness;
};

struct Resources {
  /* TODO(fclem): Move to a GPencil object UBO. */
  [[push_constant]] bool gp_stroke_order3d;
  [[push_constant]] float4 gp_depth_plane;
};

[[vertex]]
void vert_main([[resource_table]] const Prepass &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table]] const draw::Resource &resources,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const draw::GreasePencil &gpencil,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] outline::VertOut &v_out,
               [[out]] gpencil::VertOut &v_gpencil,
               [[position]] float4 &out_pos)
{
  const draw::ID id = resources.get(inst_index);
  const uint resource_id = id.resource_id<1>();
  const ViewMatrices view = views.get(0);
  const ObjectMatrices obj = models.get(resource_id);
  const ObjectInfos info = infos.get(resource_id);

  float3 world_pos;
  float3 unused_N;
  float4 unused_color;
  float unused_strength;
  float2 unused_uv;

  out_pos = gpencil.process_vertex(
      vert_id,
      obj,
      info,
      view,
      float4(uni.uniform_buf.size_viewport, uni.uniform_buf.size_viewport_inv),
      world_pos,
      unused_N,
      unused_color,
      unused_strength,
      unused_uv,
      v_gpencil.sspos_0,
      v_gpencil.sspos_1,
      v_gpencil.sspos_2,
      v_gpencil.sspos_3,
      v_gpencil.point_length,
      v_gpencil.aspect,
      v_gpencil.thickness,
      v_gpencil.hardness);

  /* Small bias to always be on top of the geom. */
  out_pos.z -= 1e-3f;

  /* ID 0 is nothing (background). */
  const uint ob_id = resource_id + 1;
  /* Should be 2 bits only [0..3]. */
  const uint outline_id = srt.outline_colorid_get(info.flag);
  /* Combine for 16bit uint target. */
  v_out.ob_id = outline_id_pack(outline_id, ob_id);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

[[fragment]]
void frag_main([[resource_table]] const Uniform &uni,
               [[resource_table]] const Resources &srt,
               [[resource_table]] const draw::View &views,
               [[in]] const outline::VertOut &v_out,
               [[in]] const gpencil::VertOut &v_gpencil,
               [[frag_coord]] float4 frag_co,
               [[frag_depth(any)]] float &out_depth,
               [[out]] FragOut &frag_out)
{
  if (draw::GreasePencil::stroke_segment_mask(frag_co.xy,
                                              v_gpencil.sspos_1.xy,
                                              v_gpencil.sspos_2.xy,
                                              v_gpencil.sspos_0,
                                              v_gpencil.sspos_3,
                                              v_gpencil.thickness.x,
                                              v_gpencil.hardness,
                                              v_gpencil.thickness.zw) < 0.001f)
  {
    gpu_discard_fragment();
    return;
  }

  const ViewMatrices view = views.get(0);

  if (!srt.gp_stroke_order3d) {
    /* Stroke order 2D. Project to gp_depth_plane. */
    float2 uvs = float2(frag_co.xy) * uni.uniform_buf.size_viewport_inv;
    float3 pos = view.point_screen_to_world(float3(uvs, frag_co.z));

    float3 ray_ori = pos;
    float3 ray_dir = view.world_incident_vector(pos);
    float3 isect = ray_plane_intersection(ray_ori, ray_dir, srt.gp_depth_plane);
    float4 ndc = view.point_world_to_homogenous(isect);
    out_depth = (ndc.z / ndc.w) * 0.5f + 0.5f;
  }
  else {
    out_depth = frag_co.z;
  }
  frag_out.object_id = v_out.ob_id;
}

}  // namespace gpencil

namespace wire {

struct Wire {
  [[resource_table]] Prepass srt;
  [[resource_table]] draw::Resource resources;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Infos infos;
  [[resource_table]] draw::View views;
  [[resource_table]] Clipping clip;

  [[resource_table]] IndexLoad indices;
  [[storage(0, read), frequency(GEOMETRY)]] const float (&pos)[];
  [[push_constant]] const int2 gpu_attr_0;

  struct VertIn {
    float3 ls_P;
  };

  VertIn input_assembly(uint in_vertex_id) const
  {
    uint v_i = indices.load(in_vertex_id);
    float3 P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
    return {.ls_P = P};
  }

  struct VertOut {
    float3 ws_P;
    float4 hs_P;
    float3 vs_P;
    uint ob_id;
  };

  VertOut vertex_main(VertIn v_in, int inst_index) const
  {
    const draw::ID id = resources.get(inst_index);
    const uint resource_id = id.resource_id<1>();
    const ViewMatrices view = views.get(0);
    const ObjectMatrices obj = models.get(resource_id);
    const eObjectInfoFlag ob_flag = infos.get(resource_id).flag;

    VertOut vert_out;
    vert_out.ws_P = obj.point_object_to_world(v_in.ls_P);
    vert_out.hs_P = view.point_world_to_homogenous(vert_out.ws_P);
    vert_out.vs_P = view.point_world_to_view(vert_out.ws_P);
    /* Small bias to always be on top of the geom. */
    vert_out.hs_P.z -= 1e-3f;

    /* ID 0 is nothing (background). */
    const uint ob_id = resource_id + 1;
    /* Should be 2 bits only [0..3]. */
    const uint outline_id = srt.outline_colorid_get(ob_flag);
    /* Combine for 16bit uint target. */
    vert_out.ob_id = outline_id_pack(outline_id, ob_id);

    return vert_out;
  }

  void geometry_main(VertOut geom_in[4],
                     uint out_vertex_id,
                     uint /*out_primitive_id*/,
                     uint /*out_invocation_id*/,
                     outline::VertOut &v_out,
                     float4 &out_pos,
                     float3 &out_world_pos) const
  {
    const ViewMatrices view = views.get(0);
    float3 view_vec = -view.view_incident_vector(geom_in[1].vs_P);

    float3 v10 = geom_in[0].vs_P - geom_in[1].vs_P;
    float3 v12 = geom_in[2].vs_P - geom_in[1].vs_P;
    float3 v13 = geom_in[3].vs_P - geom_in[1].vs_P;

    /* Known Issue: This also generates outlines for connected-overlapping edges, since their
     * vector is zero-length. */
    float3 n0 = cross(v12, v10);
    n0 *= safe_rcp(length(n0));
    float3 n3 = cross(v13, v12);
    n3 *= safe_rcp(length(n3));

    float fac0 = dot(view_vec, n0);
    float fac3 = dot(view_vec, n3);

    /* If one of the face is perpendicular to the view,
     * consider it an outline edge. */
    if (abs(fac0) > 1e-5f && abs(fac3) > 1e-5f) {
      /* If both adjacent verts are facing the camera the same way,
       * then it isn't an outline edge. */
      if (sign(fac0) == sign(fac3)) {
        return;
      }
    }

    VertOut export_vert = (out_vertex_id == 0) ? geom_in[1] : geom_in[2];

    out_pos = export_vert.hs_P;
    v_out.ob_id = export_vert.ob_id;
    out_world_pos = export_vert.ws_P;
  }
};

[[vertex]]
void vert_main([[resource_table]] const Wire &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table]] const draw::Resource &resources,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] VertOut &v_out,
               [[position]] float4 &out_pos)
{
  /* Line adjacency list primitive. */
  constexpr uint input_primitive_vertex_count = 4u;
  /* Line list primitive. */
  constexpr uint output_primitive_vertex_count = 2u;
  constexpr uint output_primitive_count = 1u;
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

  Wire::VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u);
  vert_in[2] = srt.input_assembly(in_primitive_first_vertex + 2u);
  vert_in[3] = srt.input_assembly(in_primitive_first_vertex + 3u);

  Wire::VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = srt.vertex_main(vert_in[0], inst_index);
  vert_out[1] = srt.vertex_main(vert_in[1], inst_index);
  vert_out[2] = srt.vertex_main(vert_in[2], inst_index);
  vert_out[3] = srt.vertex_main(vert_in[3], inst_index);

  float3 world_pos;
  /* Discard by default. */
  out_pos = float4(NAN_FLT);
  srt.geometry_main(
      vert_out, out_vertex_id, out_primitive_id, out_invocation_id, v_out, out_pos, world_pos);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace wire

/* clang-format off */
PipelineGraphic prepass_mesh(      vert_main, frag_main, ClippingConstant{.use_clipping = false}, Prepass{.object_type = 0 /*PrepassType::MESH */});
PipelineGraphic prepass_curves(    vert_main, frag_main, ClippingConstant{.use_clipping = false}, Prepass{.object_type = 1 /*PrepassType::CURVES */});
PipelineGraphic prepass_pointcloud(vert_main, frag_main, ClippingConstant{.use_clipping = false}, Prepass{.object_type = 2 /*PrepassType::POINTCLOUD */});

PipelineGraphic prepass_mesh_clipped(      vert_main, frag_main, ClippingConstant{.use_clipping = true}, Prepass{.object_type = 0 /*PrepassType::MESH */});
PipelineGraphic prepass_curves_clipped(    vert_main, frag_main, ClippingConstant{.use_clipping = true}, Prepass{.object_type = 1 /*PrepassType::CURVES */});
PipelineGraphic prepass_pointcloud_clipped(vert_main, frag_main, ClippingConstant{.use_clipping = true}, Prepass{.object_type = 2 /*PrepassType::POINTCLOUD */});

PipelineGraphic prepass_gpencil(        gpencil::vert_main, gpencil::frag_main, ClippingConstant{.use_clipping = false});
PipelineGraphic prepass_gpencil_clipped(gpencil::vert_main, gpencil::frag_main, ClippingConstant{.use_clipping = false});

PipelineGraphic prepass_wire(        wire::vert_main, frag_main, ClippingConstant{.use_clipping = false});
PipelineGraphic prepass_wire_clipped(wire::vert_main, frag_main, ClippingConstant{.use_clipping = false});
/* clang-format on */

}  // namespace overlay::outline
