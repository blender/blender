/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_particle_dot_base)

#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  float2 uv = gl_PointCoord - float2(0.5f);
  float dist = length(uv);

  if (dist > 0.5f) {
    gpu_discard_fragment();
    return;
  }
  /* Nice sphere falloff. */
  float intensity = sqrt(1.0f - dist * 2.0f) * 0.5f + 0.5f;
  frag_color = final_color * float4(intensity, intensity, intensity, 1.0f);

  /* The default value of GL_POINT_SPRITE_COORD_ORIGIN is GL_UPPER_LEFT. Need to reverse the Y. */
  uv.y = -uv.y;
  /* Subtract distance to outer edge of the circle. (0.75 is manually tweaked to look better) */
  float2 edge_pos = gl_FragCoord.xy - uv * (0.75f / (dist + 1e-9f));
  float2 edge_start = edge_pos + float2(-uv.y, uv.x);

  line_output = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);

  select_id_output(select_id);
}
