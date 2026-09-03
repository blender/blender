/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"
#include "util/types_float3.h"
#include "util/types_float4.h"
#include "util/types_quaternion.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline Quaternion operator-(const Quaternion q)
{
  return make_quaternion(-q.w, -q.x, -q.y, -q.z);
}

ccl_device_inline Quaternion operator*(const float a, const Quaternion q)
{
  return make_quaternion(a * q.w, a * q.x, a * q.y, a * q.z);
}
ccl_device_inline Quaternion operator*(const Quaternion q, const float a)
{
  return make_quaternion(q.w * a, q.x * a, q.y * a, q.z * a);
}

ccl_device_inline Quaternion operator+(const Quaternion a, const Quaternion b)
{
  return make_quaternion(a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z);
}

ccl_device_inline Quaternion operator-(const Quaternion a, const Quaternion b)
{
  return make_quaternion(a.w - b.w, a.x - b.x, a.y - b.y, a.z - b.z);
}

ccl_device_inline ccl_private Quaternion &operator+=(ccl_private Quaternion &a, const Quaternion b)
{
  a = a + b;
  return a;
}

ccl_device_inline ccl_private Quaternion &operator-=(ccl_private Quaternion &a, const Quaternion b)
{
  a = a - b;
  return a;
}

ccl_device_inline float dot(const Quaternion a, const Quaternion b)
{
  return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

ccl_device_inline Quaternion normalized(const Quaternion q)
{
  const float len = sqrtf(dot(q, q));
  if (len != 0.0f) {
    const float len_inv = 1.0f / len;
    return make_quaternion(q.w * len_inv, q.x * len_inv, q.y * len_inv, q.z * len_inv);
  }
  return identity_quaternion();
}

/* Linear interpolation around the shortest angle.
 * The caller is responsible for ensuring the input quaternions are normalized. */
ccl_device_inline Quaternion lerp(const Quaternion q1, const Quaternion q2, const float t)
{
  /* TODO(sergey): Add assert that the quaternions are normalized? */
  /* Ensure rotation around shortest angle, negated quaternions are the same. */
  const Quaternion q2_shortest_angle = dot(q1, q2) >= 0.0f ? q2 : -q2;
  return normalized((1.0f - t) * q1 + t * q2_shortest_angle);
}

ccl_device_template_spec Quaternion make_zero()
{
  return make_quaternion(0.0f, 0.0f, 0.0f, 0.0f);
}

ccl_device_inline float3 make_float3(const Quaternion q)
{
  /* Matches legacy storage and attribute access: make_float3(make_float4(q.w, q.x, q.y, q.z)) */
  return make_float3(q.w, q.x, q.y);
}

ccl_device_inline float4 make_float4(const Quaternion q)
{
  /* Matches legacy storage and attribute access: make_float4(q.w, q.x, q.y, q.z) */
  return make_float4(q.w, q.x, q.y, q.z);
}

/* mix() on quaternions is used from generic attribute sampling and interpolation.
 * Define it as a float4 mix for the compatibility until we can change the way quaternion
 * attributes behave. */
ccl_device_inline Quaternion mix(const Quaternion a, const Quaternion b, const float t)
{
  return a + t * (b - a);
}

ccl_device_inline void copy_v4_qt(ccl_private float *r, const Quaternion val)
{
  r[0] = val.w;
  r[1] = val.x;
  r[2] = val.y;
  r[3] = val.z;
}

CCL_NAMESPACE_END
