/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_pointcloud_lib.glsl"
#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(drw_CustomID);

  vec3 world_pos, world_nor;
  float world_radius;
  pointcloud_get_pos_nor_radius(world_pos, world_nor, world_radius);

  gl_Position = drw_point_world_to_homogenous(world_pos);

#ifdef CONSERVATIVE_RASTER
  /* Avoid expense of geometry shader by ensuring rastered point-cloud primitive
   * covers at least a whole pixel. */
  int i = gl_VertexID % 3;
  vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(2.0, -1.0) : vec2(-1.0, 2.0));
  gl_Position.xy += sizeViewportInv * gl_Position.w * ofs;
#endif

  view_clipping_distances(world_pos);
}
