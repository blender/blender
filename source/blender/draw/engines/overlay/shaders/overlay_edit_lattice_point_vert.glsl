/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_lattice_point)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  if ((data & VERT_SELECTED) != 0u) {
    final_color = theme.colors.vert_select;
  }
  else if ((data & VERT_ACTIVE) != 0u) {
    final_color = theme.colors.edit_mesh_active;
  }
  else {
    final_color = theme.colors.vert;
  }

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Small offset in Z */
  gl_Position.z -= 3e-4f;

  gl_PointSize = theme.sizes.vert * 2.0f;

  view_clipping_distances(world_pos);
}
