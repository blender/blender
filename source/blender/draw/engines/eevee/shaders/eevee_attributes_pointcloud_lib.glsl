/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

#ifdef GPU_LIBRARY_SHADER
#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

SHADER_LIBRARY_CREATE_INFO(draw_modelmat)
SHADER_LIBRARY_CREATE_INFO(draw_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
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
float3 attr_load_orco(float4 orco)
{
  float3 P = pointcloud_get_pos();
  float3 lP = transform_point(drw_modelinv(), P);
  return drw_object_orco(lP);
}
#endif

float4 attr_load_tangent(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
float3 attr_load_uv(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec3(cd_buf);
}
float4 attr_load_color(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
float4 attr_load_vec4(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
float3 attr_load_vec3(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec3(cd_buf);
}
float2 attr_load_vec2(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_vec2(cd_buf);
}
float attr_load_float(samplerBuffer cd_buf)
{
  return pointcloud_get_customdata_float(cd_buf);
}

/** \} */
