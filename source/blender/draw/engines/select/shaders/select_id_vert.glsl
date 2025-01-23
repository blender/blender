/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"

void main()
{
#ifndef UNIFORM_ID
  select_id = offset + index;
#endif

  vec3 world_pos = drw_point_object_to_world(pos);
  vec3 view_pos = drw_point_world_to_view(world_pos);
  gl_Position = drw_point_view_to_homogenous(view_pos);
  gl_PointSize = sizeVertex;

  /* Offset Z position for retopology selection occlusion. */
  gl_Position.z += get_homogenous_z_offset(
      ProjectionMatrix, view_pos.z, gl_Position.w, retopologyOffset);

  view_clipping_distances(world_pos);
}
