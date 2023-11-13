/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

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

  int cell_id = gl_VertexID / 6;
  int vert_id = gl_VertexID % 6;

  vec3 ls_cell_location;
  /* Keep in sync with update_irradiance_probe */
  ls_cell_location.z = float(cell_id % grid_resolution.z);
  ls_cell_location.y = float((cell_id / grid_resolution.z) % grid_resolution.y);
  ls_cell_location.x = float(cell_id / (grid_resolution.z * grid_resolution.y));

  cellOffset = offset + cell_id;

  vec3 ws_cell_location = corner +
                          (increment_x * ls_cell_location.x + increment_y * ls_cell_location.y +
                           increment_z * ls_cell_location.z);

  quadCoord = pos[vert_id];
  vec3 screen_pos = ViewMatrixInverse[0].xyz * quadCoord.x +
                    ViewMatrixInverse[1].xyz * quadCoord.y;
  ws_cell_location += screen_pos * sphere_size;

  gl_Position = ProjectionMatrix * (ViewMatrix * vec4(ws_cell_location, 1.0));
  gl_Position.z += 0.0001; /* Small bias to let the icon draw without Z-fighting. */
}
