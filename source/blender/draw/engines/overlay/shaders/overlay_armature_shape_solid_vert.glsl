/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_shape_solid)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  float4 bone_color, state_color;
  float4x4 inst_obmat = data_buf[gl_InstanceID];
  float4x4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  float3x3 normal_mat = transpose(inverse(to_float3x3(model_mat)));
  float3 normal = normalize(drw_normal_world_to_view(normal_mat * nor));

  inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0f);

  /* Do lighting at an angle to avoid flat shading on front facing bone. */
  constexpr float3 light = float3(0.1f, 0.1f, 0.8f);
  float n = dot(normal, light);

  /* Smooth lighting factor. */
  constexpr float s = 0.2f; /* [0.0f-0.5f] range */
  float fac = clamp((n * (1.0f - s)) + s, 0.0f, 1.0f);
  final_color.rgb = mix(state_color.rgb, bone_color.rgb, fac * fac);
  final_color.a = 1.0f;

  float4 world_pos = model_mat * float4(pos, 1.0f);
  gl_Position = drw_view().winmat * (drw_view().viewmat * world_pos);

  view_clipping_distances(world_pos.xyz);
}
