/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_nodetree)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"

namespace eevee {

struct GeomVolume {
  [[legacy_info]] ShaderCreateInfo draw_modelmat;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo draw_resource_id_varying;
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_volume_infos;

  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
};

struct GeomVolumeIn {
  [[attribute(0)]] float3 pos;
};

[[vertex]] [[clip_control]] void geom_volume(
    [[resource_table]] const GeomVolume & /*srt*/,
    [[in]] const GeomVolumeIn &vert_in,
    [[instance_id]] const int /*inst_id*/,     /* Used by model_lib. */
    [[base_instance]] const int /*base_inst*/, /* Used by model_lib. */
    [[position]] float4 &out_position)
{
  DRW_VIEW_FROM_RESOURCE_ID;

  auto &interp = interface_get(eevee_geom_iface_info, interp);

  init_interface();

  /* TODO(fclem): Find a better way? This is reverting what draw_resource_finalize does. */
  ObjectInfos info = drw_object_infos();
  float3 size = safe_rcp(info.orco_mul * 2.0f);         /* Box half-extent. */
  float3 loc = size + (info.orco_add / -info.orco_mul); /* Box center. */

  /* Use bounding box geometry for now. */
  float3 lP = loc + vert_in.pos * size;
  interp.P = drw_point_object_to_world(lP);

  out_position = reverse_z::transform(drw_point_world_to_homogenous(interp.P));
}

}  // namespace eevee
