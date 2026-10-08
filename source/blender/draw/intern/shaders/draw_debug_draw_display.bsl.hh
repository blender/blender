/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Display debug edge list.
 */

#pragma once

#include "draw_debug_shared.hh"

/* TODO(fclem): Deduplicate with overlay. */
/* edge_start and edge_pos needs to be in the range [0..sizeViewport]. */
float4 pack_line_data(float2 frag_co, float2 edge_start, float2 edge_pos)
{
  float2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0f) {
    edge /= len;

    /* Get perpendicular in direction of upper hemicircle. */
    float2 perp = float2(-edge.y, edge.x);
    if (perp.y < 0.0) {
      perp = -perp;
    }

    /* Get distance along perpendicular by projection of edge.  */
    float sin_theta = perp.x;
    float dist = dot(perp, frag_co - edge_start);

    /* Leave 0.1f boundary around dist to differentiate cleared or intentionally blocked pixels. */
    return float4(sin_theta * 0.5f + 0.5f, dist * 0.4f + 0.5f, 0.0f, 1.0f);
  }
  /* Default line if the origin is perfectly aligned with a pixel. */
  return float4(0.0f, 0.5f, 0.0f, 1.0f);
}

namespace draw::debug {

struct VertOut {
  [[no_perspective]] float2 edge_pos;
  [[flat]] float2 edge_start;
  [[flat]] float4 final_color;
};

struct DebugDrawDisplay {
  [[storage(DRW_DEBUG_DRAW_SLOT, read)]] DRWDebugVertPair (&in_debug_lines_buf)[];
  [[storage(DRW_DEBUG_DRAW_FEEDBACK_SLOT, read_write)]] DRWDebugVertPair (&out_debug_lines_buf)[];

  [[push_constant]] const float4x4 persmat;
  [[push_constant]] const float2 size_viewport;
};

[[vertex]]
void vert_main([[resource_table]] DebugDrawDisplay &srt,
               [[out]] VertOut &v_out,
               [[position]] float4 &out_pos,
               [[vertex_id]] const int vert_id)
{
  int line_id = (vert_id / 2);
  bool is_provoking_vertex = (vert_id & 1) == 0;
  /* Skip the first vertex containing header data. */
  DRWDebugVertPair vert = srt.in_debug_lines_buf[line_id + drw_debug_draw_offset];

  float3 pos = uintBitsToFloat((is_provoking_vertex) ?
                                   uint3(vert.pos1_x, vert.pos1_y, vert.pos1_z) :
                                   uint3(vert.pos2_x, vert.pos2_y, vert.pos2_z));
  float4 col = float4((uint4(vert.vert_color) >> uint4(0, 8, 16, 24)) & 0xFFu) / 255.0f;

  /* Lifetime management. */
  if (is_provoking_vertex && vert.lifetime > 1) {
    /* drw_debug_draw_v_count */
    uint vertid = atomicAdd(srt.out_debug_lines_buf[0].pos1_x, 2u);
    if (vertid < DRW_DEBUG_DRAW_VERT_MAX) {
      uint out_line_id = vertid / 2u;
      vert.lifetime -= 1;
      srt.out_debug_lines_buf[out_line_id + drw_debug_draw_offset] = vert;
    }
  }

  v_out.final_color = col;
  out_pos = srt.persmat * float4(pos, 1.0f);

  v_out.edge_start = v_out.edge_pos = (0.5f * (out_pos.xy / out_pos.w) + 0.5f) * srt.size_viewport;
}

struct FragOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 line_data;
};

[[fragment]]
void frag_main([[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out,
               [[frag_coord]] const float4 frag_co)
{
  frag_out.color = v_out.final_color;
  frag_out.line_data = pack_line_data(frag_co.xy, v_out.edge_start, v_out.edge_pos);
}

PipelineGraphic draw_display(vert_main, frag_main);

}  // namespace draw::debug
