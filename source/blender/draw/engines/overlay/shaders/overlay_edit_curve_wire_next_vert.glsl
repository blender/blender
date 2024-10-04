/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"

void main()
{
  /* Line list primitive. */
  const uint input_primitive_vertex_count = 2u;
  /* Line list primitive. */
  const uint ouput_primitive_vertex_count = 2u;
  const uint ouput_primitive_count = 2u;
  const uint ouput_invocation_count = 1u;
  const uint output_vertex_count_per_invocation = ouput_primitive_count *
                                                  ouput_primitive_vertex_count;
  const uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                       ouput_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint v_i = gpu_index_load(in_primitive_first_vertex);
  vec3 ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  float radius = rad[gpu_attr_load_index(v_i, gpu_attr_1)];
  vec3 ls_N = use_hq_normals ? gpu_attr_load_short4_snorm(nor, gpu_attr_2, v_i).xyz :
                               gpu_attr_load_uint_1010102_snorm(nor, gpu_attr_2, v_i).xyz;
  vec3 ls_T = use_hq_normals ? gpu_attr_load_short4_snorm(tan, gpu_attr_3, v_i).xyz :
                               gpu_attr_load_uint_1010102_snorm(tan, gpu_attr_3, v_i).xyz;

  if ((gl_VertexID & 1) == 0) {
    float flip = ((gl_VertexID & 2) == 0) ? -1.0 : 1.0;
    ls_P += normalSize * radius * (flip * ls_N - ls_T);
  }

  vec3 world_pos = point_object_to_world(ls_P);
  gl_Position = point_world_to_ndc(world_pos);

  finalColor = colorWireEdit;

  view_clipping_distances(world_pos);
}
