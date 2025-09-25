/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_extra_groundline)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);
  final_color = theme.colors.light;

  /* Relative to DPI scaling. Have constant screen size. */
  float3 screen_pos = drw_view().viewinv[0].xyz * pos.x + drw_view().viewinv[1].xyz * pos.y;
  float3 inst_pos = data_buf[gl_InstanceID].xyz;
  float3 p = inst_pos;
  p.z *= (pos.z == 0.0f) ? 0.0f : 1.0f;
  float screen_size = mul_project_m4_v3_zfac(uniform_buf.pixel_fac, p) * theme.sizes.pixel;
  float3 world_pos = p + screen_pos * screen_size;

  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Convert to screen position [0..sizeVp]. */
  edge_pos = edge_start = ((gl_Position.xy / gl_Position.w) * 0.5f + 0.5f) *
                          uniform_buf.size_viewport;

  view_clipping_distances(world_pos);
}
