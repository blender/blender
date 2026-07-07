/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_curve_normals)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"

void main()
{
  /* Line list primitive. */
  constexpr uint input_primitive_vertex_count = 2u;
  /* Line list primitive. */
  constexpr uint output_primitive_vertex_count = 2u;
  constexpr uint output_primitive_count = 2u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint v_i = gpu_index_load(in_primitive_first_vertex);
  float3 ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  float radius = rad[gpu_attr_load_index(v_i, gpu_attr_1)];
  float3 ls_N = use_hq_normals ? gpu_attr_load_short4_snorm(nor, gpu_attr_2, v_i).xyz :
                                 gpu_attr_load_uint_1010102_snorm(nor, gpu_attr_2, v_i).xyz;
  float3 ls_T = use_hq_normals ? gpu_attr_load_short4_snorm(tangent, gpu_attr_3, v_i).xyz :
                                 gpu_attr_load_uint_1010102_snorm(tangent, gpu_attr_3, v_i).xyz;

  if ((gl_VertexID & 1) == 0) {
    float flip = ((gl_VertexID & 2) == 0) ? -1.0f : 1.0f;
    ls_P += normal_size * radius * (flip * ls_N - ls_T);
  }

  float3 world_pos = drw_point_object_to_world(ls_P);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  final_color = theme.colors.wire_edit;

  view_clipping_distances(world_pos);
}
