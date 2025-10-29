/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#ifdef GPU_LIBRARY_SHADER
#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

SHADER_LIBRARY_CREATE_INFO(draw_modelmat)
SHADER_LIBRARY_CREATE_INFO(draw_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_pointcloud_lib.glsl"
#include "eevee_geom_types_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Point Cloud
 *
 * Point Cloud objects loads attributes from buffers through sampler buffers.
 * \{ */

#ifdef OBINFO_LIB
float3 attr_load_orco(PointCloudPoint point, float4 orco, int index)
{
  float3 P = pointcloud_get_pos();
  float3 lP = transform_point(drw_modelinv(), P);
  return drw_object_orco(lP);
}
#endif

float4 attr_load_tangent(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
float3 attr_load_uv(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_vec3(cd_buf);
}
float4 attr_load_color(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
float4 attr_load_float4(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_vec4(cd_buf);
}
float3 attr_load_float3(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_vec3(cd_buf);
}
float2 attr_load_float2(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_vec2(cd_buf);
}
float attr_load_float(PointCloudPoint point, samplerBuffer cd_buf, int index)
{
  return pointcloud_get_customdata_float(cd_buf);
}

/** \} */
