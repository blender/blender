/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Custom full-screen triangle with placeholders varyings.
 */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "eevee_lightprobe_shared.hh" /* TODO(fclem): Remove. Needed because of fragment shader. */
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* TODO(fclem): Remove. Needed because of fragment shader. */
#include "eevee_uniform.bsl.hh"

namespace eevee {

struct GeomWorld {
  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;
};

[[vertex]] [[clip_control]] void geom_world([[resource_table]] const GeomWorld & /*srt*/,
                                            [[resource_table]] const Uniform & /*uni*/,
                                            [[resource_table]] const draw::View &views,
                                            [[resource_table]] const draw::Model & /*models*/,
                                            [[vertex_id]] const int vert_id,
                                            [[position]] float4 &out_position)
{
  auto &interp = interface_get(eevee_geom_iface_info, interp);
  auto &interp_flat = interface_get(eevee_geom_iface_info, interp_flat);
  /* (W)Intel drivers require all varying iface to be written to inside the Vertex shader. */
  interp_flat.resource_id_raw = 0u;

  /* Full-screen triangle. */
  int v = vert_id % 3;
  float x = float((v & 1) << 2) - 1.0f;
  float y = float((v & 2) << 1) - 1.0f;
  out_position = float4(x, y, 1.0f, 1.0f);

  /* Pass view position to keep accuracy. */
  interp.P = views.get(0).point_ndc_to_view(out_position.xyz);
  interp.N = float3(1);

  out_position = reverse_z::transform(out_position);
}

}  // namespace eevee
