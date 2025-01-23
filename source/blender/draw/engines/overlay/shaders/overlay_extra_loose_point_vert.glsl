/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  finalColor = data_buf[gl_VertexID].color_;

  vec3 world_pos = (ModelMatrix * vec4(data_buf[gl_VertexID].pos_.xyz, 1.0)).xyz;
  gl_Position = drw_point_world_to_homogenous(world_pos);

  gl_PointSize = sizeVertex * 2.0;

  view_clipping_distances(world_pos);
}
