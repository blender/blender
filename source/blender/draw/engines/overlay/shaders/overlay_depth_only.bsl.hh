/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Used to occlude edit geometry which might not be rendered by the render engine.
 * This can also be used for rendering a depth prepass when the render engine doesn't provide one.
 * Lastly, this is used by the selection pipeline for selecting visible geometry. */

#pragma once

#include "draw_curves.bsl.hh"
#include "draw_grease_pencil.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_pointcloud.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "overlay_common.bsl.hh"
#include "select_lib.bsl.hh"

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  define const
#endif

namespace overlay::depth {

struct VertIn {
  [[attribute(0)]] float3 pos;
};

enum GeometryType : int {
  MESH = 0,
  CURVES = 1,
  POINTCLOUD = 2,
  GREASE_PENCIL = 3,
};

struct Resources {
  [[compilation_constant]] const int object_type;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table, condition(!selectable)]] draw::Resource &resources,
               [[resource_table, condition(selectable)]] draw::ResourceCustomID &resources_with_id,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[in, condition(object_type == MESH)]] const VertIn &v_in,
               [[resource_table, condition(object_type == CURVES)]] draw::Curves &curves,
               [[resource_table, condition(object_type == POINTCLOUD)]] draw::PointCloud &pt_cloud,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[position]] float4 &out_pos)
{
  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  const ObjectMatrices obj = models.get(resource_id);
  const ObjectInfos info = infos.get(resource_id);
  const ViewMatrices view = views.get(0);

  float3 world_pos;
  if (srt.object_type == MESH) [[static_branch]] {
    world_pos = obj.point_object_to_world(v_in.pos);
  }
  else if (srt.object_type == CURVES) [[static_branch]] {
    const draw::curves::Point ls_pt = curves.point_get(uint(vert_id));
    draw::curves::Point ws_pt = curves.object_to_world(ls_pt, obj.model);

    const draw::curves::ShapePoint pt = curves.shape_point_get(
        ws_pt, view.world_incident_vector(ws_pt.P));
    world_pos = pt.P;
  }
  else if (srt.object_type == POINTCLOUD) [[static_branch]] {
    const draw::pointcloud::Point ls_pt = pt_cloud.point_get(uint(vert_id));
    const draw::pointcloud::Point ws_pt = draw::pointcloud::object_to_world(ls_pt, obj.model);
    const draw::pointcloud::ShapePoint pt = draw::pointcloud::shape_point_get(
        ws_pt, view.world_incident_vector(ws_pt.P), view.up(), info.flag);

    world_pos = pt.P;
  }

  out_pos = view.point_world_to_homogenous(world_pos);

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
void frag_main([[resource_table, condition(selectable)]] const Uniform &uni,
               [[resource_table, condition(selectable)]] draw::Select &sel,
               [[in, condition(selectable)]] const draw::SelectOut &sel_out,
               [[frag_coord, condition(selectable)]] const float4 frag_co,
               [[front_facing, condition(selectable)]] const bool front_face)
{
  /* No color output, only depth (line below is implicit). */
  // gl_FragDepth = gl_FragCoord.z;

  if (sel.consts.selectable) [[static_branch]] {
    if (uni.uniform_buf.backface_culling && !front_face) {
      /* Return instead of discard since we might be using early depth testing. */
      return;
    }
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

/* Modify triangle vertices position by half a pixel to (hopefully) make it cover at least one
 * pixel if the triangle is subpixel sized. This is important to not loose small objects during
 * selection. */
namespace conservative {

struct VertIn {
  float3 ls_P;
};

struct Resources {
  [[push_constant]] int2 gpu_attr_0;
  [[storage(0, read), frequency(GEOMETRY)]] float (&pos)[];
  [[storage(1, read)]] float4x4 (&data_buf)[];

  [[resource_table]] Uniform uni;
  [[resource_table]] IndexLoad indices;

  VertIn input_assembly(uint in_vertex_id) const
  {
    uint v_i = indices.load(in_vertex_id);
    float3 ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
    return {.ls_P = ls_P};
  }

  struct VertOut {
    float4 hs_P;
    float3 ws_P;
  };

  VertOut vertex_main(const ViewMatrices view, const ObjectMatrices obj, VertIn v_in) const
  {
    VertOut v_out;
    v_out.ws_P = obj.point_object_to_world(v_in.ls_P);
    v_out.hs_P = view.point_world_to_homogenous(v_out.ws_P);
    return v_out;
  }

  void do_vertex(uint i,
                 uint out_vertex_id,
                 uint /*out_primitive_id*/,
                 const ObjectMatrices obj,
                 VertOut geom_in,
                 bool2 is_subpixel,
                 bool is_coplanar,
                 float4 &out_pos,
                 float3 &out_ws_pos) const
  {
    if (out_vertex_id != i) {
      return;
    }

    out_ws_pos = geom_in.ws_P;

    /* WORKAROUND: The subpixel hack that does the small triangle expansion needs to have correct
     * winding w.r.t. the culling mode. Otherwise, the fragment shader will discard valid triangles
     * and objects will become unselectable (see #85015). */
    if ((any(is_subpixel) || is_coplanar) && is_negative(obj.model)) {
      if (i == 1) {
        i = 2;
      }
      else if (i == 2) {
        i = 1;
      }
    }

    out_pos = geom_in.hs_P;
    if (all(is_subpixel)) {
      float2 ofs = (i == 0) ? float2(-1.0f) :
                              ((i == 1) ? float2(2.0f, -1.0f) : float2(-1.0f, 2.0f));
      /* HACK: Fix cases where the triangle is too small make it cover at least one pixel. */
      out_pos.xy += uni.uniform_buf.size_viewport_inv * geom_in.hs_P.w * ofs;
    }
    /* Test if the triangle is almost parallel with the view to avoid precision issues. */
    else if (any(is_subpixel) || is_coplanar) {
      /* HACK: Fix cases where the triangle is Parallel to the view by deforming it slightly. */
      float2 ofs = (i == 0) ? float2(-1.0f) : ((i == 1) ? float2(1.0f, -1.0f) : float2(1.0f));
      out_pos.xy += uni.uniform_buf.size_viewport_inv * geom_in.hs_P.w * ofs;
    }
    else {
      /* Triangle expansion should happen here, but we decide to not implement it for
       * depth precision & performance reasons. */
    }
  }

  void geometry_main(VertOut geom_in[3],
                     uint out_vertex_id,
                     uint out_primitive_id,
                     uint /*out_invocation_id*/,
                     const ObjectMatrices obj,
                     float4 &out_pos,
                     float3 &out_ws_pos) const
  {
    /* Compute plane normal in NDC space. */
    float3 pos0 = geom_in[0].hs_P.xyz / geom_in[0].hs_P.w;
    float3 pos1 = geom_in[1].hs_P.xyz / geom_in[1].hs_P.w;
    float3 pos2 = geom_in[2].hs_P.xyz / geom_in[2].hs_P.w;
    float3 plane = normalize(cross(pos1 - pos0, pos2 - pos0));
    /* Compute NDC bound box. */
    float4 bbox = float4(min(min(pos0.xy, pos1.xy), pos2.xy), max(max(pos0.xy, pos1.xy), pos2.xy));
    /* Convert to pixel space. */
    bbox = (bbox * 0.5f + 0.5f) * uni.uniform_buf.size_viewport.xyxy;
    /* Detect failure cases where triangles would produce no fragments. */
    bool2 is_subpixel = lessThan(bbox.zw - bbox.xy, float2(1.0f));
    /* View aligned triangle. */
    constexpr float threshold = 0.00001f;
    bool is_coplanar = abs(plane.z) < threshold;

    do_vertex(0,
              out_vertex_id,
              out_primitive_id,
              obj,
              geom_in[0],
              is_subpixel,
              is_coplanar,
              out_pos,
              out_ws_pos);
    do_vertex(1,
              out_vertex_id,
              out_primitive_id,
              obj,
              geom_in[1],
              is_subpixel,
              is_coplanar,
              out_pos,
              out_ws_pos);
    do_vertex(2,
              out_vertex_id,
              out_primitive_id,
              obj,
              geom_in[2],
              is_subpixel,
              is_coplanar,
              out_pos,
              out_ws_pos);
  }
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table, condition(!selectable)]] draw::Resource &resources,
               [[resource_table, condition(selectable)]] draw::ResourceCustomID &resources_with_id,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Clipping &clip,
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[vertex_id]] const int vert_id,
               [[instance_index]] const int inst_index,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] overlay::VertOut &v_out,
               [[position]] float4 &out_pos)
{
  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  /* Triangle list primitive. */
  constexpr uint input_primitive_vertex_count = 3u;
  /* Triangle list primitive. */
  constexpr uint output_primitive_vertex_count = 3u;
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

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u);
  vert_in[2] = srt.input_assembly(in_primitive_first_vertex + 2u);

  const ObjectMatrices obj = models.get(resource_id);
  const ViewMatrices view = views.get(0);

  Resources::VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = srt.vertex_main(view, obj, vert_in[0]);
  vert_out[1] = srt.vertex_main(view, obj, vert_in[1]);
  vert_out[2] = srt.vertex_main(view, obj, vert_in[2]);

  /* Discard by default. */
  float3 ws_pos = float3(NAN_FLT);
  out_pos = float4(NAN_FLT);
  srt.geometry_main(
      vert_out, out_vertex_id, out_primitive_id, out_invocation_id, obj, out_pos, ws_pos);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(ws_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace conservative

namespace gpencil {

/* TODO(fclem): Deduplicate. */
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
void vert_main([[resource_table]] const Uniform &uni,
               [[resource_table, condition(!selectable)]] draw::Resource &resources,
               [[resource_table, condition(selectable)]] draw::ResourceCustomID &resources_with_id,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const draw::GreasePencil &gpencil,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] gpencil::VertOut &v_gpencil,
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[position]] float4 &out_pos)
{
  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  const ObjectMatrices obj = models.get(resource_id);
  const ObjectInfos info = infos.get(resource_id);
  const ViewMatrices view = views.get(0);

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

  if (!sel.consts.selectable) [[static_branch]] {
    /* Small bias to always be on top of the geom.
     * Skip during selection so GP does not unfairly win depth picking against
     * nearby objects (especially in orthographic view). See: #156983. */
    out_pos.z -= 1e-3f;
  }

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
void frag_main([[resource_table]] const Resources &srt,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Uniform &uni,
               [[resource_table, condition(selectable)]] draw::Select &sel,
               [[in, condition(selectable)]] const draw::SelectOut &sel_out,
               [[frag_coord]] const float4 frag_co,
               [[in]] const gpencil::VertOut &v_gpencil,
               [[frag_depth(any), condition(!selectable)]] float &out_depth)
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
    if (!sel.consts.selectable) [[static_branch]] {
      /* We cannot discard the fragment in selection mode. Otherwise we would break pipeline
       * correctness (no discard if early depth test enforced). */
      gpu_discard_fragment();
    }
    return;
  }

  /* We cannot change the fragment's depth in selection mode. Otherwise we would break pipeline
   * correctness when early depth test enforced. */
  if (!sel.consts.selectable) [[static_branch]] {
    const ViewMatrices view = views.get(0);

    if (!srt.gp_stroke_order3d) {
      /* Stroke order 2D. Project to gp_depth_plane. */
      float2 uvs = float2(frag_co.xy) * uni.uniform_buf.size_viewport_inv;
      float3 pos = view.point_screen_to_world(float3(uvs, frag_co.z));

      float3 ray_ori = pos;
      float3 ray_dir = view.world_incident_vector(pos);
      float3 isect = draw::GreasePencil::ray_plane_intersection(
          ray_ori, ray_dir, srt.gp_depth_plane);
      float4 ndc = view.point_world_to_homogenous(isect);
      out_depth = (ndc.z / ndc.w) * 0.5f + 0.5f;
    }
    else {
      out_depth = frag_co.z;
    }
  }

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}
}  // namespace gpencil

/* clang-format off */
PipelineGraphic mesh(                   vert_main, frag_main, Resources{.object_type = MESH}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic mesh_clipped(           vert_main, frag_main, Resources{.object_type = MESH}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic mesh_selectable(        vert_main, frag_main, Resources{.object_type = MESH}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic mesh_selectable_clipped(vert_main, frag_main, Resources{.object_type = MESH}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic mesh_conservative(                   conservative::vert_main, frag_main, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic mesh_conservative_clipped(           conservative::vert_main, frag_main, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic mesh_conservative_selectable(        conservative::vert_main, frag_main, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic mesh_conservative_selectable_clipped(conservative::vert_main, frag_main, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic pointcloud(                   vert_main, frag_main, Resources{.object_type = POINTCLOUD}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic pointcloud_clipped(           vert_main, frag_main, Resources{.object_type = POINTCLOUD}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic pointcloud_selectable(        vert_main, frag_main, Resources{.object_type = POINTCLOUD}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic pointcloud_selectable_clipped(vert_main, frag_main, Resources{.object_type = POINTCLOUD}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic curves(                   vert_main, frag_main, Resources{.object_type = CURVES}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic curves_clipped(           vert_main, frag_main, Resources{.object_type = CURVES}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic curves_selectable(        vert_main, frag_main, Resources{.object_type = CURVES}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic curves_selectable_clipped(vert_main, frag_main, Resources{.object_type = CURVES}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic grease_pencil(                   gpencil::vert_main, gpencil::frag_main, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic grease_pencil_clipped(           gpencil::vert_main, gpencil::frag_main, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic grease_pencil_selectable(        gpencil::vert_main, gpencil::frag_main, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic grease_pencil_selectable_clipped(gpencil::vert_main, gpencil::frag_main, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});
/* clang-format on */

}  // namespace overlay::depth

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  undef const
#endif
