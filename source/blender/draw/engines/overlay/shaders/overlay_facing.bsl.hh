/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Draws meshes with depth test equal and color faces
 * depending on their winding (back/front facing).
 */

#pragma once

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_compat.hh"
#include "overlay_common.bsl.hh"

namespace overlay::facing {

struct VertIn {
  [[attribute(0)]] float3 pos;
};

[[vertex]] void vert_main([[resource_table]] const Clipping &clip,
                          [[resource_table]] const draw::Model &models,
                          [[resource_table]] const draw::View &views,
                          [[resource_table]] const draw::Resource &resources,
                          [[instance_index]] const int inst_index,
                          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                          [[in]] const VertIn &v_in,
                          [[position]] float4 &out_pos)
{
  const uint resource_id = resources.get(inst_index).resource_id<1>();

  const ObjectMatrices obj = models.get(resource_id);
  const ViewMatrices view = views.get(0);

  const float3 world_pos = obj.point_object_to_world(v_in.pos);
  out_pos = view.point_world_to_homogenous(world_pos);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

[[fragment]] void frag_main([[resource_table]] const Uniform &uni,
                            [[front_facing]] const bool facing,
                            [[out]] FragOut &frag_out)
{
  const UniformData &theme = uni.uniform_buf;
  frag_out.color = facing ? theme.colors.face_front : theme.colors.face_back;
  /* Pre-multiply the output as we do not do any blending in the frame-buffer. */
  frag_out.color.rgb *= frag_out.color.a;
}

}  // namespace overlay::facing

/* clang-format off */
PipelineGraphic overlay_facing(                    overlay::facing::vert_main, overlay::facing::frag_main, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_facing_clipped(            overlay::facing::vert_main, overlay::facing::frag_main, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
/* clang-format on */
