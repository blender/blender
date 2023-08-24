/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_pointcloud_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos, world_nor;
  float world_radius;
  pointcloud_get_pos_nor_radius(world_pos, world_nor, world_radius);

  gl_Position = point_world_to_ndc(world_pos);

#ifdef CONSERVATIVE_RASTER
  /* Avoid expense of geometry shader by ensuring rastered pointcloud primitive
   * covers at least a whole pixel. */
  int i = gl_VertexID % 3;
  vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(2.0, -1.0) : vec2(-1.0, 2.0));
  gl_Position.xy += sizeViewportInv * gl_Position.w * ofs;
#endif

  view_clipping_distances(world_pos);
}
