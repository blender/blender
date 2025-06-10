/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_loop_normals)

bool is_face_selected(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_select_mask) != 0;
}

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_hidden_mask) != 0;
}

/* Flag for paint mode overlay and normals drawing in edit-mode. */
float get_loop_flag(uint coarse_quad_index, int vert_origindex)
{
  if (is_face_hidden(coarse_quad_index) || (shader_data.is_edit_mode && vert_origindex == -1)) {
    return -1.0f;
  }
  if (is_face_selected(coarse_quad_index)) {
    return 1.0f;
  }
  return 0.0f;
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

  if ((extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_smooth_mask) != 0) {
    /* Face is smooth, use vertex normals. */
    for (int i = 0; i < 4; i++) {
      uint subdiv_vert_index = vert_loop_map[start_loop_index + i];
      Normal vert_normal = vert_normals[subdiv_vert_index];

      int origindex = input_vert_origindex[start_loop_index + i];
      float flag = get_loop_flag(coarse_quad_index, origindex);

      LoopNormal loop_normal;
      loop_normal.nx = vert_normal.x;
      loop_normal.ny = vert_normal.y;
      loop_normal.nz = vert_normal.z;
      loop_normal.flag = flag;

      output_lnor[start_loop_index + i] = loop_normal;
    }
  }
  else {
    Position pos_0 = positions[start_loop_index + 0];
    Position pos_1 = positions[start_loop_index + 1];
    Position pos_2 = positions[start_loop_index + 2];
    Position pos_3 = positions[start_loop_index + 3];
    float3 v0 = subdiv_position_to_float3(pos_0);
    float3 v1 = subdiv_position_to_float3(pos_1);
    float3 v2 = subdiv_position_to_float3(pos_2);
    float3 v3 = subdiv_position_to_float3(pos_3);

    float3 face_normal = float3(0.0f);
    add_newell_cross_v3_v3v3(face_normal, v0, v1);
    add_newell_cross_v3_v3v3(face_normal, v1, v2);
    add_newell_cross_v3_v3v3(face_normal, v2, v3);
    add_newell_cross_v3_v3v3(face_normal, v3, v0);

    face_normal = normalize(face_normal);

    LoopNormal loop_normal;
    loop_normal.nx = face_normal.x;
    loop_normal.ny = face_normal.y;
    loop_normal.nz = face_normal.z;

    for (int i = 0; i < 4; i++) {
      int origindex = input_vert_origindex[start_loop_index + i];
      loop_normal.flag = get_loop_flag(coarse_quad_index, origindex);

      output_lnor[start_loop_index + i] = loop_normal;
    }
  }
}
