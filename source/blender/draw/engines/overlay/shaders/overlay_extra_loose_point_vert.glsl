/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_extra_loose_point_base)
VERTEX_SHADER_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  final_color = data_buf[gl_VertexID].color_;

  float3 world_pos = (drw_modelmat() * float4(data_buf[gl_VertexID].pos_.xyz, 1.0f)).xyz;
  gl_Position = drw_point_world_to_homogenous(world_pos);

  gl_PointSize = theme.sizes.vert * 2.0f;

  view_clipping_distances(world_pos);
}
