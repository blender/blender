/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/gpu_shader_sequencer_info.hh"

SHADER_LIBRARY_CREATE_INFO(gpu_shader_sequencer_strips)

/* Signed distance to rounded box, centered at origin.
 * Reference: https://iquilezles.org/articles/distfunctions2d/ */
float sdf_rounded_box(vec2 pos, vec2 size, float radius)
{
  vec2 q = abs(pos) - size + radius;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void strip_box(float left,
               float right,
               float bottom,
               float top,
               vec2 pos,
               out vec2 r_pos1,
               out vec2 r_pos2,
               out vec2 r_size,
               out vec2 r_center,
               out vec2 r_pos,
               out float r_radius)
{
  /* Snap to pixel grid coordinates, so that outline/border is non-fractional
   * pixel sizes. */
  r_pos1 = round(vec2(left, bottom));
  r_pos2 = round(vec2(right, top));
  /* Make sure strip is at least 1px wide. */
  r_pos2.x = max(r_pos2.x, r_pos1.x + 1.0);
  r_size = (r_pos2 - r_pos1) * 0.5;
  r_center = (r_pos1 + r_pos2) * 0.5;
  r_pos = round(pos);

  r_radius = context_data.round_radius;
  if (r_radius > r_size.x) {
    r_radius = 0.0;
  }
}
