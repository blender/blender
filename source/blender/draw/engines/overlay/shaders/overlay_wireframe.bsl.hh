/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_gsplat_lib.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_shader_shared.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"
#include "overlay_common.bsl.hh"
#include "select_lib.bsl.hh"

namespace overlay::wireframe {

float3 hsv_to_rgb(float3 hsv)
{
  float3 nrgb = abs(hsv.x * 6.0f - float3(3.0f, 2.0f, 4.0f)) * float3(1, -1, -1) +
                float3(-1, 2, 2);
  nrgb = saturate(nrgb);
  return ((nrgb - 1.0f) * hsv.y + 1.0f) * hsv.z;
}

struct VertOutPoint {
  [[flat]] float4 final_color;
  [[flat]] float4 final_color_inner;
};

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float3 nor;
  [[attribute(2)]] float wd; /* wire-data. */
};

struct VertInPointWithRadius {
  [[attribute(4)]] float4 pos_rad;
};

enum GeomType : int {
  MESH,
  CURVES,
  POINTS,
};

struct WireColor {
  float3 rim, wire;
};

struct Resources {
  [[compilation_constant]] int pass_type;
  [[compilation_constant]] bool use_point_radius;
  [[compilation_constant]] bool use_gsplat;

  [[resource_table]] const Uniform uni;

  [[specialization_constant(true)]] bool use_custom_depth_bias;

  [[push_constant]] float ndc_offset_factor;
  [[push_constant]] float wire_opacity;
  [[push_constant]] bool use_coloring;
  [[push_constant]] bool is_transform;
  [[push_constant]] int color_type;

  [[push_constant, condition(pass_type == GeomType::MESH)]] float wire_step_param;
  /* Scene Depth texture copy for manual depth test. */
  [[sampler(0)]] sampler2DDepth depth_tx;

  bool is_edge_sharpness_visible(float wire_data) const
  {
    if (pass_type == MESH) [[static_branch]] {
      return wire_data <= wire_step_param;
    }
    return true;
  }

  WireColor wire_color_get(eObjectInfoFlag ob_flag) const
  {
    bool is_selected = flag_test(ob_flag, OBJECT_SELECTED);
    bool is_from_set = flag_test(ob_flag, OBJECT_FROM_SET);
    bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);
    const UniformData &theme = uni.uniform_buf;

    WireColor col;
    if (is_from_set) {
      col.rim = theme.colors.wire.rgb;
      col.wire = theme.colors.wire.rgb;
    }
    else if (is_selected && use_coloring) {
      if (is_transform) {
        col.rim = theme.colors.transform.rgb;
      }
      else if (is_active) {
        col.rim = theme.colors.active_object.rgb;
      }
      else {
        col.rim = theme.colors.object_select.rgb;
      }
      col.wire = theme.colors.wire.rgb;
    }
    else {
      col.rim = theme.colors.wire.rgb;
      col.wire = theme.colors.background.rgb;
    }
    return col;
  }

  WireColor wire_object_color_get(ObjectInfos info) const
  {
    const bool is_selected = flag_test(info.flag, OBJECT_SELECTED);

    WireColor col;
    if (color_type == V3D_SHADING_OBJECT_COLOR) {
      col.rim = col.wire = info.ob_color.rgb * 0.5f;
    }
    else {
      float hue = info.random;
      float3 hsv = float3(hue, 0.75f, 0.8f);
      col.rim = col.wire = hsv_to_rgb(hsv);
    }

    if (is_selected && use_coloring) {
      /* "Normalize" color. */
      col.wire += 1e-4f; /* Avoid division by 0. */
      float brightness = max(col.wire.x, max(col.wire.y, col.wire.z));
      col.wire *= 0.5f / brightness;
      col.rim += 0.75f;
    }
    else {
      col.wire += 0.5f;
      col.rim *= 0.5f;
    }
    return col;
  }
};

[[vertex]] void vert_main(
    [[resource_table]] const Resources &srt,
    [[resource_table]] const Uniform &uni,
    [[resource_table]] const Clipping &clip,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table]] const draw::View &views,
    [[resource_table, condition(use_gsplat)]] const draw::gsplat::ShapeResource &shape,
    [[resource_table, condition(!selectable)]] const draw::Resource &resources,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &resources_with_id,
    [[resource_table, condition(selectable)]] const draw::Select &sel,
    [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
    [[instance_index]] const int inst_index,
    [[vertex_id]] const int vert_id,
    [[point_size]] float &pt_size,
    [[in, condition(!use_point_radius)]] const VertIn &v_in,
    [[in, condition(use_point_radius)]] const VertInPointWithRadius &v_point_rad,
    [[out, condition(pass_type != POINTS)]] VertOutSmooth &v_out,
    [[out, condition(pass_type == POINTS)]] VertOutPoint &v_out_point,
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

  const UniformData &theme = uni.uniform_buf;
  ObjectMatrices model = models.get(resource_id);
  ObjectInfos info = infos.get(resource_id);
  ViewMatrices view = views.get(0);

  /* If no attribute is available, use a fixed facing value depending on the coloring mode.
   * This allow to keep most of the contrast between unselected and selected color
   * while keeping object coloring mode working (see #134011). */
  const float no_nor_facing = (srt.color_type == V3D_SHADING_SINGLE_COLOR) ? 0.0f : 0.5f;

  float3 ls_pos;
  if (srt.use_point_radius) [[static_branch]] {
    ls_pos = v_point_rad.pos_rad.xyz;
  }
  else {
    ls_pos = v_in.pos;
  }

  float3 ws_pos = model.point_object_to_world(ls_pos);

  if (srt.use_point_radius) [[static_branch]] {
    ws_pos += view.world_incident_vector(ws_pos) * v_point_rad.pos_rad.w;
  }

  if (srt.pass_type == POINTS) [[static_branch]] {
    pt_size = theme.sizes.vert * 2.0f;
  }

  float3 ws_nor = float3(0.0f);
  float facing = no_nor_facing;
  if (!srt.use_point_radius && srt.pass_type == MESH) [[static_branch]] {
    const float3 V = view.world_incident_vector(ws_pos);
    ws_nor = safe_normalize(model.normal_object_to_world(v_in.nor));
    if (!all(equal(v_in.nor, float3(0)))) {
      facing = dot(ws_nor, V);
    }
  }

  out_pos = view.point_world_to_homogenous(ws_pos);

  if (srt.use_gsplat) [[static_branch]] {
    const draw::gsplat::PointShape gs = shape.get_point(uint(vert_id), model, view);
    ws_pos = gs.wP;
    out_pos = gs.hP;
  }

  if (!srt.use_point_radius && srt.pass_type == MESH) [[static_branch]] {
    if (!srt.use_custom_depth_bias) {
      float facing_ratio = clamp(1.0f - facing * facing, 0.0f, 1.0f);
      float flip = sign(facing); /* Flip when not facing the normal (i.e.: back-facing). */
      float curvature = (1.0f - v_in.wd * 0.75f); /* Avoid making things worse for curvy areas. */
      float3 ws_ofs = ws_nor * (facing_ratio * curvature * flip);
      float3 vs_ofs = view.normal_world_to_view(ws_ofs);

      /* Push vertex half a pixel (maximum) in normal direction. */
      out_pos.xy += vs_ofs.xy * uni.uniform_buf.size_viewport_inv * out_pos.w;

      /* Push the vertex towards the camera. Helps a bit. */
      out_pos.z -= facing_ratio * curvature * 1.0e-6f * out_pos.w;
    }
  }

  /* Curves do not need the offset since they *are* the curve geometry. */
  if (srt.pass_type != CURVES) [[static_branch]] {
    out_pos.z -= srt.ndc_offset_factor * 0.5f;
  }

  WireColor col;
  if (srt.color_type == V3D_SHADING_OBJECT_COLOR || srt.color_type == V3D_SHADING_RANDOM_COLOR) {
    col = srt.wire_object_color_get(info);
  }
  else {
    col = srt.wire_color_get(info.flag);
  }

  if (srt.pass_type == POINTS) [[static_branch]] {
    v_out_point.final_color = float4(col.wire * srt.wire_opacity, srt.wire_opacity);
    v_out_point.final_color_inner = float4(col.rim * srt.wire_opacity, srt.wire_opacity);
  }
  else if (srt.pass_type != POINTS && !srt.use_point_radius) [[static_branch]] {
    /* Convert to screen position [0..sizeVp]. */
    v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                        uni.uniform_buf.size_viewport;

    facing = saturate(abs(facing));
    /* Do interpolation in a non-linear space to have a better visual result. */
    col.rim = pow(col.rim, float3(1.0f / 2.2f));
    col.wire = pow(col.wire, float3(1.0f / 2.2f));
    const float3 final_front_col = mix(col.rim, col.wire, 0.35f);
    v_out.final_color.rgb = mix(col.rim, final_front_col, facing);
    v_out.final_color.rgb = pow(v_out.final_color.rgb, float3(2.2f));

    v_out.final_color.a = srt.wire_opacity;
    v_out.final_color.rgb *= srt.wire_opacity;

    if (srt.pass_type == MESH) [[static_branch]] {
      /* Cull flat edges below threshold. */
      const bool no_attr = all(equal(v_in.nor, float3(0)));
      if (!no_attr && !srt.is_edge_sharpness_visible(v_in.wd)) {
        v_out.edge_start = float2(-1.0f);
      }
    }

    if (sel.consts.selectable) [[static_branch]] {
      /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
       * wire to at least create one fragment that will pass the occlusion query. */
      out_pos.xy += uni.uniform_buf.size_viewport_inv * out_pos.w *
                    ((vert_id % 2 == 0) ? -1.0f : 1.0f);
    }
  }

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

[[fragment]] void frag_mesh([[resource_table]] const Resources &srt,
                            [[resource_table]] const Uniform &uni,
                            [[resource_table, condition(selectable)]] draw::Select &sel,
                            [[frag_coord]] const float4 frag_co,
                            [[frag_depth(any), condition(!selectable)]] float &out_depth,
                            [[in]] const VertOutSmooth &v_out,
                            [[in, condition(selectable)]] const draw::SelectOut &sel_out,
                            [[out]] FragOut &frag_out)
{
  /* Needed only because of wireframe slider.
   * If we could get rid of it would be nice because of performance drain of discard. */
  if (v_out.edge_start.r == -1.0f) {
    gpu_discard_fragment();
    return;
  }

  frag_out.line_output = pack_line_data(frag_co.xy, v_out.edge_start, v_out.edge_pos);
  frag_out.color = v_out.final_color;

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
  else {
    out_depth = frag_co.z;

    if (srt.use_custom_depth_bias) {
      float2 dir;
      float dist;
      unpack_line_data(frag_out.line_output.xy, dir, dist);
      bool dir_horiz = abs(dir.x) > abs(dir.y);

      float2 uv = frag_co.xy * uni.uniform_buf.size_viewport_inv;
      float depth_occluder = texture(srt.depth_tx, uv).r;
      float depth_min = depth_occluder;
      float2 uv_offset = uni.uniform_buf.size_viewport_inv;
      if (dir_horiz) {
        uv_offset.y = 0.0f;
      }
      else {
        uv_offset.x = 0.0f;
      }

      depth_min = min(depth_min, texture(srt.depth_tx, uv - uv_offset).r);
      depth_min = min(depth_min, texture(srt.depth_tx, uv + uv_offset).r);

      float delta = abs(depth_occluder - depth_min);

      if (frag_co.z < (depth_occluder + delta) && frag_co.z > depth_occluder) {
        out_depth = depth_occluder;
      }
    }
  }
}

[[fragment]] void frag_curves([[resource_table, condition(selectable)]] draw::Select &sel,
                              [[frag_coord]] const float4 frag_co,
                              [[in]] const VertOutSmooth &v_out,
                              [[in, condition(selectable)]] const draw::SelectOut &sel_out,
                              [[out]] FragOut &frag_out)
{
  frag_out.line_output = pack_line_data(frag_co.xy, v_out.edge_start, v_out.edge_pos);
  frag_out.color = v_out.final_color;

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

[[fragment]] void frag_point([[resource_table, condition(selectable)]] draw::Select &sel,
                             [[frag_coord]] const float4 frag_co,
                             [[point_coord]] const float2 point_co,
                             [[in]] const VertOutPoint &v_out_point,
                             [[in, condition(selectable)]] const draw::SelectOut &sel_out,
                             [[out]] FragOut &frag_out)
{
  float2 centered = abs(point_co - float2(0.5f));
  float dist = max(centered.x, centered.y);

  float fac = dist * dist * 4.0f;
  /* Create a small gradient so that dense objects have a small fresnel effect. */
  /* Non linear blend. */
  float3 rim_col = sqrt(v_out_point.final_color_inner.rgb);
  float3 wire_col = sqrt(v_out_point.final_color.rgb);
  float3 final_front_col = mix(rim_col, wire_col, 0.35f);
  float3 color = mix(final_front_col, rim_col, saturate(fac));
  frag_out.color = float4(color * color, v_out_point.final_color.a);
  frag_out.line_output = float4(0.0f);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

/* clang-format off */
PipelineGraphic mesh(                   vert_main, frag_mesh, Resources{.pass_type = 0 /*MESH*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic mesh_clipped(           vert_main, frag_mesh, Resources{.pass_type = 0 /*MESH*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic mesh_selectable(        vert_main, frag_mesh, Resources{.pass_type = 0 /*MESH*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic mesh_selectable_clipped(vert_main, frag_mesh, Resources{.pass_type = 0 /*MESH*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic curve(                   vert_main, frag_curves, Resources{.pass_type = 1 /*CURVES*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic curve_clipped(           vert_main, frag_curves, Resources{.pass_type = 1 /*CURVES*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic curve_selectable(        vert_main, frag_curves, Resources{.pass_type = 1 /*CURVES*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic curve_selectable_clipped(vert_main, frag_curves, Resources{.pass_type = 1 /*CURVES*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic points(                   vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic points_clipped(           vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic points_selectable(        vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic points_selectable_clipped(vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic points_with_radius(                   vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = true, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic points_with_radius_clipped(           vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = true, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic points_with_radius_selectable(        vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = true, .use_gsplat = false}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic points_with_radius_selectable_clipped(vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = true, .use_gsplat = false}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});

PipelineGraphic gsplat(                   vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = true}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic gsplat_clipped(           vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = true}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic gsplat_selectable(        vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = true}, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic gsplat_selectable_clipped(vert_main, frag_point, Resources{.pass_type = 2 /*POINTS*/, .use_point_radius = false, .use_gsplat = true}, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});
/* clang-format on */

}  // namespace overlay::wireframe
