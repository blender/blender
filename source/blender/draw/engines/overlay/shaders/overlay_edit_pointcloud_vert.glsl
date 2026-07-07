/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  final_color = theme.colors.vert_select;

  float radius = pos_rad.w;
  float3 world_pos = drw_point_object_to_world(pos_rad.xyz);
  float3 V = drw_world_incident_vector(world_pos);

  /* Offset the position so the selection point is always
   * drawn in from of the point, regardless of the radius. */
  world_pos += V * radius;
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Small offset in Z for depth precision. */
  gl_Position.z -= 3e-4f;

  gl_PointSize = theme.sizes.vert * 2.0f;
  view_clipping_distances(world_pos);
}
