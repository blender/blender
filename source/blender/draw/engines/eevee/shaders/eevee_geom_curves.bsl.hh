/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_infos.hh"
#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_nodetree)
VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)

#include "draw_curves_lib.glsl"
#include "draw_model_lib.glsl"
#include "eevee_attributes_curves_lib.glsl"
#include "eevee_nodetree_vert_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed becaused of fragment shader. */
#include "eevee_surf_common.bsl.hh"
#include "eevee_velocity.bsl.hh"

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  define const
#endif

namespace eevee {

struct GeomCurve {
  [[legacy_info]] ShaderCreateInfo draw_modelmat;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo draw_resource_id_varying;
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_curves;
  [[legacy_info]] ShaderCreateInfo draw_curves_infos;

  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
  /* WORKAROUND: Until we get condition support for interfaces. */
  [[legacy_info]] ShaderCreateInfo eevee_geom_curves_iface_info;
};

[[vertex]] [[clip_control]] void geom_curves(
    [[resource_table]] const PipelineConstants &pipe,
    [[resource_table]] const GeomCurve & /*srt*/,
    [[resource_table, condition(is_shadow_pipe)]] GeomShadow &shadow,
    [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
    [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
    [[vertex_id]] const int vert_id,
    [[position]] float4 &out_position,
    /* Note: Removed manually if not needed. Otherwise, can generate geometry shader fallback. */
    [[viewport_index]] int &out_viewport)
{
  DRW_VIEW_FROM_RESOURCE_ID;

  auto &interp = interface_get(eevee_geom_iface_info, interp);
  auto &curve_interp = interface_get(eevee_geom_curves_iface_info, curve_interp);
  auto &curve_interp_flat = interface_get(eevee_geom_curves_iface_info, curve_interp_flat);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_iface = interface_get(eevee_shadow_iface_info, shadow_iface);

    shadow_iface.shadow_view_id = int(drw_view_id);
    out_viewport = int(shadow.render_view_buf[drw_view_id].viewport_index);
  }

  init_interface();

  const curves::Point ls_pt = curves::point_get(uint(vert_id));
  const curves::Point ws_pt = curves::object_to_world(ls_pt, drw_modelmat());

  const float3 V = drw_world_incident_vector(ws_pt.P);

  const curves::ShapePoint pt = curves::shape_point_get(ws_pt, V);
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
  curve_interp_flat.strand_id = ws_pt.curve_id;

  if (pipe.use_velocity) [[static_branch]] {
    /* clang-format off */ /* Multiline define messes up line index. */
    [[resource_table]] const GeometryVelocity &geo_vel = resource_table_get(eevee::GeometryVelocity);
    /* clang-format on */
    auto &motion = interface_get(eevee_velocity_iface_info, motion);
    /* Due to the screen space nature of the vertex positioning, we compute only the motion of
     * curve strand, not its cylinder. Otherwise we would add the rotation velocity. */
    int vert_idx = ws_pt.point_id;
    float3 prv, nxt;
    float3 pos = ls_pt.P;
    geo_vel.local_position_deltas(pos, vert_idx, prv, nxt, drw_resource_id());
    /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
     * ignores motion from animated displacement. Supporting animated displacement motion vectors
     * would require evaluating the node-tree multiple time with different node-tree UBOs evaluated
     * at different times, but also with different attributes (maybe we could assume static
     * attribute at least). */
    geo_vel.vertex_velocity(
        prv, pos, nxt, motion.prev, motion.next, drw_resource_id(), drw_modelmat());
  }

  init_globals(true);
  attrib_load(CurvesPoint{ws_pt.curve_id, ws_pt.point_id, ws_pt.curve_segment});

  interp.P += nodetree_displacement();

  if (pipe.is_shadow_pipe) [[static_branch]] {
    /* Since curves always face the view, camera and shadow orientation don't match.
     * Apply a bias to avoid self-shadow issues. */
    interp.P -= V * ws_pt.radius;
  }

  if (pipe.use_clip_plane) [[static_branch]] {
    auto &clip_interp = interface_get(eevee_clip_plane, clip_interp);
    const auto &clip_plane = buffer_get(eevee_clip_plane, clip_plane);
    clip_interp.clip_distance = dot(clip_plane.plane, float4(interp.P, 1.0f));
  }

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_clip = interface_get(eevee_shadow_iface_info, shadow_clip);

    float3 vs_P = drw_point_world_to_view(interp.P);
    ShadowRenderView view = shadow.render_view_buf[drw_view_id];
    shadow_clip.position = shadow_position_vector_get(vs_P, view);
    shadow_clip.vector = shadow_clip_vector_get(vs_P, view.clip_distance_inv);
  }

  out_position = reverse_z::transform(drw_point_world_to_homogenous(interp.P));
}

}  // namespace eevee

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  undef const
#endif
