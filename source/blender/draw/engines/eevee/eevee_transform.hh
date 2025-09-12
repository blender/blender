/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/**
 * The #Transform class is used to store object transforms in a compact manner (row major).
 */
struct Transform {
  /* The transform is stored transposed for compactness. */
  float4 x, y, z;

#ifndef GPU_SHADER
  Transform() = default;
  Transform(const float4x4 &tx)
      : x(tx[0][0], tx[1][0], tx[2][0], tx[3][0]),
        y(tx[0][1], tx[1][1], tx[2][1], tx[3][1]),
        z(tx[0][2], tx[1][2], tx[2][2], tx[3][2])
  {
  }

  operator float4x4() const
  {
    return float4x4(float4(x.x, y.x, z.x, 0.0f),
                    float4(x.y, y.y, z.y, 0.0f),
                    float4(x.z, y.z, z.z, 0.0f),
                    float4(x.w, y.w, z.w, 1.0f));
  }
#endif
};

static inline float4x4 transform_to_matrix(Transform t)
{
  return float4x4(float4(t.x.x, t.y.x, t.z.x, 0.0f),
                  float4(t.x.y, t.y.y, t.z.y, 0.0f),
                  float4(t.x.z, t.y.z, t.z.z, 0.0f),
                  float4(t.x.w, t.y.w, t.z.w, 1.0f));
}

static inline Transform transform_from_matrix(float4x4 m)
{
  Transform t;
  t.x = float4(m[0][0], m[1][0], m[2][0], m[3][0]);
  t.y = float4(m[0][1], m[1][1], m[2][1], m[3][1]);
  t.z = float4(m[0][2], m[1][2], m[2][2], m[3][2]);
  return t;
}

static inline float3 transform_x_axis(Transform t)
{
  return float3(t.x.x, t.y.x, t.z.x);
}
static inline float3 transform_y_axis(Transform t)
{
  return float3(t.x.y, t.y.y, t.z.y);
}
static inline float3 transform_z_axis(Transform t)
{
  return float3(t.x.z, t.y.z, t.z.z);
}
static inline float3 transform_location(Transform t)
{
  return float3(t.x.w, t.y.w, t.z.w);
}

#ifdef GPU_SHADER
static inline bool transform_equal(Transform a, Transform b)
{
  return all(equal(a.x, b.x)) && all(equal(a.y, b.y)) && all(equal(a.z, b.z));
}
#endif

static inline float3 transform_point(Transform t, float3 point)
{
  return float4(point, 1.0f) * float3x4(t.x, t.y, t.z);
}

static inline float3 transform_direction(Transform t, float3 direction)
{
  return direction * float3x3(float3(t.x.x, t.x.y, t.x.z),
                              float3(t.y.x, t.y.y, t.y.z),
                              float3(t.z.x, t.z.y, t.z.z));
}

static inline float3 transform_direction_transposed(Transform t, float3 direction)
{
  return float3x3(float3(t.x.x, t.x.y, t.x.z),
                  float3(t.y.x, t.y.y, t.y.z),
                  float3(t.z.x, t.z.y, t.z.z)) *
         direction;
}

/* Assumes the transform has unit scale. */
static inline float3 transform_point_inversed(Transform t, float3 point)
{
  return float3x3(float3(t.x.x, t.x.y, t.x.z),
                  float3(t.y.x, t.y.y, t.y.z),
                  float3(t.z.x, t.z.y, t.z.z)) *
         (point - transform_location(t));
}

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
