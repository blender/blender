/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_uv_face_dots)

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  float3 world_pos = float3(au, 0.0f);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  final_color = ((flag & FACE_UV_SELECT) != 0u) ? theme.colors.facedot :
                                                  float4(theme.colors.wire.rgb, 1.0f);
  gl_PointSize = dot_size;
}
