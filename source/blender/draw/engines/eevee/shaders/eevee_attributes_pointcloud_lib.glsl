/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_pointcloud_lib.glsl"
#include "eevee_geom_types_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Point Cloud
 *
 * Point Cloud objects loads attributes from buffers through sampler buffers.
 * \{ */

float3 attr_load_orco(PointCloudPoint point, float4 /*orco*/, int /*index*/)
{
  /* NOTE: Doesn't support ORCO attribute. */
  return point.orco_default;
}
float4 attr_load_tangent(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_vec4(point.point_id, cd_buf);
}
float3 attr_load_uv(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_vec3(point.point_id, cd_buf);
}
float4 attr_load_color(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_vec4(point.point_id, cd_buf);
}
float4 attr_load_float4(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_vec4(point.point_id, cd_buf);
}
float3 attr_load_float3(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_vec3(point.point_id, cd_buf);
}
float2 attr_load_float2(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_vec2(point.point_id, cd_buf);
}
float attr_load_float(PointCloudPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return pointcloud::get_customdata_float(point.point_id, cd_buf);
}

/** \} */
