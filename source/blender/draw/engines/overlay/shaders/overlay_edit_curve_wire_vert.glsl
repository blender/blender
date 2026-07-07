/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_curve_wire)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  float3 final_pos = pos;

  float flip = (gl_InstanceID != 0) ? -1.0f : 1.0f;

  if (gl_VertexID % 2 == 0) {
    final_pos += normal_size * rad * (flip * nor - tangent);
  }

  float3 world_pos = drw_point_object_to_world(final_pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  edge_start = edge_pos = ((gl_Position.xy / gl_Position.w) * 0.5f + 0.5f) *
                          uniform_buf.size_viewport;

  final_color = theme.colors.wire_edit;

  view_clipping_distances(world_pos);
}
