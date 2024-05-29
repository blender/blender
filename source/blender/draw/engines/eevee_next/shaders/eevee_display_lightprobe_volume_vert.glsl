/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)

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

  lP = pos[gl_VertexID % 6];
  int cell_index = gl_VertexID / 6;

  ivec3 grid_res = grid_resolution;

  cell = ivec3(cell_index / (grid_res.z * grid_res.y),
               (cell_index / grid_res.z) % grid_res.y,
               cell_index % grid_res.z);

  vec3 ws_cell_pos = lightprobe_volume_grid_sample_position(grid_to_world, grid_res, cell);

  float sphere_radius_final = sphere_radius;
  if (display_validity) {
    float validity = texelFetch(validity_tx, cell, 0).r;
    sphere_radius_final *= mix(1.0, 0.1, validity);
  }

  vec3 vs_offset = vec3(lP, 0.0) * sphere_radius_final;
  vec3 vP = drw_point_world_to_view(ws_cell_pos) + vs_offset;

  gl_Position = drw_point_view_to_homogenous(vP);
  /* Small bias to let the icon draw without Z-fighting. */
  gl_Position.z += 0.0001;
}
