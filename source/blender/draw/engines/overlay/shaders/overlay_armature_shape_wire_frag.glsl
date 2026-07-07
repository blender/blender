/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_shape_wire)

#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

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
  float half_size = (do_smooth_wire ? wire_width - 0.5f : wire_width) / 2.0f;

  float dist = abs(edge_coord) - half_size;
  float mix_w = saturate(edge_step(dist));

  frag_color = mix(float4(final_color.rgb, alpha), float4(0), mix_w);
  frag_color.a *= 1.0f - mix_w;
  line_output = float4(0);

  select_id_output(select_id);
}
