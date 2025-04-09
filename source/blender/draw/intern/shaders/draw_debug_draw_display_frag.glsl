/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Display debug edge list.
 */

#include "draw_debug_info.hh"

FRAGMENT_SHADER_CREATE_INFO(draw_debug_draw_display)

/* TODO(fclem): Deduplicate with overlay. */
/* edge_start and edge_pos needs to be in the range [0..sizeViewport]. */
vec4 pack_line_data(vec2 frag_co, vec2 edge_start, vec2 edge_pos)
{
  vec2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0) {
    edge /= len;
    vec2 perp = vec2(-edge.y, edge.x);
    float dist = dot(perp, frag_co - edge_start);
    /* Add 0.1 to differentiate with cleared pixels. */
    return vec4(perp * 0.5 + 0.5, dist * 0.25 + 0.5 + 0.1, 1.0);
  }
  /* Default line if the origin is perfectly aligned with a pixel. */
  return vec4(1.0, 0.0, 0.5 + 0.1, 1.0);
}

void main()
{
  out_color = final_color;
  out_line_data = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
}
