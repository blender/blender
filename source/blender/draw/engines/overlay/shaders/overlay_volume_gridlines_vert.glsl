/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_volume_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_volume_gridlines_range)

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "select_lib.glsl"

float4 flag_to_color(uint flag)
{
  /* Color mapping for flags */
  float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  /* Cell types: 1 is Fluid, 2 is Obstacle, 4 is Empty, 8 is Inflow, 16 is Outflow */
  if (bool(flag & uint(1))) {
    color.rgb += float3(0.0f, 0.0f, 0.75f); /* blue */
  }
  if (bool(flag & uint(2))) {
    color.rgb += float3(0.4f, 0.4f, 0.4f); /* gray */
  }
  if (bool(flag & uint(4))) {
    color.rgb += float3(0.25f, 0.0f, 0.2f); /* dark purple */
  }
  if (bool(flag & uint(8))) {
    color.rgb += float3(0.0f, 0.5f, 0.0f); /* dark green */
  }
  if (bool(flag & uint(16))) {
    color.rgb += float3(0.9f, 0.3f, 0.0f); /* orange */
  }
  if (is_zero(color.rgb)) {
    color.rgb += float3(0.5f, 0.0f, 0.0f); /* medium red */
  }
  return color;
}

void main()
{
  select_id_set(in_select_id);

  int cell = gl_VertexID / 8;
  float3x3 rot_mat = float3x3(0.0f);

  float3 cell_offset = float3(0.5f);
  int3 cell_div = volumeSize;
  if (sliceAxis == 0) {
    cell_offset.x = slicePosition * float(volumeSize.x);
    cell_div.x = 1;
    rot_mat[2].x = 1.0f;
    rot_mat[0].y = 1.0f;
    rot_mat[1].z = 1.0f;
  }
  else if (sliceAxis == 1) {
    cell_offset.y = slicePosition * float(volumeSize.y);
    cell_div.y = 1;
    rot_mat[1].x = 1.0f;
    rot_mat[2].y = 1.0f;
    rot_mat[0].z = 1.0f;
  }
  else if (sliceAxis == 2) {
    cell_offset.z = slicePosition * float(volumeSize.z);
    cell_div.z = 1;
    rot_mat[0].x = 1.0f;
    rot_mat[1].y = 1.0f;
    rot_mat[2].z = 1.0f;
  }

  int3 cell_co;
  cell_co.x = cell % cell_div.x;
  cell_co.y = (cell / cell_div.x) % cell_div.y;
  cell_co.z = cell / (cell_div.x * cell_div.y);

  finalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

#if defined(SHOW_FLAGS) || defined(SHOW_RANGE)
  uint flag = texelFetch(flagTexture, cell_co + int3(cell_offset), 0).r;
#endif

#ifdef SHOW_FLAGS
  finalColor = flag_to_color(flag);
#endif

#ifdef SHOW_RANGE
  float value = texelFetch(fieldTexture, cell_co + int3(cell_offset), 0).r;
  if (value >= lowerBound && value <= upperBound) {
    if (cellFilter == 0 || bool(uint(cellFilter) & flag)) {
      finalColor = rangeColor;
    }
  }
#endif
  /* NOTE(Metal): Declaring constant arrays in function scope to avoid increasing local shader
   * memory pressure. */
  const int indices[8] = int_array(0, 1, 1, 2, 2, 3, 3, 0);

  /* Corners for cell outlines. 0.45 is arbitrary. Any value below 0.5f can be used to avoid
   * overlapping of the outlines. */
  const float3 corners[4] = float3_array(float3(-0.45f, 0.45f, 0.0f),
                                         float3(0.45f, 0.45f, 0.0f),
                                         float3(0.45f, -0.45f, 0.0f),
                                         float3(-0.45f, -0.45f, 0.0f));

  float3 pos = domainOriginOffset +
               cellSize * (float3(cell_co + adaptiveCellOffset) + cell_offset);
  float3 rotated_pos = rot_mat * corners[indices[gl_VertexID % 8]];
  pos += rotated_pos * cellSize;

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
}
