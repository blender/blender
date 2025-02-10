/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  if ((data & VERT_SELECTED) != 0u) {
    finalColor = colorVertexSelect;
  }
  else if ((data & VERT_ACTIVE) != 0u) {
    finalColor = colorEditMeshActive;
  }
  else {
    finalColor = colorVertex;
  }

  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Small offset in Z */
  gl_Position.z -= 3e-4;

  gl_PointSize = sizeVertex * 2.0;

  view_clipping_distances(world_pos);
}
