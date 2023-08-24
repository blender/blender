/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  float time, thick_time, thickness;
  vec3 world_pos, tan, binor;
  hair_get_pos_tan_binor_time(is_persp,
                              ModelMatrixInverse,
                              ViewMatrixInverse[3].xyz,
                              ViewMatrixInverse[2].xyz,
                              world_pos,
                              tan,
                              binor,
                              time,
                              thickness,
                              thick_time);

  gl_Position = point_world_to_ndc(world_pos);

  view_clipping_distances(world_pos);
}
