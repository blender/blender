/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_nodetree)
VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)

#include "draw_model.bsl.hh"
#include "draw_pointcloud_lib.glsl"
#include "draw_view.bsl.hh"
#include "eevee_attributes_pointcloud_lib.glsl"
#include "eevee_nodetree_vert_lib.glsl"
#include "eevee_pipeline.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed becaused of fragment shader. */
#include "eevee_surf_common.bsl.hh"
#include "eevee_velocity.bsl.hh"

namespace eevee {

struct GeomPointCloud {
  [[legacy_info]] ShaderCreateInfo draw_pointcloud;

  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
  /* WORKAROUND: Until we get condition support for interfaces. */
  [[legacy_info]] ShaderCreateInfo eevee_geom_pointcloud_iface_info;

  [[push_constant]] bool ptcloud_backface;
};

[[vertex]] [[clip_control]] void geom_pointcloud(
    [[resource_table]] const PipelineConstants &pipe,
    [[resource_table]] const GeomPointCloud & /*srt*/,
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
  auto &pointcloud_interp = interface_get(eevee_geom_pointcloud_iface_info, pointcloud_interp);

  /* clang-format off */ /* Multiline macro breaks error line counting. */
  auto &pointcloud_interp_flat = interface_get(eevee_geom_pointcloud_iface_info, pointcloud_interp_flat);
  /* clang-format on */

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_iface = interface_get(eevee_shadow_iface_info, shadow_iface);

    shadow_iface.shadow_view_id = int(view_id);
    out_viewport = int(shadow.render_view_buf[view_id].viewport_index);
  }

  init_interface(id.raw_id);

  const eObjectInfoFlag ob_flag = ob_infos.flag;

  const pointcloud::Point ls_pt = pointcloud::point_get(uint(vert_id));
  const pointcloud::Point ws_pt = pointcloud::object_to_world(ls_pt, obj.model);
  const pointcloud::ShapePoint pt = pointcloud::shape_point_get(
      ws_pt, view.world_incident_vector(ws_pt.P), view.up(), ob_flag);

  pointcloud_interp_flat.id = ws_pt.point_id;
  pointcloud_interp.position = ws_pt.P;
  pointcloud_interp.radius = ws_pt.radius;

  interp.P = pt.P;
  interp.N = pt.N;

  if (pipe.is_shadow_pipe) [[static_branch]] {
    /* Since point clouds always face the view, camera and shadow orientation don't match.
     * Apply a bias to avoid self-shadow issues. */
    interp.P -= view.world_incident_vector(interp.P) * pointcloud_interp.radius;
  }

  if (pipe.use_velocity) [[static_branch]] {
    /* clang-format off */ /* Multiline define messes up line index. */
    [[resource_table]] const GeometryVelocity &geo_vel = resource_table_get(eevee::GeometryVelocity);
    /* clang-format on */
    auto &motion = interface_get(eevee_velocity_iface_info, motion);
    float3 lP = obj.point_world_to_object(pointcloud_interp.position);
    float3 prv, nxt;
    geo_vel.local_position_deltas(lP, pointcloud_interp_flat.id, prv, nxt, resource_id);
    /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
     * ignores motion from animated displacement. Supporting animated displacement motion vectors
     * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated
     * at different times, but also with different attributes (maybe we could assume static
     * attribute at least). */
    geo_vel.vertex_velocity(prv, lP, nxt, motion.prev, motion.next, resource_id, obj.model);
  }

  /* Compute Original Coordinate (ORCO). */
  float3 lP_orco = ls_pt.P * ob_infos.orco_mul + ob_infos.orco_add;

  init_globals(uni, view, true);
  attrib_load(PointCloudPoint{ls_pt.P, ws_pt.point_id, lP_orco});

  interp.P += nodetree_displacement();

  if (pipe.use_clip_plane) [[static_branch]] {
    auto &clip_interp = interface_get(eevee_clip_plane, clip_interp);
    const auto &clip_plane = buffer_get(eevee_clip_plane, clip_plane);
    clip_interp.clip_distance = dot(clip_plane.plane, float4(interp.P, 1.0f));
  }

  float3 vs_P = view.point_world_to_view(interp.P);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_clip = interface_get(eevee_shadow_iface_info, shadow_clip);

    float3 vs_P = view.point_world_to_view(interp.P);
    ShadowRenderView view = shadow.render_view_buf[view_id];
    shadow_clip.position = shadow_position_vector_get(vs_P, view);
    shadow_clip.vector = shadow_clip_vector_get(vs_P, view.clip_distance_inv);
  }

  out_position = reverse_z::transform(view.point_view_to_homogenous(vs_P));
}

}  // namespace eevee
