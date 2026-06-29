/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Display debug edge list.
 */

#include "draw_debug_infos.hh"
#include "gpu_shader_math_constants_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(draw_debug_draw_display)

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

    /* Leave 0.1f boundary around dist to differentiate cleared or intentially blocked pixels. */
    return float4(sin_theta * 0.5f + 0.5f, dist * 0.4f + 0.5f, 0.0f, 1.0f);
  }
  else {
    /* Default line if the origin is perfectly aligned with a pixel. */
    return float4(0.0f, 0.5f, 0.0f, 1.0f);
  }
}

void main()
{
  out_color = final_color;
  out_line_data = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
}
