/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_sculpt_data)

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    SculptData data;
    data.face_set_color = sculpt_face_set_color[loop_index];

    if (shader_data.has_sculpt_mask) {
      data.mask = sculpt_mask[loop_index];
    }
    else {
      data.mask = 0.0f;
    }

    sculpt_data[loop_index] = data;
  }
}
