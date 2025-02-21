/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Finalize normals after accumulating or interpolation.
 *
 * Normals are accumulated in the `subdiv_normals_accumulate_comp.glsl`, (custom) split normals are
 * interpolated as custom data layer in `subdiv_custom_data_interp_comp.glsl` using GPU_COMP_U16.
 */

#include "subdiv_lib.glsl"

#ifdef CUSTOM_NORMALS
COMPUTE_SHADER_CREATE_INFO(subdiv_custom_normals_finalize)
#else
COMPUTE_SHADER_CREATE_INFO(subdiv_normals_finalize)
#endif

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

#ifdef CUSTOM_NORMALS
  for (int i = 0; i < 4; i++) {
    CustomNormal custom_normal = custom_normals[start_loop_index + i];
    vec3 nor = vec3(custom_normal.x, custom_normal.y, custom_normal.z);
    PosNorLoop vertex_data = pos_nor[start_loop_index + i];
    pos_nor[start_loop_index + i] = subdiv_set_vertex_nor(vertex_data, normalize(nor));
  }
#else
  for (int i = 0; i < 4; i++) {
    uint subdiv_vert_index = vert_loop_map[start_loop_index + i];
    vec3 nor = vertex_normals[subdiv_vert_index];
    PosNorLoop vertex_data = pos_nor[start_loop_index + i];
    pos_nor[start_loop_index + i] = subdiv_set_vertex_nor(vertex_data, nor);
  }
#endif
}
