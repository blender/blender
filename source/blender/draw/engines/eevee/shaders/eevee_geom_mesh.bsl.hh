/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_nodetree)
VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)

#include "draw_model_lib.glsl"
#include "eevee_attributes_mesh_lib.glsl"
#include "eevee_nodetree_vert_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed becaused of fragment shader. */
#include "eevee_surf_common.bsl.hh"
#include "eevee_velocity.bsl.hh"

namespace eevee {

struct GeomMesh {
  [[legacy_info]] ShaderCreateInfo draw_modelmat;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo draw_resource_id_varying;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
};

struct GeomMeshVertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float3 nor;
};

[[vertex]] [[clip_control]] void geom_mesh(
    [[resource_table]] const PipelineConstants &pipe,
    [[resource_table]] const GeomMesh & /*srt*/,
    [[resource_table, condition(is_shadow_pipe)]] const GeomShadow &shadow,
    [[in]] const GeomMeshVertIn &vert_in,
    [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
    [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
    [[vertex_id]] const int vert_id,
    [[position]] float4 &out_position,
    /* Note: Removed manually if not needed. Otherwise, can generate geometry shader fallback. */
    [[viewport_index]] int &out_viewport)
{
  DRW_VIEW_FROM_RESOURCE_ID;

  auto &interp = interface_get(eevee_geom_iface_info, interp);

  if (pipe.is_shadow_pipe) [[static_branch]] {
    auto &shadow_iface = interface_get(eevee_shadow_iface_info, shadow_iface);

    shadow_iface.shadow_view_id = int(drw_view_id);
    out_viewport = int(shadow.render_view_buf[drw_view_id].viewport_index);
  }

  init_interface();

  interp.P = drw_point_object_to_world(vert_in.pos);
  interp.N = normalize(drw_normal_object_to_world(vert_in.nor));
  if (pipe.use_velocity) [[static_branch]] {
    /* clang-format off */ /* Multiline define messes up line index. */
    [[resource_table]] const GeometryVelocity &geo_vel = resource_table_get(eevee::GeometryVelocity);
    /* clang-format on */
    auto &motion = interface_get(eevee_velocity_iface_info, motion);
    float3 prv, nxt;
    geo_vel.local_position_deltas(vert_in.pos, vert_id, prv, nxt, drw_resource_id());
    /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
     * ignores motion from animated displacement. Supporting animated displacement motion vectors
     * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated
     * at different times, but also with different attributes (maybe we could assume static
     * attribute at least). */
    geo_vel.vertex_velocity(
        prv, vert_in.pos, nxt, motion.prev, motion.next, drw_resource_id(), drw_modelmat());
  }

  init_globals(true);
  attrib_load(MeshVertex{vert_in.pos});

  interp.P += nodetree_displacement();

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
