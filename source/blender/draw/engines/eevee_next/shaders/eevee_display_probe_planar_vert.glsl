/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)

void main()
{
  /* Constant array moved inside function scope.
   * Minimizes local register allocation in MSL. */
  const vec2 pos[6] = vec2[6](vec2(-1.0, -1.0),
                              vec2(1.0, -1.0),
                              vec2(-1.0, 1.0),

                              vec2(1.0, -1.0),
                              vec2(1.0, 1.0),
                              vec2(-1.0, 1.0));

  vec2 lP = pos[gl_VertexID % 6];
  int display_index = gl_VertexID / 6;

  probe_index = display_data_buf[display_index].probe_index;

  mat4 plane_to_world = display_data_buf[display_index].plane_to_world;
  probe_normal = safe_normalize(plane_to_world[2].xyz);

  vec3 P = transform_point(plane_to_world, vec3(lP, 0.0));
  gl_Position = drw_point_world_to_homogenous(P);
  /* Small bias to let the probe draw without Z-fighting. */
  gl_Position.z -= 0.0001;
}
