/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_extra_point_base)
VERTEX_SHADER_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_VertexID]);

  float3 world_pos = drw_point_object_to_world(data_buf[gl_VertexID].pos_.xyz);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  gl_PointSize = theme.sizes.object_center;
  float radius = 0.5f * theme.sizes.object_center;
  float outline_width = theme.sizes.pixel;
  radii[0] = radius;
  radii[1] = radius - 1.0f;
  radii[2] = radius - outline_width;
  radii[3] = radius - outline_width - 1.0f;
  radii /= theme.sizes.object_center;

  fill_color = data_buf[gl_VertexID].color_;
  outline_color = theme.colors.outline;

#ifdef SELECT_ENABLE
  /* Selection frame-buffer can be very small.
   * Make sure to only rasterize one pixel to avoid making the selection radius very big. */
  gl_PointSize = 1.0f;
#endif

  view_clipping_distances(world_pos);
}
