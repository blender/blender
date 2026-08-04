/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_float3x3.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline float3x3 operator*(const float a, const float3x3 m)
{
  float3x3 t;
  t.x = a * m.x;
  t.y = a * m.y;
  t.z = a * m.z;
  return t;
}
ccl_device_inline float3x3 operator*(const float3x3 m, const float a)
{
  float3x3 t;
  t.x = m.x * a;
  t.y = m.y * a;
  t.z = m.z * a;
  return t;
}

ccl_device_inline float3x3 operator*(const float3x3 a, const float3x3 b)
{
  const float3 c_x = make_float3(b.x.x, b.y.x, b.z.x);
  const float3 c_y = make_float3(b.x.y, b.y.y, b.z.y);
  const float3 c_z = make_float3(b.x.z, b.y.z, b.z.z);

  float3x3 r;
  r.x = make_float3(dot(a.x, c_x), dot(a.x, c_y), dot(a.x, c_z));
  r.y = make_float3(dot(a.y, c_x), dot(a.y, c_y), dot(a.y, c_z));
  r.z = make_float3(dot(a.z, c_x), dot(a.z, c_y), dot(a.z, c_z));
  return r;
}

/* Multiply 3x3 matrix by a 3-component column-vector.
 * The result is a 3-component column vector. */
ccl_device_inline float3 operator*(const float3x3 m, const float3 v)
{
  float3 r;
  r.x = dot(m.x, v);
  r.y = dot(m.y, v);
  r.z = dot(m.z, v);
  return r;
}

ccl_device_inline float3x3 operator/(const float3x3 m, const float a)
{
  const float inv_a = 1.0f / a;
  float3x3 t;
  t.x = m.x * inv_a;
  t.y = m.y * inv_a;
  t.z = m.z * inv_a;
  return t;
}

/* Construct 3x3 scale matrix:
 *           [  s.x  0.0  0.0 ]
 *  Result = [  0.0  s.y  0.0 ]
 *           [  0.0  0.0  s.z ] */
ccl_device_inline float3x3 float3x3_scale(const float3 s)
{
  return make_float3x3(
      make_float3(s.x, 0.0f, 0.0f), make_float3(0.0f, s.y, 0.0f), make_float3(0.0f, 0.0f, s.z));
}

ccl_device_inline float3x3 transposed(const float3x3 m)
{
  return make_float3x3(make_float3(m.x.x, m.y.x, m.z.x),
                       make_float3(m.x.y, m.y.y, m.z.y),
                       make_float3(m.x.z, m.y.z, m.z.z));
}

ccl_device_inline float determinant(const float3x3 m)
{
  return (m.x.x * (m.y.y * m.z.z - m.z.y * m.y.z) - m.x.y * (m.y.x * m.z.z - m.z.x * m.y.z) +
          m.x.z * (m.y.x * m.z.y - m.z.x * m.y.y));
}

/* The adjugate or classical adjoint adj(m) */
ccl_device_inline float3x3 adjoint(const float3x3 m)
{
  float3x3 r;

  r.x = make_float3(m.y.y * m.z.z - m.z.y * m.y.z,
                    -m.x.y * m.z.z + m.z.y * m.x.z,
                    m.x.y * m.y.z - m.y.y * m.x.z);
  r.y = make_float3(-m.y.x * m.z.z + m.z.x * m.y.z,
                    m.x.x * m.z.z - m.z.x * m.x.z,
                    -m.x.x * m.y.z + m.y.x * m.x.z);
  r.z = make_float3(m.y.x * m.z.y - m.z.x * m.y.y,
                    -m.x.x * m.z.y + m.z.x * m.x.y,
                    m.x.x * m.y.y - m.y.x * m.x.y);

  return r;
}

ccl_device_inline float3x3 inverted(const float3x3 m)
{
  const float det = determinant(m);
  if (det != 0.0f) {
    return adjoint(m) / det;
  }
  return zero_float3x3();
}

/* Convert quaternion to a 3x3 scaled rotation matrix.
 *
 * The quaternion is denoted by a 4-element vector (w, i, j, k).
 *
 * The choice of rotation is such that the quaternion [1 0 0 0] goes to an identity matrix and for
 * small a, b, c the quaternion [1 a b c] goes to the matrix
 *
 *         [  0 -c  b ]
 *   I + 2 [  c  0 -a ] + higher order terms
 *         [ -b  a  0 ]
 *
 * which corresponds to a Rodrigues approximation, the last matrix being the cross-product matrix
 * of [a b c]. Together with the property that R(q1 * q2) = R(q1) * R(q2) this uniquely defines the
 * mapping from q to R.
 *
 * No normalization of the quaternion is performed, i.e. R = ||q||^2 * Q, where Q is an orthonormal
 * matrix such that det(Q) = 1 and Q*Q' = I.
 *
 * Adopted from Ceres-solver. */
ccl_device_inline float3x3 quaternion_to_scaled_rotation(const float4 q)
{
  /* Make convenient names for elements of q.
   * Note: quaternion (w, x, y, z) is stored as float4(w, x, y, z). */
  const float a = q.x;  // w
  const float b = q.y;  // x
  const float c = q.z;  // y
  const float d = q.w;  // z

  /* Define common terms. */
  const float aa = a * a;
  const float ab = a * b;
  const float ac = a * c;
  const float ad = a * d;
  const float bb = b * b;
  const float bc = b * c;
  const float bd = b * d;
  const float cc = c * c;
  const float cd = c * d;
  const float dd = d * d;

  float3x3 r;
  r.x = make_float3(aa + bb - cc - dd, 2.0f * (bc - ad), 2.0f * (ac + bd));
  r.y = make_float3(2.0f * (ad + bc), aa - bb + cc - dd, 2.0f * (cd - ab));
  r.z = make_float3(2.0f * (bd - ac), 2.0f * (ab + cd), aa - bb - cc + dd);
  return r;
}

/* Same as above except that the rotation matrix is normalized by the Frobenius norm,
 * so that R * R' = I (and det(R) = 1).
 *
 * Adopted from Ceres-solver. */
ccl_device_inline float3x3 quaternion_to_rotation(const float4 q)
{
  /* Note: quaternion (w, x, y, z) is stored as float4(w, x, y, z). */
  const float normalizer = 1.0f / (q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  return quaternion_to_scaled_rotation(q) * normalizer;
}

CCL_NAMESPACE_END
