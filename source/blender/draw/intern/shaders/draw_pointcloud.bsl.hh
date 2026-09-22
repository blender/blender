/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_model.bsl.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "gpu_shader_math_matrix_conversion.bsl.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

namespace draw::pointcloud {

struct Point {
  float3 P;
  float radius;
  /* Point index for attribute loading. */
  int point_id;
  /* Position on shape facing the camera. */
  float3 shape_pos;
};

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

float3x3 facing_matrix(const float3 V, const float3 up_axis, bool flip)
{
  float3x3 facing_mat;
  facing_mat[2] = V;
  if (flip) {
    facing_mat[2] = -facing_mat[2];
  }
  facing_mat[1] = normalize(cross(up_axis, facing_mat[2]));
  facing_mat[0] = cross(facing_mat[1], facing_mat[2]);
  return facing_mat;
}

/**
 * Return the normal of the expanded position in world-space.
 * \arg pt : world space curve point.
 * \arg V : world space view vector (toward viewer) at `pt.P`.
 */
ShapePoint shape_point_get(const Point pt,
                           const float3 V,
                           const float3 up_axis,
                           const eObjectInfoFlag ob_flag,
                           bool flip = false)
{
  ShapePoint shape;
  shape.N = facing_matrix(V, up_axis, flip) * pt.shape_pos;
  if (flag_test(ob_flag, OBJECT_NEGATIVE_SCALE)) {
    shape.N = -shape.N;
  }
  shape.P = pt.P + shape.N * pt.radius;
  return shape;
}

}  // namespace draw::pointcloud

namespace draw {

struct PointCloud {
  [[resource_table]] draw::Infos infos;

  [[sampler(0), frequency(BATCH)]] samplerBuffer ptcloud_pos_rad_tx;

  static int point_id_get(uint vert_id)
  {
    /* Remove shape indices. */
    return int(vert_id / uint(DRW_POINTCLOUD_STRIP_TILE_SIZE));
  }

  /* Return data about the pointcloud point. */
  pointcloud::Point point_get(uint vert_id)
  {
    pointcloud::Point pt;
    pt.point_id = point_id_get(vert_id);

    float4 pos_rad = texelFetch(ptcloud_pos_rad_tx, pt.point_id);
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
};

}  // namespace draw

namespace draw::pointcloud {

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

}  // namespace draw::pointcloud
