/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_model.bsl.hh"
#include "eevee_attributes_mesh_lib.bsl.hh" /* IWYU pragma: export */
#include "eevee_nodetree_vert_lib.bsl.hh"
#include "eevee_pipeline.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"
#include "eevee_velocity.bsl.hh"

namespace eevee {

struct GeomMeshVertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float3 nor;
};

[[vertex]] [[clip_control]] void geom_mesh(
    [[resource_table]] KernelGlobals &kg,
    [[resource_table]] const PipelineConstants &pipe,
    [[resource_table]] const Uniform &uni,
    [[instance_index]] const int inst_index,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table]] const draw::Resource &res_id,
    [[resource_table, condition(is_shadow_pipe)]] const GeomShadow &shadow,
    [[resource_table, condition(use_clip_plane)]] const ClipPlane &clip_srt,
    [[resource_table, condition(use_velocity)]] const GeometryVelocity &geo_vel,
    [[in]] const GeomMeshVertIn &vert_in,
    [[out]] VertOutCommon &interp,
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

  interp.P = obj.point_object_to_world(vert_in.pos);
  interp.N = normalize(obj.normal_object_to_world(vert_in.nor));
  if (pipe.use_velocity) [[static_branch]] {
    float3 prv, nxt;
    geo_vel.local_position_deltas(vert_in.pos, vert_id, prv, nxt, resource_id);
    /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
     * ignores motion from animated displacement. Supporting animated displacement motion vectors
     * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated
     * at different times, but also with different attributes (maybe we could assume static
     * attribute at least). */
    geo_vel.vertex_velocity(
        prv, vert_in.pos, nxt, motion.prev, motion.next, resource_id, obj.model);
  }

  ObjectInfos ob_infos = infos.get(resource_id);
  /* Compute Original Coordinate (ORCO). */
  float3 lP_orco = vert_in.pos * ob_infos.orco_mul + ob_infos.orco_add;

  ShadingData sd = init_globals(uni, interp, view, true, float4(0));
  init_globals_mesh(interp, sd);

  attrib_load(MeshVertex{vert_in.pos, to_float3x3(obj.model_inverse), lP_orco});

  interp.P += nodetree_displacement(kg, sd);

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
