/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* To be compiled with common_subdiv_lib.glsl */

layout(std430, binding = 1) readonly buffer inputCoarseData
{
  float coarse_stretch_area[];
};

layout(std430, binding = 2) writeonly buffer outputSubdivData
{
  float subdiv_stretch_area[];
};

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  uint start_loop_index = quad_index * 4;

  uint coarse_quad_index = coarse_face_index_from_subdiv_quad_index(quad_index, coarse_face_count);

  for (int i = 0; i < 4; i++) {
    subdiv_stretch_area[start_loop_index + i] = coarse_stretch_area[coarse_quad_index];
  }
}
