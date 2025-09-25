/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_edit_mesh_edge)

bool test_occlusion()
{
  return gl_FragCoord.z > texelFetch(depth_tx, int2(gl_FragCoord.xy), 0).r;
}

float edge_step(float dist)
{
  if (do_smooth_wire) {
    return smoothstep(LINE_SMOOTH_START, LINE_SMOOTH_END, dist);
  }
  else {
    return step(0.5f, dist);
  }
}

void main()
{
  float dist = abs(geometry_noperspective_out.edge_coord) - max(theme.sizes.edge - 0.5f, 0.0f);
  float dist_outer = dist - max(theme.sizes.edge, 1.0f);
  float mix_w = edge_step(dist);
  float mix_w_outer = edge_step(dist_outer);
  /* Line color & alpha. */
  frag_color = mix(geometry_flat_out.final_color_outer,
                   geometry_out.final_color,
                   1.0f - mix_w * geometry_flat_out.final_color_outer.a);
  /* Line edges shape. */
  frag_color.a *= 1.0f - (geometry_flat_out.final_color_outer.a > 0.0f ? mix_w_outer : mix_w);

  frag_color.a *= test_occlusion() ? alpha : 1.0f;
  line_output = float4(0.0f);
}
