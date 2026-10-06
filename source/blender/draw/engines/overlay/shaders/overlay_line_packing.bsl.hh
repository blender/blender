/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_base.bsl.hh"

namespace overlay {

/**
 * Pack overlay line data to float4.
 * Note: edge_start, edge_pos need to be in the range [0..uniform_buf.size_viewport].
 * Note: returns float4 for FBO output; only the first two components store data.
 */
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

    /* Pack dist to [0.1, ..., 0.9], leaving 0.1 boundary for clear or blocked pixels. */
    return float4(sin_theta * 0.5f + 0.5f, dist * 0.4f + 0.5f, 0.0f, 1.0f);
  }
  /* Default line if the origin is perfectly aligned with a pixel. */
  return float4(0.0f, 0.5f, 0.0f, 1.0f);
}

/**
 * Pack overlay line data to float4, carrying a blocker value to indicate
 * a stage does its own AA in this pixel.
 */
float4 pack_line_data_no_aa()
{
  return float4(0.0f, 1.0f, 0.0f, 1.0f);
}

/**
 * Unpack overlay line data, recovering perpendicular vector direction and signed distance.
 */
void unpack_line_data(float2 data, float2 &perp, float &dist)
{
  /* Unpack distance to edge, remove 0.1 boundary around value. */
  dist = (data.y - 0.5f) * 2.5f;

  /* Recover perpendicular vector from packed sin_theta. */
  float sin_theta = (data.x - 0.5f) * 2.0f;
  float cos_theta = cos_from_sin(sin_theta);
  perp = normalize(float2(sin_theta, cos_theta));
}

}  // namespace overlay
