/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_axis_angle_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_euler_lib.glsl"
#include "gpu_shader_math_matrix_conversion_lib.glsl"
#include "gpu_shader_math_matrix_normalize_lib.glsl"
#include "gpu_shader_math_quaternion_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace detail {

Quaternion normalized_to_quat_fast(float3x3 mat)
{
  /* Caller must ensure matrices aren't negative for valid results, see: #24291, #94231. */
  Quaternion q;

  /* Method outlined by Mike Day, ref: https://math.stackexchange.com/a/3183435/220949
   * with an additional `sqrtf(..)` for higher precision result.
   * Removing the `sqrt` causes tests to fail unless the precision is set to 1e-6f or larger. */

  if (mat[2][2] < 0.0f) {
    if (mat[0][0] > mat[1][1]) {
      float trace = 1.0f + mat[0][0] - mat[1][1] - mat[2][2];
      float s = 2.0f * sqrt(trace);
      if (mat[1][2] < mat[2][1]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.y = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[1][2] - mat[2][1]) * s;
      q.z = (mat[0][1] + mat[1][0]) * s;
      q.w = (mat[2][0] + mat[0][2]) * s;
      if ((trace == 1.0f) && (q.x == 0.0f && q.z == 0.0f && q.w == 0.0f)) {
        /* Avoids the need to normalize the degenerate case. */
        q.y = 1.0f;
      }
    }
    else {
      float trace = 1.0f - mat[0][0] + mat[1][1] - mat[2][2];
      float s = 2.0f * sqrt(trace);
      if (mat[2][0] < mat[0][2]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.z = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[2][0] - mat[0][2]) * s;
      q.y = (mat[0][1] + mat[1][0]) * s;
      q.w = (mat[1][2] + mat[2][1]) * s;
      if ((trace == 1.0f) && (q.x == 0.0f && q.y == 0.0f && q.w == 0.0f)) {
        /* Avoids the need to normalize the degenerate case. */
        q.z = 1.0f;
      }
    }
  }
  else {
    if (mat[0][0] < -mat[1][1]) {
      float trace = 1.0f - mat[0][0] - mat[1][1] + mat[2][2];
      float s = 2.0f * sqrt(trace);
      if (mat[0][1] < mat[1][0]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.w = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[0][1] - mat[1][0]) * s;
      q.y = (mat[2][0] + mat[0][2]) * s;
      q.z = (mat[1][2] + mat[2][1]) * s;
      if ((trace == 1.0f) && (q.x == 0.0f && q.y == 0.0f && q.z == 0.0f)) {
        /* Avoids the need to normalize the degenerate case. */
        q.w = 1.0f;
      }
    }
    else {
      /* NOTE(@ideasman42): A zero matrix will fall through to this block,
       * needed so a zero scaled matrices to return a quaternion without rotation, see: #101848. */
      float trace = 1.0f + mat[0][0] + mat[1][1] + mat[2][2];
      float s = 2.0f * sqrt(trace);
      q.x = 0.25f * s;
      s = 1.0f / s;
      q.y = (mat[1][2] - mat[2][1]) * s;
      q.z = (mat[2][0] - mat[0][2]) * s;
      q.w = (mat[0][1] - mat[1][0]) * s;
      if ((trace == 1.0f) && (q.y == 0.0f && q.z == 0.0f && q.w == 0.0f)) {
        /* Avoids the need to normalize the degenerate case. */
        q.x = 1.0f;
      }
    }
  }
  return q;
}

Quaternion normalized_to_quat_with_checks(float3x3 mat)
{
  float det = determinant(mat);
  if (!isfinite(det)) {
    return Quaternion::identity();
  }
  if (det < 0.0f) {
    return normalized_to_quat_fast(-mat);
  }
  return normalized_to_quat_fast(mat);
}

void normalized_to_eul2(float3x3 mat, out EulerXYZ eul1, out EulerXYZ eul2)
{
  float cy = hypot(mat[0][0], mat[0][1]);
  if (cy > 16.0f * FLT_EPSILON) {
    eul1.x = atan2(mat[1][2], mat[2][2]);
    eul1.y = atan2(-mat[0][2], cy);
    eul1.z = atan2(mat[0][1], mat[0][0]);

    eul2.x = atan2(-mat[1][2], -mat[2][2]);
    eul2.y = atan2(-mat[0][2], -cy);
    eul2.z = atan2(-mat[0][1], -mat[0][0]);
  }
  else {
    eul1.x = atan2(-mat[2][1], mat[1][1]);
    eul1.y = atan2(-mat[0][2], cy);
    eul1.z = 0.0f;

    eul2 = eul1;
  }
}

}  // namespace detail

/* -------------------------------------------------------------------- */
/** \name Quaternion Functions
 * \{ */

Quaternion to_quaternion(EulerXYZ eul)
{
  float ti = eul.x * 0.5f;
  float tj = eul.y * 0.5f;
  float th = eul.z * 0.5f;
  float ci = cos(ti);
  float cj = cos(tj);
  float ch = cos(th);
  float si = sin(ti);
  float sj = sin(tj);
  float sh = sin(th);
  float cc = ci * ch;
  float cs = ci * sh;
  float sc = si * ch;
  float ss = si * sh;

  Quaternion quat;
  quat.x = cj * cc + sj * ss;
  quat.y = cj * sc - sj * cs;
  quat.z = cj * ss + sj * cc;
  quat.w = cj * cs - sj * sc;
  return quat;
}

/**
 * Extract quaternion rotation from transform matrix.
 * \note normalized is set to false by default.
 */
Quaternion to_quaternion(float3x3 mat)
{
  return detail::normalized_to_quat_with_checks(normalize(mat));
}
/**
 * Extract quaternion rotation from transform matrix.
 * \note normalized is set to false by default.
 */
Quaternion to_quaternion(float3x3 mat, const bool normalized)
{
  if (!normalized) {
    mat = normalize(mat);
  }
  return to_quaternion(mat);
}
/**
 * Extract quaternion rotation from transform matrix.
 * \note normalized is set to false by default.
 */
Quaternion to_quaternion(float4x4 mat)
{
  return to_quaternion(to_float3x3(mat));
}
/**
 * Extract quaternion rotation from transform matrix.
 * \note normalized is set to false by default.
 */
Quaternion to_quaternion(float4x4 mat, const bool normalized)
{
  return to_quaternion(to_float3x3(mat), normalized);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Functions
 * \{ */

/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
EulerXYZ to_euler(float3x3 mat, const bool normalized)
{
  if (!normalized) {
    mat = normalize(mat);
  }
  EulerXYZ eul1, eul2;
  detail::normalized_to_eul2(mat, eul1, eul2);
  /* Return best, which is just the one with lowest values it in. */
  return (length_manhattan(eul1.as_float3()) > length_manhattan(eul2.as_float3())) ? eul2 : eul1;
}
/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
EulerXYZ to_euler(float3x3 mat)
{
  return to_euler(mat, true);
}
/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
EulerXYZ to_euler(float4x4 mat, const bool normalized)
{
  return to_euler(to_float3x3(mat), normalized);
}
/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
EulerXYZ to_euler(float4x4 mat)
{
  return to_euler(to_float3x3(mat));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Axis Angle Functions
 * \{ */

Quaternion to_axis_angle(AxisAngle axis_angle)
{
  float angle_cos = cos(axis_angle.angle);
  /** Using half angle identities: sin(angle / 2) = sqrt((1 - angle_cos) / 2) */
  float sine = sqrt(0.5f - angle_cos * 0.5f);
  float cosine = sqrt(0.5f + angle_cos * 0.5f);

  /* TODO(fclem): Optimize. */
  float angle_sin = sin(axis_angle.angle);
  if (angle_sin < 0.0f) {
    sine = -sine;
  }

  Quaternion quat;
  quat.x = cosine;
  quat.y = axis_angle.axis.x * sine;
  quat.z = axis_angle.axis.y * sine;
  quat.w = axis_angle.axis.z * sine;
  return quat;
}

AxisAngle to_axis_angle(Quaternion quat)
{
  /* Calculate angle/2, and sin(angle/2). */
  float ha = acos(quat.x);
  float si = sin(ha);

  /* From half-angle to angle. */
  float angle = ha * 2;
  /* Prevent division by zero for axis conversion. */
  if (abs(si) < 0.0005f) {
    si = 1.0f;
  }

  float3 axis = float3(quat.y, quat.z, quat.w) / si;
  if (is_zero(axis)) {
    axis[1] = 1.0f;
  }
  return AxisAngle(axis, angle);
}

AxisAngle to_axis_angle(EulerXYZ eul)
{
  /* Use quaternions as intermediate representation for now... */
  return to_axis_angle(to_quaternion(eul));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrices Functions
 * \{ */

/**
 * Decompose a matrix into location, rotation, and scale components.
 * \tparam allow_negative_scale: if true, will compute determinant to know if matrix is negative.
 * Rotation and scale values will be flipped if it is negative.
 * This is a costly operation so it is disabled by default.
 */
void to_rot_scale(float3x3 mat, out EulerXYZ r_rotation, out float3 r_scale)
{
  r_scale = to_scale(mat);
  r_rotation = to_euler(mat, true);
}

/**
 * Decompose a matrix into location, rotation, and scale components.
 * \tparam allow_negative_scale: if true, will compute determinant to know if matrix is negative.
 * Rotation and scale values will be flipped if it is negative.
 * This is a costly operation so it is disabled by default.
 */
void to_rot_scale(float3x3 mat,
                  out EulerXYZ r_rotation,
                  out float3 r_scale,
                  const bool allow_negative_scale)
{
  float3x3 normalized_mat = normalize_and_get_size(mat, r_scale);
  if (allow_negative_scale) {
    if (is_negative(normalized_mat)) {
      normalized_mat = -normalized_mat;
      r_scale = -r_scale;
    }
  }
  r_rotation = to_euler(normalized_mat, true);
}
void to_rot_scale(float3x3 mat, out Quaternion r_rotation, out float3 r_scale)
{
  r_scale = to_scale(mat);
  r_rotation = to_quaternion(mat, true);
}
void to_rot_scale(float3x3 mat,
                  out Quaternion r_rotation,
                  out float3 r_scale,
                  const bool allow_negative_scale)
{
  float3x3 normalized_mat = normalize_and_get_size(mat, r_scale);
  if (allow_negative_scale) {
    if (is_negative(normalized_mat)) {
      normalized_mat = -normalized_mat;
      r_scale = -r_scale;
    }
  }
  r_rotation = to_quaternion(normalized_mat, true);
}

void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out EulerXYZ r_rotation,
                      out float3 r_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(to_float3x3(mat), r_rotation, r_scale);
}
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out EulerXYZ r_rotation,
                      out float3 r_scale,
                      const bool allow_negative_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(to_float3x3(mat), r_rotation, r_scale, allow_negative_scale);
}
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out Quaternion r_rotation,
                      out float3 r_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(to_float3x3(mat), r_rotation, r_scale);
}
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out Quaternion r_rotation,
                      out float3 r_scale,
                      const bool allow_negative_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(to_float3x3(mat), r_rotation, r_scale, allow_negative_scale);
}

/** \} */
