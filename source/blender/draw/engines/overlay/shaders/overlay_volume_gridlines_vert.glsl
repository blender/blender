/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

vec4 flag_to_color(uint flag)
{
  /* Color mapping for flags */
  vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
  /* Cell types: 1 is Fluid, 2 is Obstacle, 4 is Empty, 8 is Inflow, 16 is Outflow */
  if (bool(flag & uint(1))) {
    color.rgb += vec3(0.0, 0.0, 0.75); /* blue */
  }
  if (bool(flag & uint(2))) {
    color.rgb += vec3(0.4, 0.4, 0.4); /* gray */
  }
  if (bool(flag & uint(4))) {
    color.rgb += vec3(0.25, 0.0, 0.2); /* dark purple */
  }
  if (bool(flag & uint(8))) {
    color.rgb += vec3(0.0, 0.5, 0.0); /* dark green */
  }
  if (bool(flag & uint(16))) {
    color.rgb += vec3(0.9, 0.3, 0.0); /* orange */
  }
  if (is_zero(color.rgb)) {
    color.rgb += vec3(0.5, 0.0, 0.0); /* medium red */
  }
  return color;
}

void main()
{
  int cell = gl_VertexID / 8;
  mat3 rot_mat = mat3(0.0);

  vec3 cell_offset = vec3(0.5);
  ivec3 cell_div = volumeSize;
  if (sliceAxis == 0) {
    cell_offset.x = slicePosition * float(volumeSize.x);
    cell_div.x = 1;
    rot_mat[2].x = 1.0;
    rot_mat[0].y = 1.0;
    rot_mat[1].z = 1.0;
  }
  else if (sliceAxis == 1) {
    cell_offset.y = slicePosition * float(volumeSize.y);
    cell_div.y = 1;
    rot_mat[1].x = 1.0;
    rot_mat[2].y = 1.0;
    rot_mat[0].z = 1.0;
  }
  else if (sliceAxis == 2) {
    cell_offset.z = slicePosition * float(volumeSize.z);
    cell_div.z = 1;
    rot_mat[0].x = 1.0;
    rot_mat[1].y = 1.0;
    rot_mat[2].z = 1.0;
  }

  ivec3 cell_co;
  cell_co.x = cell % cell_div.x;
  cell_co.y = (cell / cell_div.x) % cell_div.y;
  cell_co.z = cell / (cell_div.x * cell_div.y);

  finalColor = vec4(0.0, 0.0, 0.0, 1.0);

#if defined(SHOW_FLAGS) || defined(SHOW_RANGE)
  uint flag = texelFetch(flagTexture, cell_co + ivec3(cell_offset), 0).r;
#endif

#ifdef SHOW_FLAGS
  finalColor = flag_to_color(flag);
#endif

#ifdef SHOW_RANGE
  float value = texelFetch(fieldTexture, cell_co + ivec3(cell_offset), 0).r;
  if (value >= lowerBound && value <= upperBound) {
    if (cellFilter == 0 || bool(uint(cellFilter) & flag)) {
      finalColor = rangeColor;
    }
  }
#endif
  /* NOTE(Metal): Declaring constant arrays in function scope to avoid increasing local shader
   * memory pressure. */
  const int indices[8] = int[8](0, 1, 1, 2, 2, 3, 3, 0);

  /* Corners for cell outlines. 0.45 is arbitrary. Any value below 0.5 can be used to avoid
   * overlapping of the outlines. */
  const vec3 corners[4] = vec3[4](vec3(-0.45, 0.45, 0.0),
                                  vec3(0.45, 0.45, 0.0),
                                  vec3(0.45, -0.45, 0.0),
                                  vec3(-0.45, -0.45, 0.0));

  vec3 pos = domainOriginOffset + cellSize * (vec3(cell_co + adaptiveCellOffset) + cell_offset);
  vec3 rotated_pos = rot_mat * corners[indices[gl_VertexID % 8]];
  pos += rotated_pos * cellSize;

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
}
