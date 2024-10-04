/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"

bool test_occlusion()
{
  vec3 ndc = (gl_Position.xyz / gl_Position.w) * 0.5 + 0.5;
  return (ndc.z - 0.00035) > texture(depthTex, ndc.xy).r;
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  /* Avoid undefined behavior after return. */
  finalColor = vec4(0.0);
  gl_Position = vec4(0.0);

#if defined(FACE_NORMAL) || defined(VERT_NORMAL) || defined(LOOP_NORMAL)
  /* Point primitive. */
  const uint input_primitive_vertex_count = 1u;
  /* Line list primitive. */
  const uint ouput_primitive_vertex_count = 2u;
  const uint ouput_primitive_count = 1u;
  const uint ouput_invocation_count = 1u;

  const uint output_vertex_count_per_invocation = ouput_primitive_count *
                                                  ouput_primitive_vertex_count;
  const uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                       ouput_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(gl_VertexID) % ouput_primitive_vertex_count;
  uint out_primitive_id = (uint(gl_VertexID) / ouput_primitive_vertex_count) %
                          ouput_primitive_count;
  uint out_invocation_id = (uint(gl_VertexID) / output_vertex_count_per_invocation) %
                           ouput_invocation_count;

  uint vert_i = gpu_index_load(in_primitive_first_vertex);

  vec3 ls_pos = gpu_attr_load_float3(pos, gpu_attr_1, vert_i);
#endif

  vec3 nor;
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

  finalColor = colorNormal;

#elif defined(VERT_NORMAL)
  nor = gpu_attr_load_uint_1010102_snorm(vnor, gpu_attr_0, vert_i).xyz;
  finalColor = colorVNormal;

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
  finalColor = colorLNormal;

#else

  /* Select the right normal by checking if the generic attribute is used. */
  if (!all(equal(lnor.xyz, vec3(0)))) {
    if (lnor.w < 0.0) {
      return;
    }
    nor = lnor.xyz;
    finalColor = colorLNormal;
  }
  else if (!all(equal(vnor.xyz, vec3(0)))) {
    if (vnor.w < 0.0) {
      return;
    }
    nor = vnor.xyz;
    finalColor = colorVNormal;
  }
  else {
    nor = norAndFlag.xyz;
    if (all(equal(nor, vec3(0)))) {
      return;
    }
    finalColor = colorNormal;
  }
  vec3 ls_pos = pos;
#endif

  vec3 n = normalize(normal_object_to_world(nor));
  vec3 world_pos = point_object_to_world(ls_pos);

  if ((gl_VertexID & 1) == 0) {
    if (isConstantScreenSizeNormals) {
      bool is_persp = (drw_view.winmat[3][3] == 0.0);
      if (is_persp) {
        float dist_fac = length(cameraPos - world_pos);
        float cos_fac = dot(cameraForward, cameraVec(world_pos));
        world_pos += n * normalScreenSize * dist_fac * cos_fac * pixelFac * sizePixel;
      }
      else {
        float frustrum_fac = mul_project_m4_v3_zfac(n) * sizePixel;
        world_pos += n * normalScreenSize * frustrum_fac;
      }
    }
    else {
      world_pos += n * normalSize;
    }
  }

  gl_Position = point_world_to_ndc(world_pos);

  finalColor.a *= (test_occlusion()) ? alpha : 1.0;

  view_clipping_distances(world_pos);
}
