/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct Quaternion {
  float x, y, z, w;

  static Quaternion identity()
  {
    return {1, 0, 0, 0};
  }

  float4 as_float4() const
  {
    return float4(this->x, this->y, this->z, this->w);
  }
};

/* -------------------------------------------------------------------- */
/** \name Quaternion Math
 * \{ */

Quaternion math_quaternion_multiply(Quaternion a, Quaternion b)
{
  Quaternion result;
  result.x = a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w;
  result.y = a.x * b.y + a.y * b.x + a.z * b.w - a.w * b.z;
  result.z = a.x * b.z - a.y * b.w + a.z * b.x + a.w * b.y;
  result.w = a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x;
  return result;
}

Quaternion quaternion_conjugate(Quaternion q)
{
  return {q.x, -q.y, -q.z, -q.w};
}

float3 transform_point_by_quaternion(Quaternion q, float3 v)
{
  const Quaternion v_quat = {0.0f, v.x, v.y, v.z};
  const Quaternion result = math_quaternion_multiply(math_quaternion_multiply(q, v_quat),
                                                     quaternion_conjugate(q));
  return float3(result.y, result.z, result.w);
}

/** \} */
