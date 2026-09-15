/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

#include "draw_gsplat_lib.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "eevee_attributes_pointcloud_lib.glsl"
#include "eevee_nodetree_vert_lib.glsl"
#include "eevee_pipeline.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed because of fragment shader. */
#include "eevee_surf_common.bsl.hh"
#include "eevee_velocity.bsl.hh"

namespace eevee {

struct GeomGSplat {
  /* WORKAROUND: Until we get condition support for interfaces. */
  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
  [[legacy_info]] ShaderCreateInfo eevee_geom_gsplat_iface_info;
};

[[vertex]] [[clip_control]] void geom_gsplat(
    [[resource_table]] const PipelineConstants &pipe,
    [[resource_table]] const GeomGSplat & /*srt*/,
    [[resource_table]] const draw::gsplat::ShapeResource &shape,
    [[resource_table]] const Uniform &uni,
    [[instance_index]] const int inst_index,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table]] const draw::Resource &res_id,
    [[resource_table, condition(is_shadow_pipe)]] GeomShadow &shadow,
    [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
    [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
    [[vertex_id]] const int vert_id,
    [[position]] float4 &out_position,
    /* Note: Removed manually if not needed. Otherwise, can generate geometry shader fallback. */
    [[viewport_index]] int &out_viewport)
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
  const ObjectInfos ob_infos = infos.get(resource_id);

  auto &interp = interface_get(eevee_geom_iface_info, interp);
  auto &gsplat_interp = interface_get(eevee_geom_gsplat_iface_info, gsplat_interp);
  auto &gsplat_interp_flat = interface_get(eevee_geom_gsplat_iface_info, gsplat_interp_flat);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_iface = interface_get(eevee_shadow_iface_info, shadow_iface);

    shadow_iface.shadow_view_id = int(view_id);
    out_viewport = int(shadow.render_view_buf[view_id].viewport_index);
  }

  init_interface(id.raw_id);

  /* TODO(not_mark): Disabled compute pass for now, due to incompatibility with instancing. */
  /* draw::gsplat::SplatShape gs;
  if (pipe.is_shadow_pipe) [[static_branch]] {
    gs = shape.get_splat_fallback(uint(vert_id), obj, view);
  }
  else {
    gs = shape.get_splat(uint(vert_id), obj, view);
  } */
  draw::gsplat::SplatShape gs = shape.get_splat(uint(vert_id), obj, view);
  if (gs.is_culled()) {
    out_position = float4(NAN_FLT);
    return;
  }

  /* Gaussian splats typically do an alpha cut off. The original paper used 1/255
   * as a threshold, which persists in te fitted data. */
  float opacity = shape.get_opacity(gs.id);
  if (opacity < (1.0f / 255.0f)) {
    out_position = float4(NAN_FLT);
    return;
  }

  /* Output interface data. */
  gsplat_interp.billboard_co = gs.shape_offset;
  gsplat_interp_flat.opacity = opacity;
  interp.P = gs.wP;
  interp.N = gs.wN;

  if (pipe.is_shadow_pipe) [[static_branch]] {
    /* GSplat billboards always face the view, so camera and shadow orientation don't match.
     * Apply a bias to avoid self-shadow issues. */
    interp.P -= view.world_incident_vector(interp.P) * 1e-3f;
  }

  if (pipe.use_velocity) [[static_branch]] {
    /* clang-format off */ /* Multi-line define messes up line index. */
    [[resource_table]] const GeometryVelocity &geo_vel =
    resource_table_get(eevee::GeometryVelocity);
    /* clang-format on */
    auto &motion = interface_get(eevee_velocity_iface_info, motion);
    float3 lP = gs.mean;
    float3 prv, nxt;
    geo_vel.local_position_deltas(lP, int(gs.id), prv, nxt, resource_id);
    /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
     * ignores motion from animated displacement. Supporting animated displacement motion vectors
     * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated
     * at different times, but also with different attributes (maybe we could assume static
     * attribute at least). */
    geo_vel.vertex_velocity(prv, lP, nxt, motion.prev, motion.next, resource_id, obj.model);
  }

  /* Compute Original Coordinate (ORCO). */
  float3 lP_orco = gs.mean * ob_infos.orco_mul + ob_infos.orco_add;

  init_globals(uni, view, true);
  attrib_load(PointCloudPoint{gs.mean, int(gs.id), lP_orco});

  /* NOTE(not_mark) What does displacement even mean on a gsplat :S */
  /* interp.P += nodetree_displacement(); */

  if (pipe.use_clip_plane) [[static_branch]] {
    auto &clip_interp = interface_get(eevee_clip_plane, clip_interp);
    const auto &clip_plane = buffer_get(eevee_clip_plane, clip_plane);
    clip_interp.clip_distance = dot(clip_plane.plane, float4(interp.P, 1.0f));
  }

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_clip = interface_get(eevee_shadow_iface_info, shadow_clip);

    float3 vs_P = view.point_world_to_view(interp.P);
    ShadowRenderView view = shadow.render_view_buf[view_id];
    shadow_clip.position = shadow_position_vector_get(vs_P, view);
    shadow_clip.vector = shadow_clip_vector_get(vs_P, view.clip_distance_inv);
  }

  out_position = reverse_z::transform(gs.hP);
}

}  // namespace eevee
