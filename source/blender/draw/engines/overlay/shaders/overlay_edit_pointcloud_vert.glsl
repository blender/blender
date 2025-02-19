/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  gl_PointSize = sizeVertex * 2.0;

  if ((data & VERT_SELECTED) != 0u) {
    finalColor = colorVertexSelect;
  }
  else {
    gl_PointSize = 0.0;
  }

  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Small offset in Z */
  gl_Position.z -= 3e-4;

  view_clipping_distances(world_pos);
}
