/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_VertexID]);

  vec3 world_pos = drw_point_object_to_world(data_buf[gl_VertexID].pos_.xyz);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  gl_PointSize = sizeObjectCenter;
  float radius = 0.5 * sizeObjectCenter;
  float outline_width = sizePixel;
  radii[0] = radius;
  radii[1] = radius - 1.0;
  radii[2] = radius - outline_width;
  radii[3] = radius - outline_width - 1.0;
  radii /= sizeObjectCenter;

  fillColor = data_buf[gl_VertexID].color_;
  outlineColor = colorOutline;

#ifdef SELECT_ENABLE
  /* Selection frame-buffer can be very small.
   * Make sure to only rasterize one pixel to avoid making the selection radius very big. */
  gl_PointSize = 1.0;
#endif

  view_clipping_distances(world_pos);
}
