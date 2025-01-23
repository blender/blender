/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec3 world_pos = vec3(au, 0.0);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  bool is_selected = (flag & FACE_UV_SELECT) != 0u;
  bool is_active = (flag & FACE_UV_ACTIVE) != 0u;

  finalColor = (is_selected) ? colorFaceSelect : colorFace;
  finalColor = (is_active) ? colorEditMeshActive : finalColor;
  finalColor.a *= uvOpacity;
}
