/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_particle_dot)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(drw_custom_id());

  /* Draw-size packed in alpha. */
  float draw_size = ucolor.a;

  float3 world_pos = part_pos;

  gl_Position = drw_point_world_to_homogenous(world_pos);
  /* World sized points. */
  gl_PointSize = draw_size * drw_view().winmat[1][1] * uniform_buf.size_viewport.y / gl_Position.w;

  /* Coloring */
  if (part_val < 0.0f) {
    final_color = float4(ucolor.rgb, 1.0f);
  }
  else {
    final_color = float4(texture(weight_tx, part_val).rgb, 1.0f);
  }

  view_clipping_distances(world_pos);
}
