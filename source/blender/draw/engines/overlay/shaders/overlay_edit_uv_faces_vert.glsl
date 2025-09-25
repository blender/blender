/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_uv_faces)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  float3 world_pos = float3(au, 0.0f);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  bool is_selected = (flag & FACE_UV_SELECT) != 0u;
  bool is_active = (flag & FACE_UV_ACTIVE) != 0u;
  eObjectInfoFlag ob_flag = drw_object_infos().flag;
  bool is_object_active = flag_test(ob_flag, OBJECT_ACTIVE_EDIT_MODE);

  final_color = (is_selected) ? theme.colors.face_select : theme.colors.face;
  final_color = (is_active) ? theme.colors.edit_mesh_active : final_color;
  final_color.a *= is_object_active ? uv_opacity : (uv_opacity * 0.25f);
}
