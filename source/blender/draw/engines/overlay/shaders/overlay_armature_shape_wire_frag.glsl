/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_shape_wire)

#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

/**
 * We want to know how much a pixel is covered by a line.
 * We replace the square pixel with a circle of the same area and try to find the intersection
 * area. The area we search is the circular segment. https://en.wikipedia.org/wiki/Circular_segment
 * The formula for the area uses inverse trig function and is quite complex. Instead,
 * we approximate it by using the smooth-step function and a 1.05 factor to the disc radius.
 */

#define M_1_SQRTPI 0.5641895835477563f /* `1/sqrt(pi)`. */

#define DISC_RADIUS (M_1_SQRTPI * 1.05f)
#define GRID_LINE_SMOOTH_START (0.5f - DISC_RADIUS)
#define GRID_LINE_SMOOTH_END (0.5f + DISC_RADIUS)

float edge_step(float dist)
{
  if (do_smooth_wire) {
    return smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist);
  }
  else {
    return step(0.5f, dist);
  }
}

void main()
{
  float half_size = (do_smooth_wire ? wire_width - 0.5f : wire_width) / 2.0f;

  float dist = abs(edgeCoord) - half_size;
  float mix_w = saturate(edge_step(dist));

  fragColor = mix(float4(finalColor.rgb, alpha), float4(0), mix_w);
  fragColor.a *= 1.0f - mix_w;
  lineOutput = float4(0);

  select_id_output(select_id);
}
