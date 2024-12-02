/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

#ifdef GPU_LIBRARY_SHADER
#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

SHADER_LIBRARY_CREATE_INFO(draw_modelmat_new)
SHADER_LIBRARY_CREATE_INFO(draw_resource_handle_new)
SHADER_LIBRARY_CREATE_INFO(draw_pointcloud_new)

#include "draw_model_lib.glsl"
#include "draw_pointcloud_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Point Cloud
 *
 * Point Cloud objects loads attributes from buffers through sampler buffers.
 * \{ */

#ifdef OBINFO_LIB
vec3 attr_load_orco(vec4 orco)
{
  vec3 P = pointcloud_get_pos();
  vec3 lP = transform_point(ModelMatrixInverse, P);
  return OrcoTexCoFactors[0].xyz + lP * OrcoTexCoFactors[1].xyz;
}
#endif

vec4 attr_load_tangent(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
vec3 attr_load_uv(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec3(cd_buf);
}
vec4 attr_load_color(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
vec4 attr_load_vec4(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
vec3 attr_load_vec3(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec3(cd_buf);
}
vec2 attr_load_vec2(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec2(cd_buf);
}
float attr_load_float(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_float(cd_buf);
}

/** \} */
