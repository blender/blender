/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_paint_overlay_flag)

bool is_face_selected(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_select_mask) != 0;
}

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_hidden_mask) != 0;
}

/* Flag for paint mode overlay and normals drawing in edit-mode. */
int get_loop_flag(uint coarse_quad_index, int vert_origindex)
{
  if (is_face_hidden(coarse_quad_index) || (shader_data.is_edit_mode && vert_origindex == -1)) {
    return -1;
  }
  if (is_face_selected(coarse_quad_index)) {
    return 1;
  }
  return 0;
}

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  uint start_loop_index = quad_index * 4;

  uint coarse_quad_index = coarse_face_index_from_subdiv_quad_index(quad_index,
                                                                    shader_data.coarse_face_count);
  for (int i = 0; i < 4; i++) {
    int origindex = input_vert_origindex[start_loop_index + i];
    int flag = get_loop_flag(coarse_quad_index, origindex);

    flags[start_loop_index + i] = flag;
  }
}
