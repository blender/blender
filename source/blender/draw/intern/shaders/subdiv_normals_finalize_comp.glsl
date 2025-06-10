/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Finalize normals after accumulating or interpolation.
 *
 * Custom normals are interpolated from coarse face corners to subdivided face corners
 * by the generic custom data interpolation process. This shader is necessary to combine
 * them with the interleaved flag in the final buffer.
 *
 * TODO: Move the currently interleaved flag to a separate buffer so this is unnecessary.
 */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_custom_normals_finalize)

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
    return -1.0;
  }
  if (is_face_selected(coarse_quad_index)) {
    return 1.0;
  }
  return 0.0;
}

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint coarse_quad_index = coarse_face_index_from_subdiv_quad_index(quad_index,
                                                                    shader_data.coarse_face_count);

  uint start_loop_index = quad_index * 4;

  for (int i = 0; i < 4; i++) {
    Normal custom_normal = custom_normals[start_loop_index + i];
    float3 nor = float3(custom_normal.x, custom_normal.y, custom_normal.z);
    nor = normalize(nor);

    LoopNormal lnor;
    lnor.nx = nor.x;
    lnor.ny = nor.y;
    lnor.nz = nor.z;

    int origindex = input_vert_origindex[start_loop_index + i];
    lnor.flag = get_loop_flag(coarse_quad_index, origindex);

    output_lnor[start_loop_index + i] = lnor;
  }
}
