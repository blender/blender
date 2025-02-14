/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

#ifdef GPU_METAL
  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 5e-5;
#endif

  bool is_select = (nor.w > 0.0);
  bool is_hidden = (nor.w < 0.0);

  /* Don't draw faces that are selected. */
  if (is_hidden || is_select) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }
  else {
    view_clipping_distances(world_pos);
  }
}
