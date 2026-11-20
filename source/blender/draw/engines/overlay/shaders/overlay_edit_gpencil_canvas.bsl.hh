/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Display a 3D grid at the grease pencil object draw plane.
 */
#pragma once

#include "draw_view.bsl.hh"
#include "overlay_common.bsl.hh"

namespace overlay::edit_gpencil::canvas {

struct Resources {
  [[push_constant]] float4 color;
  [[push_constant]] float3 axis_x;
  [[push_constant]] float3 axis_y;
  [[push_constant]] float3 origin;
  [[push_constant]] int half_line_count;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const Clipping &clip,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Uniform &uni,
               [[vertex_id]] const int vert_id,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] VertOut &v_out,
               [[position]] float4 &out_pos)
{
  float2 pos;
  pos.x = float(vert_id % 2);
  pos.y = float(vert_id / 2) / float(srt.half_line_count - 1);

  if (pos.y > 1.0f) {
    pos.xy = pos.yx;
    pos.x -= 1.0f + 1.0f / float(srt.half_line_count - 1);
  }

  pos -= 0.5f;

  const ViewMatrices view = views.get(0);

  float3 world_pos = srt.axis_x * pos.x + srt.axis_y * pos.y + srt.origin;

  out_pos = view.point_world_to_homogenous(world_pos);

  v_out.final_color = srt.color;

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

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
}  // namespace overlay::edit_gpencil::canvas

namespace overlay {
PipelineGraphic gpencil_canvas(edit_gpencil::canvas::vert_main,
                               wire_frag,
                               ClippingConstant{.use_clipping = false},
                               draw::SelectConstant{.selectable = false});
PipelineGraphic gpencil_canvas_clipped(edit_gpencil::canvas::vert_main,
                                       wire_frag,
                                       ClippingConstant{.use_clipping = true},
                                       draw::SelectConstant{.selectable = false});
}  // namespace overlay
