/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_nodetree)

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "eevee_pipeline.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed becaused of fragment shader. */
#include "eevee_surf_common.bsl.hh"
#include "eevee_uniform.bsl.hh"

namespace eevee {

struct GeomVolume {
  [[legacy_info]] ShaderCreateInfo draw_volume_infos;

  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
};

struct GeomVolumeIn {
  [[attribute(0)]] float3 pos;
};

[[vertex]] [[clip_control]] void geom_volume(
    [[resource_table]] const PipelineConstants & /*pipe*/,
    [[resource_table]] const GeomVolume & /*srt*/,
    [[resource_table]] const Uniform & /*uni*/,
    [[instance_index]] const int inst_index,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::Infos &infos,
    [[resource_table]] const draw::Resource &res_id,
    [[in]] const GeomVolumeIn &vert_in,
    [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
    [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
    [[position]] float4 &out_position)
{
  const draw::ID id = res_id.get(inst_index);
  const uint view_id = 0;
  const uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(view_id);
  const ObjectMatrices obj = models.get(resource_id);

  auto &interp = interface_get(eevee_geom_iface_info, interp);

  init_interface(id.raw_id);

  /* TODO(fclem): Find a better way? This is reverting what draw_resource_finalize does. */
  ObjectInfos info = infos.get(resource_id);
  float3 size = safe_rcp(info.orco_mul * 2.0f);         /* Box half-extent. */
  float3 loc = size + (info.orco_add / -info.orco_mul); /* Box center. */

  /* Use bounding box geometry for now. */
  float3 lP = loc + vert_in.pos * size;
  interp.P = obj.point_object_to_world(lP);

  out_position = reverse_z::transform(view.point_world_to_homogenous(interp.P));
}

}  // namespace eevee
