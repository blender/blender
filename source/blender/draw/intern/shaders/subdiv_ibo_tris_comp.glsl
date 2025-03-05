/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Generate triangle indices from subdivision quads. */

#include "subdiv_lib.glsl"

#ifdef SINGLE_MATERIAL
COMPUTE_SHADER_CREATE_INFO(subdiv_tris_single_material)
#else
COMPUTE_SHADER_CREATE_INFO(subdiv_tris_multiple_materials)
#endif

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_hidden_mask) != 0;
}

void main()
{
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint loop_index = quad_index * 4;
  uint coarse_quad_index = coarse_face_index_from_subdiv_quad_index(quad_index,
                                                                    shader_data.coarse_face_count);

#ifdef SINGLE_MATERIAL
  uint triangle_loop_index = quad_index * 6;
#else
  uint mat_offset = face_mat_offset[coarse_quad_index];
  uint triangle_loop_index = (quad_index + mat_offset) * 6;
#endif

  if (shader_data.use_hide && is_face_hidden(coarse_quad_index)) {
    output_tris[triangle_loop_index + 0] = 0xffffffff;
    output_tris[triangle_loop_index + 1] = 0xffffffff;
    output_tris[triangle_loop_index + 2] = 0xffffffff;
    output_tris[triangle_loop_index + 3] = 0xffffffff;
    output_tris[triangle_loop_index + 4] = 0xffffffff;
    output_tris[triangle_loop_index + 5] = 0xffffffff;
  }
  else {
    output_tris[triangle_loop_index + 0] = loop_index + 0;
    output_tris[triangle_loop_index + 1] = loop_index + 1;
    output_tris[triangle_loop_index + 2] = loop_index + 2;
    output_tris[triangle_loop_index + 3] = loop_index + 0;
    output_tris[triangle_loop_index + 4] = loop_index + 2;
    output_tris[triangle_loop_index + 5] = loop_index + 3;
  }
}
