/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_axis_angle_lib.glsl"
#include "gpu_shader_math_euler_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_quaternion_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Interpolate
 * \{ */

/**
 * Generic function for implementing slerp
 * (quaternions and spherical vector coords).
 *
 * \param t: factor in [0..1]
 * \param cosom: dot product from normalized vectors/quaternions.
 * \param r_w: calculated weights.
 */
float2 interpolate_dot_slerp(float t, float cosom)
{
  float2 w = float2(1.0f - t, t);
  /* Within [-1..1] range, avoid aligned axis. */
  constexpr float eps = 1e-4f;
  if (abs(cosom) < 1.0f - eps) {
    float omega = acos(cosom);
    w = sin(w * omega) / sin(omega);
  }
  return w;
}

Quaternion interpolate(Quaternion a, Quaternion b, float t)
{
  float4 quat = a.as_float4();
  float cosom = dot(a.as_float4(), b.as_float4());
  /* Rotate around shortest angle. */
  if (cosom < 0.0f) {
    cosom = -cosom;
    quat = -quat;
  }
  float2 w = interpolate_dot_slerp(t, cosom);
  quat = w.x * quat + w.y * b.as_float4();
  return Quaternion(UNPACK4(quat));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotate
 * \{ */

/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2f)).
 */
float3x3 rotate(float3x3 mat, AxisAngle rotation)
{
  float3x3 result;
  /* axis_vec is given to be normalized. */
  if (rotation.axis.x == 1.0f) {
    float angle_cos = cos(rotation.angle);
    float angle_sin = sin(rotation.angle);
    for (int c = 0; c < 3; c++) {
      result[0][c] = mat[0][c];
      result[1][c] = angle_cos * mat[1][c] + angle_sin * mat[2][c];
      result[2][c] = -angle_sin * mat[1][c] + angle_cos * mat[2][c];
    }
  }
  else if (rotation.axis.y == 1.0f) {
    float angle_cos = cos(rotation.angle);
    float angle_sin = sin(rotation.angle);
    for (int c = 0; c < 3; c++) {
      result[0][c] = angle_cos * mat[0][c] - angle_sin * mat[2][c];
      result[1][c] = mat[1][c];
      result[2][c] = angle_sin * mat[0][c] + angle_cos * mat[2][c];
    }
  }
  else if (rotation.axis.z == 1.0f) {
    float angle_cos = cos(rotation.angle);
    float angle_sin = sin(rotation.angle);
    for (int c = 0; c < 3; c++) {
      result[0][c] = angle_cos * mat[0][c] + angle_sin * mat[1][c];
      result[1][c] = -angle_sin * mat[0][c] + angle_cos * mat[1][c];
      result[2][c] = mat[2][c];
    }
  }
  else {
    /* Un-optimized case. Arbitrary rotation. */
    result = mat * from_rotation(rotation);
  }
  return result;
}
/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2f)).
 */
float3x3 rotate(float3x3 mat, EulerXYZ rotation)
{
  AxisAngle axis_angle;
  if (rotation.y == 0.0f && rotation.z == 0.0f) {
    axis_angle = AxisAngle(float3(1.0f, 0.0f, 0.0f), rotation.x);
  }
  else if (rotation.x == 0.0f && rotation.z == 0.0f) {
    axis_angle = AxisAngle(float3(0.0f, 1.0f, 0.0f), rotation.y);
  }
  else if (rotation.x == 0.0f && rotation.y == 0.0f) {
    axis_angle = AxisAngle(float3(0.0f, 0.0f, 1.0f), rotation.z);
  }
  else {
    /* Un-optimized case. Arbitrary rotation. */
    return mat * from_rotation(rotation);
  }
  return rotate(mat, axis_angle);
}
/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2f)).
 */
float4x4 rotate(float4x4 mat, AxisAngle rotation)
{
  float4x4 result = to_float4x4(rotate(to_float3x3(mat), rotation));
  result[0][3] = mat[0][3];
  result[1][3] = mat[1][3];
  result[2][3] = mat[2][3];
  result[3][0] = mat[3][0];
  result[3][1] = mat[3][1];
  result[3][2] = mat[3][2];
  result[3][3] = mat[3][3];
  return result;
}
/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2f)).
 */
float4x4 rotate(float4x4 mat, EulerXYZ rotation)
{
  float4x4 result = to_float4x4(rotate(to_float3x3(mat), rotation));
  result[0][3] = mat[0][3];
  result[1][3] = mat[1][3];
  result[2][3] = mat[2][3];
  result[3][0] = mat[3][0];
  result[3][1] = mat[3][1];
  result[3][2] = mat[3][2];
  result[3][3] = mat[3][3];
  return result;
}

/** \} */
