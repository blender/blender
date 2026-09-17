/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Overlay shaders for all gsplat-related overlay draws: viewer attribute, outline prepass,
 * depth-only, edit vertices, and wireframe.
 *
 * \note Many of the fragment shaders here are duplicates or partial reimplementations of glsl
 *       shaders. These can be deduplicated during the BSL-porting of the overlay shaders.
 */

#pragma once

#include "draw_view_infos.hh"  // IWYU pragma: export

#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "gpu_shader_math_vector.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

#include "draw_gsplat_lib.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_pointcloud_lib.glsl"
#include "draw_view.bsl.hh"
#include "draw_view_clipping_lib.glsl"

#include "select_lib.bsl.hh"

#include "overlay_common_lib.glsl"
#include "overlay_shader_shared.hh"

namespace overlay {

struct PipelineResources {
  [[legacy_info]] ShaderCreateInfo drw_clipped;

  /** WORKAROUND: This exact compilation constant is checked in Metal backend to enable clip
   * distances. */
  [[compilation_constant]] bool use_clipping;
  [[compilation_constant]] bool selectable;
};

namespace viewer_attribute {

struct Resources {
  [[uniform(OVERLAY_GLOBALS_SLOT)]] const UniformData &uniform_buf;
  [[sampler(3)]] samplerBuffer attribute_tx;
  [[push_constant]] const float opacity;
};
struct VertOut {
  [[smooth]] float4 final_color;
};
struct FragOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 line;
};

[[vertex]] void vert([[resource_table]] const PipelineResources &pipe,
                     [[resource_table]] const Resources &srt,
                     [[resource_table]] const draw::gsplat::ShapeResource &shape,
                     [[resource_table]] const draw::Model &models,
                     [[resource_table]] const draw::Infos & /* infos */,
                     [[resource_table]] const draw::View &views,
                     [[resource_table]] const draw::Resource &res_id,
                     [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
                     [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
                     [[vertex_id]] const int vert_id,
                     [[instance_index]] const int inst_index,
                     [[position]] float4 &out_position,
                     [[point_size]] float &out_pointsize,
                     [[clip_distance,
                       condition(use_clipping)]] float (&)[6], /* Used by `view_clipping_lib`. */
                     [[out]] VertOut &v_out)
{
  draw::ID id = res_id.get(inst_index);
  uint view_id = 0;
  uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(view_id);
  const ObjectMatrices obj = models.get(resource_id);

  const draw::gsplat::PointShape gs = shape.get_point(uint(vert_id), obj, view);
  if (gs.is_culled()) {
    out_position = float4(NAN_FLT);
    return;
  }

  out_position = gs.hP;
  out_pointsize = srt.uniform_buf.sizes.vert * 2.0f;
  v_out.final_color = pointcloud::get_customdata_vec4(int(gs.id), srt.attribute_tx);

  if (pipe.use_clipping) [[static_branch]] {
    view_clipping_distances(gs.wP);
  }
}

[[fragment]] void frag([[resource_table]] const Resources &srt,
                       [[in]] const VertOut &v_in,
                       [[out]] FragOut &f_out)
{
  f_out.color = v_in.final_color * srt.opacity;
  f_out.line = float4(0.0f);
}

}  // namespace viewer_attribute

namespace wireframe {

struct Resources {
  [[uniform(OVERLAY_GLOBALS_SLOT)]] const UniformData &uniform_buf;

  [[push_constant]] const float ndc_offset_factor;
  [[push_constant]] const float wire_opacity;
  [[push_constant]] const int color_type;
  [[push_constant]] const bool use_coloring;
  [[push_constant]] const bool is_transform;
};
struct VertOut {
  [[flat]] uint select_id;
  [[flat]] float4 final_color;
  [[flat]] float4 final_color_inner;
};
struct FragOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 line;
};

namespace detail {

float3 hsv_to_rgb(float3 hsv)
{
  float3 nrgb = abs(hsv.x * 6.0f - float3(3.0f, 2.0f, 4.0f)) * float3(1, -1, -1) +
                float3(-1, 2, 2);
  nrgb = clamp(nrgb, 0.0f, 1.0f);
  return ((nrgb - 1.0f) * hsv.y + 1.0f) * hsv.z;
}

void wire_color_get(float3 &rim_col,
                    float3 &wire_col,
                    const ObjectInfos &ob_infos,
                    [[resource_table]] const Resources &srt)
{
  bool is_selected = flag_test(ob_infos.flag, OBJECT_SELECTED);
  bool is_from_set = flag_test(ob_infos.flag, OBJECT_FROM_SET);
  bool is_active = flag_test(ob_infos.flag, OBJECT_ACTIVE);

  if (is_from_set) {
    rim_col = srt.uniform_buf.colors.wire.rgb;
    wire_col = srt.uniform_buf.colors.wire.rgb;
  }
  else if (is_selected && srt.use_coloring) {
    if (srt.is_transform) {
      rim_col = srt.uniform_buf.colors.transform.rgb;
    }
    else if (is_active) {
      rim_col = srt.uniform_buf.colors.active_object.rgb;
    }
    else {
      rim_col = srt.uniform_buf.colors.object_select.rgb;
    }
    wire_col = srt.uniform_buf.colors.wire.rgb;
  }
  else {
    rim_col = srt.uniform_buf.colors.wire.rgb;
    wire_col = srt.uniform_buf.colors.background.rgb;
  }
}

void wire_object_color_get(float3 &rim_col,
                           float3 &wire_col,
                           const ObjectInfos &ob_infos,
                           [[resource_table]] const Resources &srt)
{
  if (srt.color_type == V3D_SHADING_OBJECT_COLOR) {
    rim_col = wire_col = ob_infos.ob_color.rgb * 0.5f;
  }
  else {
    float3 hsv = float3(ob_infos.random, 0.75f, 0.8f);
    rim_col = wire_col = hsv_to_rgb(hsv);
  }

  if (flag_test(ob_infos.flag, OBJECT_SELECTED) && srt.use_coloring) {
    wire_col += 1e-4f; /* Avoid division by 0. */
    float brightness = max(wire_col.x, max(wire_col.y, wire_col.z));
    wire_col *= 0.5f / brightness;
    rim_col += 0.75f;
  }
  else {
    rim_col *= 0.5f;
    wire_col += 0.5f;
  }
}

}  // namespace detail

[[vertex]] void vert(
    [[resource_table]] const PipelineResources &pipe,
    [[resource_table]] const Resources &srt,
    [[resource_table]] const draw::gsplat::ShapeResource &shape,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table]] const draw::View &views,
    [[resource_table, condition(selectable)]] const draw::Select & /* select */,
    [[resource_table, condition(selectable == 0)]] const draw::Resource &res_id,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &custom_id,
    [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
    [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
    [[vertex_id]] const int vert_id,
    [[instance_index]] const int inst_index,
    [[position]] float4 &out_position,
    [[point_size]] float &out_pointsize,
    [[clip_distance, condition(use_clipping)]] float (&)[6], /* Used by `view_clipping_lib`. */
    [[out]] VertOut &v_out)
{
  draw::ID id;
  if (pipe.selectable) [[static_branch]] {
    v_out.select_id = custom_id.get_custom_id(inst_index);
    id = custom_id.get(inst_index);
  }
  else {
    id = res_id.get(inst_index);
  }
  uint view_id = 0;
  uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(view_id);
  const ObjectMatrices obj = models.get(resource_id);
  const ObjectInfos ob_infos = infos.get(resource_id);

  const draw::gsplat::PointShape gs = shape.get_point(uint(vert_id), obj, view);
  if (gs.is_culled()) {
    out_position = float4(NAN_FLT);
    return;
  }

  out_position = gs.hP;
  out_pointsize = srt.uniform_buf.sizes.vert * 2.0f;
  out_position.z -= srt.ndc_offset_factor * 0.5f;

  /* Get wire object color */
  float3 rim_col;
  float3 wire_col;
  if (srt.color_type == V3D_SHADING_OBJECT_COLOR || srt.color_type == V3D_SHADING_RANDOM_COLOR) {
    detail::wire_object_color_get(rim_col, wire_col, ob_infos, srt);
  }
  else {
    detail::wire_color_get(rim_col, wire_col, ob_infos, srt);
  }

  v_out.final_color = float4(wire_col * srt.wire_opacity, srt.wire_opacity);
  v_out.final_color_inner = float4(rim_col * srt.wire_opacity, srt.wire_opacity);

  if (pipe.use_clipping) [[static_branch]] {
    view_clipping_distances(gs.wP);
  }
}

[[fragment]] void frag([[resource_table]] const PipelineResources &pipe,
                       [[resource_table]] const Resources &srt,
                       [[resource_table, condition(selectable)]] draw::Select &select,
                       [[point_coord]] const float2 &in_pointcoord,
                       [[frag_coord]] const float4 frag_coordinate,
                       [[in]] const VertOut &v_in,
                       [[out]] FragOut &f_out)
{
  float2 centered = abs(in_pointcoord - float2(0.5f));
  float dist = max(centered.x, centered.y);

  /* Create a small gradient so that dense objects have a small fresnel effect. */
  /* Non linear blend. */
  float fac = dist * dist * 4.0f;
  float3 rim_col = sqrt(v_in.final_color_inner.rgb);
  float3 wire_col = sqrt(v_in.final_color.rgb);
  float3 final_front_col = mix(rim_col, wire_col, 0.35f);
  float3 color = mix(final_front_col, rim_col, saturate(fac));

  f_out.color = float4(color * color, v_in.final_color.a);
  f_out.line = float4(0.0f);

  if (pipe.selectable) [[static_branch]] {
    select.select_id_output(v_in.select_id, frag_coordinate);
  }
}

}  // namespace wireframe

namespace edit_vert {

struct Resources {
  [[uniform(OVERLAY_GLOBALS_SLOT)]] const UniformData &uniform_buf;
};
struct VertOut {
  [[flat]] float4 final_color;
};
struct FragOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 line;
};

[[vertex]] void vert([[resource_table]] const PipelineResources &pipe,
                     [[resource_table]] const Resources &srt,
                     [[resource_table]] const draw::gsplat::ShapeResource &shape,
                     [[resource_table]] const draw::Resource &res_id,
                     [[resource_table]] const draw::Model &models,
                     [[resource_table]] const draw::View &views,
                     [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
                     [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
                     [[vertex_id]] const int vert_id,
                     [[instance_index]] const int inst_index,
                     [[position]] float4 &out_position,
                     [[point_size]] float &out_pointsize,
                     [[clip_distance,
                       condition(use_clipping)]] float (&)[6], /* Used by `view_clipping_lib`. */
                     [[out]] VertOut &v_out)
{
  draw::ID id = res_id.get(inst_index);
  uint view_id = 0;
  uint resource_id = id.resource_id<1>();

  const ObjectMatrices obj = models.get(resource_id);
  const ViewMatrices view = views.get(view_id);

  const draw::gsplat::PointShape gs = shape.get_point(uint(vert_id), obj, view);
  if (gs.is_culled()) {
    out_position = float4(NAN_FLT);
    return;
  }

  /* NOTE: could just offset homogeneous z by a tiny bit instead? */
  float3 wP = gs.wP;
  float3 wV = view.world_incident_vector(wP);
  wP += wV * 1e-2f;

  /* Add small offset to z-position for depth precision. */
  out_position = view.point_world_to_homogenous(wP);
  out_position.z -= 3e-4f;
  out_pointsize = srt.uniform_buf.sizes.vert * 2.0f;

  v_out.final_color = srt.uniform_buf.colors.vert_select;

  if (pipe.use_clipping) [[static_branch]] {
    view_clipping_distances(wP);
  }
}

[[fragment]] void frag([[point_coord]] const float2 &in_pointcoord,
                       [[frag_coord]] const float4 frag_coordinate,
                       [[in]] const VertOut &v_in,
                       [[out]] FragOut &f_out)
{
  /* Round point with jagged edges. */
  float dist_squared = distance_squared(in_pointcoord, float2(0.5f));
  constexpr float dist_threshold = 0.25f;
  if (dist_squared > dist_threshold) {
    gpu_discard_fragment();
    return;
  }

  f_out.color = v_in.final_color;
  f_out.line = float4(0.0f);
}

}  // namespace edit_vert

namespace depth_only {

struct Resources {
  [[uniform(OVERLAY_GLOBALS_SLOT)]] const UniformData &uniform_buf;
};
struct VertOut {
  [[flat]] uint select_id;
};

[[vertex]] void vert(
    [[resource_table]] const PipelineResources &pipe,
    [[resource_table]] const Resources &srt,
    [[resource_table]] const draw::gsplat::ShapeResource &shape,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[resource_table, condition(selectable)]] const draw::Select & /* select */,
    [[resource_table, condition(selectable == 0)]] const draw::Resource &res_id,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &custom_id,
    [[vertex_id]] const int vert_id,
    [[instance_index]] const int inst_index,
    [[position]] float4 &out_position,
    [[clip_distance, condition(use_clipping)]] float (&)[6], /* Used by `view_clipping_lib`. */
    [[out]] VertOut &v_out)
{
  draw::ID id;
  if (pipe.selectable) [[static_branch]] {
    v_out.select_id = custom_id.get_custom_id(inst_index);
    id = custom_id.get(inst_index);
  }
  else {
    v_out.select_id = 0u;
    id = res_id.get(inst_index);
  }
  uint view_id = 0;
  uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(view_id);
  const ObjectMatrices obj = models.get(resource_id);

  const draw::gsplat::SplatShape gs = shape.get_splat(uint(vert_id), obj, view);
  if (gs.is_culled()) {
    out_position = float4(NAN_FLT);
    return;
  }

  /* Output splat billboard as position. */
  out_position = gs.hP;

  if (pipe.use_clipping) [[static_branch]] {
    view_clipping_distances(gs.wP);
  }
}

[[fragment]] void frag([[resource_table]] const PipelineResources &pipe,
                       [[resource_table]] const Resources &srt,
                       [[resource_table, condition(selectable)]] draw::Select &select,
                       [[frag_coord]] const float4 frag_coord,
                       [[front_facing]] const bool in_frontfacing,
                       [[in]] const VertOut &v_in)
{
  if (pipe.selectable) [[static_branch]] {
    if (srt.uniform_buf.backface_culling && !in_frontfacing) {
      /* Return early since we are not using early depth testing. */
      return;
    }

    /* This is optimized to NOP in the non select case. */
    select.select_id_output(v_in.select_id, frag_coord);
  }
}

}  // namespace depth_only

uint outline_colorid_get(ObjectInfos ob_infos, bool is_transform)
{
  eObjectInfoFlag ob_flag = ob_infos.flag;
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);

  if (is_transform) {
    return 0u; /* theme.colors.transform */
  }
  if (is_active) {
    return 3u; /* theme.colors.active */
  }
  return 1u; /* theme.colors.object_select */
}

namespace outline_prepass {

struct Resources {
  [[push_constant]] const bool is_transform;
};
struct VertOut {
  [[flat]] uint ob_id;
  [[flat]] float alpha;
  [[smooth]] float2 P;
};
struct FragOut {
  [[frag_color(0)]] uint ob_id;
};

[[vertex]] void vert(
    [[resource_table]] const PipelineResources &pipe,
    [[resource_table]] const Resources &srt,
    [[resource_table]] const draw::gsplat::ShapeResource &shape,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table, condition(selectable == 0)]] const draw::Resource &res_id,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &custom_id,
    [[vertex_id]] const int vert_id,
    [[instance_index]] const int inst_index,
    [[position]] float4 &out_position,
    [[clip_distance, condition(use_clipping)]] float (&)[6], /* Used by `view_clipping_lib`. */
    [[out]] VertOut &v_out)
{
  draw::ID id;
  if (pipe.selectable) [[static_branch]] {
    id = custom_id.get(inst_index);
  }
  else {
    id = res_id.get(inst_index);
  }
  uint view_id = 0;
  uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(view_id);
  const ObjectMatrices obj = models.get(resource_id);
  const ObjectInfos ob_infos = infos.get(resource_id);
  const draw::gsplat::SplatShape gs = shape.get_splat(uint(vert_id), obj, view);

  if (gs.is_culled()) {
    out_position = float4(NAN_FLT);
    return;
  }

  /* Output splat billboard as position. */
  out_position = gs.hP;

  /* Small bias to always be on top of the geom. */
  out_position.z -= 1e-3f;

  /* ID 0 is nothing (background) */
  v_out.ob_id = resource_id;

  /* Should be 2 bits only [0..3]. */
  uint outline_id = outline_colorid_get(ob_infos, srt.is_transform);

  /* Combine for 16bit uint target. */
  v_out.ob_id = outline_id_pack(outline_id, v_out.ob_id);

  v_out.P = gs.shape_offset;
  v_out.alpha = shape.get_opacity(gs.id);

  if (pipe.use_clipping) [[static_branch]] {
    view_clipping_distances(gs.wP);
  }
}

[[fragment]] void frag([[frag_coord]] const float4 frag_coord,
                       [[in]] const VertOut &v_in,
                       [[out]] FragOut &f_out)
{
  /* Discard outline parts below a threshold. Otherwise, the gsplat outline shows billboards. */
  float alpha = draw::gsplat::evaluate_gaussian(v_in.P, v_in.alpha);
  if (alpha < 0.1f) {
    gpu_discard_fragment();
  }

  f_out.ob_id = v_in.ob_id;
}

}  // namespace outline_prepass

/* clang-format off */
PipelineGraphic viewer_attribute_gsplat(viewer_attribute::vert, viewer_attribute::frag, PipelineResources{.use_clipping = false, .selectable = false});
PipelineGraphic viewer_attribute_gsplat_clipped(viewer_attribute::vert, viewer_attribute::frag, PipelineResources{.use_clipping = true, .selectable = false});

PipelineGraphic edit_gsplat(edit_vert::vert, edit_vert::frag, PipelineResources{.use_clipping = false, .selectable = false});
PipelineGraphic edit_gsplat_clipped(edit_vert::vert, edit_vert::frag, PipelineResources{.use_clipping = true, .selectable = false}); /* So anyway. */

PipelineGraphic depth_gsplat(depth_only::vert, depth_only::frag, PipelineResources{.use_clipping = false, .selectable = false});
PipelineGraphic depth_gsplat_selectable(depth_only::vert, depth_only::frag, PipelineResources{.use_clipping = false, .selectable = true});
PipelineGraphic depth_gsplat_clipped(depth_only::vert, depth_only::frag, PipelineResources{.use_clipping = true, .selectable = false});
PipelineGraphic depth_gsplat_selectable_clipped(depth_only::vert, depth_only::frag, PipelineResources{.use_clipping = true, .selectable = true});

PipelineGraphic outline_prepass_gsplat(outline_prepass::vert, outline_prepass::frag, PipelineResources{.use_clipping = false, .selectable = false});
PipelineGraphic outline_prepass_gsplat_selectable(outline_prepass::vert, outline_prepass::frag, PipelineResources{.use_clipping = false, .selectable = true});
PipelineGraphic outline_prepass_gsplat_clipped(outline_prepass::vert, outline_prepass::frag, PipelineResources{.use_clipping = true, .selectable = false});
PipelineGraphic outline_prepass_gsplat_selectable_clipped(outline_prepass::vert, outline_prepass::frag, PipelineResources{.use_clipping = true, .selectable = true});

PipelineGraphic wireframe_gsplat(wireframe::vert, wireframe::frag, PipelineResources{.use_clipping = false, .selectable = false});
PipelineGraphic wireframe_gsplat_selectable(wireframe::vert, wireframe::frag, PipelineResources{.use_clipping = false, .selectable = true});
PipelineGraphic wireframe_gsplat_clipped(wireframe::vert, wireframe::frag, PipelineResources{.use_clipping = true, .selectable = false});
PipelineGraphic wireframe_gsplat_selectable_clipped(wireframe::vert, wireframe::frag, PipelineResources{.use_clipping = true, .selectable = true});
/* clang-format on */

}  // namespace overlay
