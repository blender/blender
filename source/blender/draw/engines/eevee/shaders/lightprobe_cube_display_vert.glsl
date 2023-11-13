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

  pid = 1 + (gl_VertexID / 6); /* +1 for the world */
  int vert_id = gl_VertexID % 6;

  quadCoord = pos[vert_id];

  vec3 ws_location = probes_data[pid].position_type.xyz;
  vec3 screen_pos = ViewMatrixInverse[0].xyz * quadCoord.x +
                    ViewMatrixInverse[1].xyz * quadCoord.y;
  ws_location += screen_pos * sphere_size;

  gl_Position = ProjectionMatrix * (ViewMatrix * vec4(ws_location, 1.0));
  gl_Position.z += 0.0001; /* Small bias to let the icon draw without Z-fighting. */
}
