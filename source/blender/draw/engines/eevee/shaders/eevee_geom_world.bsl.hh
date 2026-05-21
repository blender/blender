/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Custom full-screen triangle with placeholders varyings.
 */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_nodetree)

#include "draw_view_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"

namespace eevee {

struct GeomWorld {
  [[legacy_info]] ShaderCreateInfo draw_modelmat;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo draw_resource_id_varying;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
};

[[vertex]] [[clip_control]] void geom_world([[resource_table]] const GeomWorld & /*srt*/,
                                            [[vertex_id]] const int vert_id,
                                            [[position]] float4 &out_position)
{
  auto &resource_iface = interface_get(draw_resource_id_varying, drw_ResourceID_iface);
  /* (W)Intel drivers require all varying iface to be written to inside the Vertex shader. */
  resource_iface.resource_id = 0u;

  auto &interp = interface_get(eevee_geom_iface_info, interp);

  /* Full-screen triangle. */
  int v = vert_id % 3;
  float x = float((v & 1) << 2) - 1.0f;
  float y = float((v & 2) << 1) - 1.0f;
  out_position = float4(x, y, 1.0f, 1.0f);

  /* Pass view position to keep accuracy. */
  interp.P = drw_point_ndc_to_view(out_position.xyz);
  interp.N = float3(1);

  out_position = reverse_z::transform(out_position);
}

}  // namespace eevee
