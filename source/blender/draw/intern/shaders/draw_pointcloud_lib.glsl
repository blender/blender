/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#include "draw_defines.hh"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_matrix_conversion_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace pointcloud {

struct Point {
  float3 P;
  float radius;
  /* Point index for attribute loading. */
  int point_id;
  /* Position on shape facing the camera. */
  float3 shape_pos;
};

int point_id_get(uint vert_id)
{
  /* Remove shape indices. */
  return int(vert_id / uint(DRW_POINTCLOUD_STRIP_TILE_SIZE));
}

/* Return data about the pointcloud point. */
Point point_get(uint vert_id)
{
  Point pt;
  pt.point_id = point_id_get(vert_id);

  auto &buf = sampler_get(draw_pointcloud, ptcloud_pos_rad_tx);
  float4 pos_rad = texelFetch(buf, pt.point_id);
  pt.P = pos_rad.xyz;
  pt.radius = pos_rad.w;
  pt.shape_pos = float3(NAN_FLT);
  switch (vert_id % DRW_POINTCLOUD_STRIP_TILE_SIZE) {
    case 0:
      pt.shape_pos = float3(-1.0, 0.0, 0.0);
      break;
    case 1:
      pt.shape_pos = float3(0.0, -1.0, 0.0);
      break;
    case 2:
      pt.shape_pos = float3(0.0, 0.0, 1.0);
      break;
    case 3:
      pt.shape_pos = float3(1.0, 0.0, 0.0);
      break;
    case 4:
      pt.shape_pos = float3(0.0, 0.0, 1.0);
      break;
    case 5:
      pt.shape_pos = float3(0.0, 1.0, 0.0);
      break;
    case 6:
      pt.shape_pos = float3(-1.0, 0.0, 0.0);
      break;
    default:
      break;
  }
  return pt;
}

Point object_to_world(Point pt, float4x4 object_to_world)
{
  pt.P = transform_point(object_to_world, pt.P);
  pt.radius *= length(to_scale(object_to_world)) * M_SQRT1_3;
  return pt;
}

struct ShapePoint {
  /* Position on the shape. */
  float3 P;
  /* Shading normal at the position on the shape. */
  float3 N;
};

float3x3 facing_matrix(const float3 V, const float3 up_axis)
{
  float3x3 facing_mat;
  facing_mat[2] = V;
#ifdef MAT_GEOM_POINTCLOUD
  if (ptcloud_backface) {
    facing_mat[2] = -facing_mat[2];
  }
#endif
  facing_mat[1] = normalize(cross(up_axis, facing_mat[2]));
  facing_mat[0] = cross(facing_mat[1], facing_mat[2]);
  return facing_mat;
}

/**
 * Return the normal of the expanded position in world-space.
 * \arg pt : world space curve point.
 * \arg V : world space view vector (toward viewer) at `pt.P`.
 */
ShapePoint shape_point_get(const Point pt, const float3 V, const float3 up_axis)
{
  ShapePoint shape;
  shape.N = facing_matrix(V, up_axis) * pt.shape_pos;
  shape.P = pt.P + shape.N * pt.radius;
  return shape;
}

float3 get_point_position(const int point_id)
{
  auto &buf = sampler_get(draw_pointcloud, ptcloud_pos_rad_tx);
  return texelFetch(buf, point_id).xyz;
}

float get_customdata_float(const int point_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, point_id).r;
}

float2 get_customdata_vec2(const int point_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, point_id).rg;
}

float3 get_customdata_vec3(const int point_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, point_id).rgb;
}

float4 get_customdata_vec4(const int point_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, point_id).rgba;
}

float2 get_barycentric()
{
  /* TODO: To be implemented. */
  return float2(0.0f);
}

}  // namespace pointcloud
