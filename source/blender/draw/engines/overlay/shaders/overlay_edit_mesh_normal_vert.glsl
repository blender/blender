/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_mesh_loop_normal)
#ifdef GLSL_CPP_STUBS
#  define LOOP_NORMAL
#endif

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "overlay_common_lib.glsl"

bool test_occlusion()
{
  float3 ndc = (gl_Position.xyz / gl_Position.w) * 0.5f + 0.5f;
  return (ndc.z - 0.00035f) > texture(depth_tx, ndc.xy).r;
}

void main()
{
  /* Avoid undefined behavior after return. */
  final_color = float4(0.0f);
  gl_Position = float4(0.0f);

#if defined(FACE_NORMAL) || defined(VERT_NORMAL) || defined(LOOP_NORMAL)
  /* Point primitive. */
  constexpr uint input_primitive_vertex_count = 1u;
  /* Line list primitive. */
  constexpr uint output_primitive_vertex_count = 2u;
  constexpr uint output_primitive_count = 1u;
  constexpr uint output_invocation_count = 1u;

  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint vert_i = gpu_index_load(in_primitive_first_vertex);

  float3 ls_pos = gpu_attr_load_float3(pos, gpu_attr_1, vert_i);
#endif

  float3 nor;
#if defined(FACE_NORMAL)
#  if defined(FLOAT_NORMAL)
  /* Path for opensubdiv. To be phased out at some point. */
  nor = norAndFlag[vert_i].xyz;
#  else
  if (hq_normals) {
    nor = gpu_attr_load_short4_snorm(norAndFlag, gpu_attr_0, vert_i).xyz;
  }
  else {
    nor = gpu_attr_load_uint_1010102_snorm(norAndFlag, gpu_attr_0, vert_i).xyz;
  }
#  endif

  final_color = theme.colors.normal;

#elif defined(VERT_NORMAL)
#  if defined(FLOAT_NORMAL)
  /* Path for opensubdiv. To be phased out at some point. */
  nor = gpu_attr_load_float3(vnor, gpu_attr_0, vert_i);
#  else
  nor = gpu_attr_load_uint_1010102_snorm(vnor, gpu_attr_0, vert_i).xyz;
#  endif
  final_color = theme.colors.vnormal;

#elif defined(LOOP_NORMAL)
#  if defined(FLOAT_NORMAL)
  /* Path for opensubdiv. To be phased out at some point. */
  nor = lnor[vert_i].xyz;
#  else
  if (hq_normals) {
    nor = gpu_attr_load_short4_snorm(lnor, gpu_attr_0, vert_i).xyz;
  }
  else {
    nor = gpu_attr_load_uint_1010102_snorm(lnor, gpu_attr_0, vert_i).xyz;
  }
#  endif
  final_color = theme.colors.lnormal;

#else

  /* Select the right normal by checking if the generic attribute is used. */
  if (!all(equal(lnor.xyz, float3(0)))) {
    if (lnor.w < 0.0f) {
      return;
    }
    nor = lnor.xyz;
    final_color = theme.colors.lnormal;
  }
  else if (!all(equal(vnor.xyz, float3(0)))) {
    if (vnor.w < 0.0f) {
      return;
    }
    nor = vnor.xyz;
    final_color = theme.colors.vnormal;
  }
  else {
    nor = norAndFlag.xyz;
    if (all(equal(nor, float3(0)))) {
      return;
    }
    final_color = theme.colors.normal;
  }
  float3 ls_pos = pos;
#endif

  float3 n = normalize(drw_normal_object_to_world(nor));
  float3 world_pos = drw_point_object_to_world(ls_pos);

  if ((gl_VertexID & 1) == 0) {
    if (is_constant_screen_size_normals) {
      bool is_persp = (drw_view().winmat[3][3] == 0.0f);
      if (is_persp) {
        float dist_fac = length(drw_view_position() - world_pos);
        float cos_fac = dot(drw_view_forward(), drw_world_incident_vector(world_pos));
        world_pos += n * normal_screen_size * dist_fac * cos_fac * uniform_buf.pixel_fac *
                     theme.sizes.pixel;
      }
      else {
        float frustrum_fac = mul_project_m4_v3_zfac(uniform_buf.pixel_fac, n) * theme.sizes.pixel;
        world_pos += n * normal_screen_size * frustrum_fac;
      }
    }
    else {
      world_pos += n * normal_size;
    }
  }

  gl_Position = drw_point_world_to_homogenous(world_pos);

  final_color.a *= (test_occlusion()) ? alpha : 1.0f;

  view_clipping_distances(world_pos);
}
