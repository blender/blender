/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_curves.bsl.hh"
#include "draw_model.bsl.hh"
#include "eevee_attributes_curves_lib.bsl.hh" /* IWYU pragma: export */
#include "eevee_nodetree_vert_lib.bsl.hh"
#include "eevee_pipeline.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed because of fragment shader. */
#include "eevee_shadow_shared.hh"
#include "eevee_surf_common.bsl.hh"
#include "eevee_velocity.bsl.hh"

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  define const
#endif

namespace eevee {

[[vertex]] [[clip_control]] void geom_curves(
    [[resource_table]] KernelGlobals &kg,
    [[resource_table]] const PipelineConstants &pipe,
    [[resource_table]] const draw::Curves &curves,
    [[resource_table]] const Uniform &uni,
    [[instance_index]] const int inst_index,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table]] const draw::Resource &res_id,
    [[resource_table, condition(is_shadow_pipe)]] GeomShadow &shadow,
    [[resource_table, condition(use_clip_plane)]] const ClipPlane &clip_srt,
    [[resource_table, condition(use_velocity)]] const GeometryVelocity &geo_vel,
    [[out]] VertOutCommon &interp,
    [[out]] VertOutCurves &curve_interp,
    [[out, condition(is_shadow_pipe)]] VertOutShadow &shadow_iface,
    [[out, condition(is_shadow_pipe)]] VertOutShadowClipping &shadow_clip,
    [[out, condition(use_clip_plane)]] VertOutClipPlane &clip_interp,
    [[out, condition(use_velocity)]] VertOutVelocity &motion,
    [[vertex_id]] const int vert_id,
    [[position]] float4 &out_position,
    [[viewport_index, condition(is_shadow_pipe &&use_multi_viewport)]] int &out_viewport)
{
  draw::ID id = res_id.get(inst_index);
  uint view_id = 0;
  uint resource_id = id.resource_id<1>();
  if (pipe.is_shadow_pipe) [[static_branch]] {
    view_id = id.view_id<64>();
    resource_id = id.resource_id<64>();
  }

  const ViewMatrices view = views.get(view_id);
  const ObjectMatrices obj = models.get(resource_id);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    shadow_iface.shadow_view_id = int(view_id);
    if (pipe.use_multi_viewport) [[static_branch]] {
      out_viewport = int(shadow.render_view_buf[view_id].viewport_index);
    }
  }

  init_interface(interp, id.raw_id);

  const draw::curves::Point ls_pt = curves.point_get(uint(vert_id));
  const draw::curves::Point ws_pt = curves.object_to_world(ls_pt, obj.model);

  const float3 V = view.world_incident_vector(ws_pt.P);

  const draw::curves::ShapePoint pt = curves.shape_point_get(ws_pt, V);
  interp.P = pt.P;
  /* Correct normal is derived in fragment shader. */
  interp.N = pt.curve_N;

  curve_interp.binormal = pt.curve_B;
  curve_interp.tangent = pt.curve_T;
  /* Final radius is used for correct normal interpolation. */
  curve_interp.radius = ws_pt.radius;
  /* Scaled by radius for correct interpolation. */
  curve_interp.time_width = ws_pt.azimuthal_offset * ws_pt.radius;
  /* Note: Used for attribute loading. */
  curve_interp.point_id = float(ws_pt.point_id);
  curve_interp.strand_id = ws_pt.curve_id;

  if (pipe.use_velocity) [[static_branch]] {
    /* Due to the screen space nature of the vertex positioning, we compute only the motion of
     * curve strand, not its cylinder. Otherwise we would add the rotation velocity. */
    int vert_idx = ws_pt.point_id;
    float3 prv, nxt;
    float3 pos = ls_pt.P;
    geo_vel.local_position_deltas(pos, vert_idx, prv, nxt, resource_id);
    /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
     * ignores motion from animated displacement. Supporting animated displacement motion vectors
     * would require evaluating the node-tree multiple time with different node-tree UBOs evaluated
     * at different times, but also with different attributes (maybe we could assume static
     * attribute at least). */
    geo_vel.vertex_velocity(prv, pos, nxt, motion.prev, motion.next, resource_id, obj.model);
  }

  ObjectInfos ob_infos = infos.get(resource_id);
  /* Compute Original Coordinate (ORCO). */
  float3 lP_root = curves.get_curve_root_pos(ws_pt.point_id, ws_pt.curve_segment);
  float3 lP_orco = lP_root * ob_infos.orco_mul + ob_infos.orco_add;

  ShadingData sd = init_globals(uni, interp, view, true, float4(0));
  init_globals_curves(interp, curve_interp, sd, view);

  attrib_load(CurvesPoint{ws_pt.curve_id,
                          ws_pt.point_id,
                          ws_pt.curve_segment,
                          curves.drw_curves.is_point_attribute,
                          lP_orco});

  interp.P += nodetree_displacement(kg, sd);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    /* Since curves always face the view, camera and shadow orientation don't match.
     * Apply a bias to avoid self-shadow issues. */
    interp.P -= V * ws_pt.radius;
  }

  if (pipe.use_clip_plane) [[static_branch]] {
    clip_interp.clip_distance = dot(clip_srt.clip_plane.plane, float4(interp.P, 1.0f));
  }

  float3 vs_P = view.point_world_to_view(interp.P);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    ShadowRenderView view = shadow.render_view_buf[view_id];
    shadow_clip.position = shadow_position_vector_get(vs_P, view);
    shadow_clip.vector = shadow_clip_vector_get(vs_P, view.clip_distance_inv);
  }

  out_position = reverse_z::transform(view.point_view_to_homogenous(vs_P));
}

}  // namespace eevee

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  undef const
#endif
