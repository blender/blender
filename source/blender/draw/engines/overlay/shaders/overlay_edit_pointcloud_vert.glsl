/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  finalColor = colorVertexSelect;

  /* The radius is pre-multiplied. */
  float radius = pos_rad.w * 0.01;
  vec3 world_pos = drw_point_object_to_world(pos_rad.xyz);
  vec3 V = drw_world_incident_vector(world_pos);

  /* Offset the position so the selection point is always
   * drawn in from of the point, regardless of the radius. */
  world_pos += V * radius;
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Small offset in Z for depth precision. */
  gl_Position.z -= 3e-4;

  gl_PointSize = sizeVertex * 2.0;
  view_clipping_distances(world_pos);
}
