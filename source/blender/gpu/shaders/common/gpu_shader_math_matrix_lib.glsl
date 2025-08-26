/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_rotation_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_MATRIX_LIB_GLSL
#  define GPU_SHADER_MATH_MATRIX_LIB_GLSL

/* -------------------------------------------------------------------- */
/** \name Static constructors
 * \{ */

float2x2 mat2x2_diagonal(float v)
{
  return float2x2(float2(v, 0.0f), float2(0.0f, v));
}
float3x3 mat3x3_diagonal(float v)
{
  return float3x3(float3(v, 0.0f, 0.0f), float3(0.0f, v, 0.0f), float3(0.0f, 0.0f, v));
}
float4x4 mat4x4_diagonal(float v)
{
  return float4x4(float4(v, 0.0f, 0.0f, 0.0f),
                  float4(0.0f, v, 0.0f, 0.0f),
                  float4(0.0f, 0.0f, v, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, v));
}

float2x2 mat2x2_all(float v)
{
  return float2x2(float2(v), float2(v));
}
float3x3 mat3x3_all(float v)
{
  return float3x3(float3(v), float3(v), float3(v));
}
float4x4 mat4x4_all(float v)
{
  return float4x4(float4(v), float4(v), float4(v), float4(v));
}

float2x2 mat2x2_zero()
{
  return mat2x2_all(0.0f);
}
float3x3 mat3x3_zero()
{
  return mat3x3_all(0.0f);
}
float4x4 mat4x4_zero()
{
  return mat4x4_all(0.0f);
}

float2x2 mat2x2_identity()
{
  return mat2x2_diagonal(1.0f);
}
float3x3 mat3x3_identity()
{
  return mat3x3_diagonal(1.0f);
}
float4x4 mat4x4_identity()
{
  return mat4x4_diagonal(1.0f);
}

/** \} */

/* Metal does not need prototypes. */
#  ifndef GPU_METAL

/* -------------------------------------------------------------------- */
/** \name Matrix Operations
 * \{ */

/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float2x2 invert(float2x2 mat);
float3x3 invert(float3x3 mat);
float4x4 invert(float4x4 mat);

float2x2 invert(float2x2 mat, out bool r_success);
float3x3 invert(float3x3 mat, out bool r_success);
float4x4 invert(float4x4 mat, out bool r_success);

/**
 * Flip the matrix across its diagonal. Also flips dimensions for non square matrices.
 */
// mat3x3 transpose(mat3x3 mat); /* Built-In using GLSL language. */

/**
 * Normalize each column of the matrix individually.
 */
float2x2 normalize(float2x2 mat);
float2x3 normalize(float2x3 mat);
float2x4 normalize(float2x4 mat);
float3x2 normalize(float3x2 mat);
float3x3 normalize(float3x3 mat);
float3x4 normalize(float3x4 mat);
float4x2 normalize(float4x2 mat);
float4x3 normalize(float4x3 mat);
float4x4 normalize(float4x4 mat);

/**
 * Normalize each column of the matrix individually.
 * Return the length of each column vector.
 */
float2x2 normalize_and_get_size(float2x2 mat, out float2 r_size);
float2x3 normalize_and_get_size(float2x3 mat, out float2 r_size);
float2x4 normalize_and_get_size(float2x4 mat, out float2 r_size);
float3x2 normalize_and_get_size(float3x2 mat, out float3 r_size);
float3x3 normalize_and_get_size(float3x3 mat, out float3 r_size);
float3x4 normalize_and_get_size(float3x4 mat, out float3 r_size);
float4x2 normalize_and_get_size(float4x2 mat, out float4 r_size);
float4x3 normalize_and_get_size(float4x3 mat, out float4 r_size);
float4x4 normalize_and_get_size(float4x4 mat, out float4 r_size);

/**
 * Returns the determinant of the matrix.
 * It can be interpreted as the signed volume (or area) of the unit cube after transformation.
 */
// float determinant(mat3x3 mat); /* Built-In using GLSL language. */

/**
 * Returns the adjoint of the matrix (also known as adjugate matrix).
 */
float2x2 adjoint(float2x2 mat);
float3x3 adjoint(float3x3 mat);
float4x4 adjoint(float4x4 mat);

/**
 * Equivalent to `mat * from_location(translation)` but with fewer operation.
 */
float4x4 translate(float4x4 mat, float2 translation);
float4x4 translate(float4x4 mat, float3 translation);

/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2f)).
 */
float3x3 rotate(float3x3 mat, AxisAngle rotation);
float3x3 rotate(float3x3 mat, EulerXYZ rotation);
float4x4 rotate(float4x4 mat, AxisAngle rotation);
float4x4 rotate(float4x4 mat, EulerXYZ rotation);

/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
float3x3 scale(float3x3 mat, float2 scale);
float3x3 scale(float3x3 mat, float3 scale);
float4x4 scale(float4x4 mat, float2 scale);
float4x4 scale(float4x4 mat, float3 scale);

/**
 * Interpolate each component linearly.
 */
// mat4x4 interpolate_linear(mat4x4 a, mat4x4 b, float t); /* TODO. */

/**
 * A polar-decomposition-based interpolation between matrix A and matrix B.
 */
// mat3x3 interpolate(mat3x3 a, mat3x3 b, float t); /* Not implemented. Too complex to port. */

/**
 * Naive interpolation implementation, faster than polar decomposition
 *
 * \note This code is about five times faster than the polar decomposition.
 * However, it gives un-expected results even with non-uniformly scaled matrices,
 * see #46418 for an example.
 *
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
float3x3 interpolate_fast(float3x3 a, float3x3 b, float t);

/**
 * Naive transform matrix interpolation,
 * based on naive-decomposition-based interpolation from #interpolate_fast<T, 3, 3>.
 */
float4x4 interpolate_fast(float4x4 a, float4x4 b, float t);

/**
 * Compute Moore-Penrose pseudo inverse of matrix.
 * Singular values below epsilon are ignored for stability (truncated SVD).
 */
// mat4x4 pseudo_invert(mat4x4 mat, float epsilon); /* Not implemented. Too complex to port. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init helpers.
 * \{ */

/**
 * Create a translation only matrix. Matrix dimensions should be at least 4 col x 3 row.
 */
float4x4 from_location(float3 location);

/**
 * Create a matrix whose diagonal is defined by the given scale vector.
 */
float2x2 from_scale(float2 scale);
float3x3 from_scale(float3 scale);
float4x4 from_scale(float4 scale);

/**
 * Create a rotation only matrix.
 */
float2x2 from_rotation(Angle rotation);
float3x3 from_rotation(EulerXYZ rotation);
float3x3 from_rotation(Quaternion rotation);
float3x3 from_rotation(AxisAngle rotation);

/**
 * Create a transform matrix with rotation and scale applied in this order.
 */
float3x3 from_rot_scale(EulerXYZ rotation, float3 scale);

/**
 * Create a transform matrix with translation and rotation applied in this order.
 */
float4x4 from_loc_rot(float3 location, EulerXYZ rotation);

/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
float4x4 from_loc_rot_scale(float3 location, EulerXYZ rotation, float3 scale);

/**
 * Creates a 2D rotation matrix with the angle that the given direction makes with the x axis.
 * Assumes the direction vector is normalized.
 */
float2x2 from_direction(float2 direction);

/**
 * Create a rotation matrix from 2 basis vectors.
 * The matrix determinant is given to be positive and it can be converted to other rotation types.
 * \note `forward` and `up` must be normalized.
 */
// mat3x3 from_normalized_axis_data(vec3 forward, vec3 up); /* TODO. */

/**
 * Create a transform matrix with translation and rotation from 2 basis vectors and a translation.
 * \note `forward` and `up` must be normalized.
 */
// mat4x4 from_normalized_axis_data(vec3 location, vec3 forward, vec3 up); /* TODO. */

/**
 * Create a rotation matrix from only one \a up axis.
 * The other axes are chosen to always be orthogonal. The resulting matrix is a basis matrix.
 * \note `up` must be normalized.
 * \note This can be used to create a tangent basis from a normal vector.
 * \note The output of this function is not given to be same across blender version. Prefer using
 * `from_orthonormal_axes` for more stable output.
 */
float3x3 from_up_axis(float3 up);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion function.
 * \{ */

/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
EulerXYZ to_euler(float3x3 mat);
EulerXYZ to_euler(float3x3 mat, const bool normalized);
EulerXYZ to_euler(float4x4 mat);
EulerXYZ to_euler(float4x4 mat, const bool normalized);

/**
 * Extract quaternion rotation from transform matrix.
 * \note normalized is set to false by default.
 */
Quaternion to_quaternion(float3x3 mat);
Quaternion to_quaternion(float3x3 mat, const bool normalized);
Quaternion to_quaternion(float4x4 mat);
Quaternion to_quaternion(float4x4 mat, const bool normalized);

/**
 * Extract the absolute 3d scale from a transform matrix.
 */
float3 to_scale(float3x3 mat);
float3 to_scale(float3x3 mat, const bool allow_negative_scale);
float3 to_scale(float4x4 mat);
float3 to_scale(float4x4 mat, const bool allow_negative_scale);

/**
 * Decompose a matrix into location, rotation, and scale components.
 * \tparam allow_negative_scale: if true, will compute determinant to know if matrix is negative.
 * Rotation and scale values will be flipped if it is negative.
 * This is a costly operation so it is disabled by default.
 */
void to_rot_scale(float3x3 mat, out EulerXYZ r_rotation, out float3 r_scale);
void to_rot_scale(float3x3 mat, out Quaternion r_rotation, out float3 r_scale);
void to_rot_scale(float3x3 mat, out AxisAngle r_rotation, out float3 r_scale);
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out EulerXYZ r_rotation,
                      out float3 r_scale);
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out Quaternion r_rotation,
                      out float3 r_scale);
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out AxisAngle r_rotation,
                      out float3 r_scale);

void to_rot_scale(float3x3 mat,
                  out EulerXYZ r_rotation,
                  out float3 r_scale,
                  const bool allow_negative_scale);
void to_rot_scale(float3x3 mat,
                  out Quaternion r_rotation,
                  out float3 r_scale,
                  const bool allow_negative_scale);
void to_rot_scale(float3x3 mat,
                  out AxisAngle r_rotation,
                  out float3 r_scale,
                  const bool allow_negative_scale);
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out EulerXYZ r_rotation,
                      out float3 r_scale,
                      const bool allow_negative_scale);
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out Quaternion r_rotation,
                      out float3 r_scale,
                      const bool allow_negative_scale);
void to_loc_rot_scale(float4x4 mat,
                      out float3 r_location,
                      out AxisAngle r_rotation,
                      out float3 r_scale,
                      const bool allow_negative_scale);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform function.
 * \{ */

/**
 * Transform a 3d point using a 3x3 matrix (rotation & scale).
 */
float3 transform_point(float3x3 mat, float3 point);

/**
 * Transform a 3d point using a 4x4 matrix (location & rotation & scale).
 */
float3 transform_point(float4x4 mat, float3 point);

/**
 * Transform a 3d direction vector using a 3x3 matrix (rotation & scale).
 */
float3 transform_direction(float3x3 mat, float3 direction);

/**
 * Transform a 3d direction vector using a 4x4 matrix (rotation & scale).
 */
float3 transform_direction(float4x4 mat, float3 direction);

/**
 * Project a point using a matrix (location & rotation & scale & perspective divide).
 */
float2 project_point(float3x3 mat, float2 point);
float3 project_point(float4x4 mat, float3 point);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Projection Matrices.
 * \{ */

/**
 * \brief Create an orthographic projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * The resulting matrix can be used with either #project_point or #transform_point.
 */
float4x4 projection_orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip);

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * `left`, `right`, `bottom`, `top` are frustum side distances at `z=near_clip`.
 * The resulting matrix can be used with #project_point.
 */
float4x4 projection_perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip);

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * Uses field of view angles instead of plane distances.
 * The resulting matrix can be used with #project_point.
 */
float4x4 projection_perspective_fov(float angle_left,
                                    float angle_right,
                                    float angle_bottom,
                                    float angle_top,
                                    float near_clip,
                                    float far_clip);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compare / Test
 * \{ */

/**
 * Returns true if all of the matrices components are strictly equal to 0.
 */
bool is_zero(float3x3 a);
bool is_zero(float4x4 a);

/**
 * Returns true if matrix has inverted handedness.
 *
 * \note It doesn't use determinant(mat4x4) as only the 3x3 components are needed
 * when the matrix is used as a transformation to represent location/scale/rotation.
 */
bool is_negative(float3x3 mat);
bool is_negative(float4x4 mat);

/**
 * Returns true if matrices are equal within the given epsilon.
 */
bool is_equal(float2x2 a, float2x2 b, float epsilon);
bool is_equal(float3x3 a, float3x3 b, float epsilon);
bool is_equal(float4x4 a, float4x4 b, float epsilon);

/**
 * Test if the X, Y and Z axes are perpendicular with each other.
 */
bool is_orthogonal(float3x3 mat);
bool is_orthogonal(float4x4 mat);

/**
 * Test if the X, Y and Z axes are perpendicular with each other and unit length.
 */
bool is_orthonormal(float3x3 mat);
bool is_orthonormal(float4x4 mat);

/**
 * Test if the X, Y and Z axes are perpendicular with each other and the same length.
 */
bool is_uniformly_scaled(float3x3 mat);

/** \} */

#  endif /* GPU_METAL */

/* ---------------------------------------------------------------------- */
/** \name Implementation
 * \{ */

float2x2 invert(float2x2 mat)
{
  return inverse(mat);
}
float3x3 invert(float3x3 mat)
{
  return inverse(mat);
}
float4x4 invert(float4x4 mat)
{
  return inverse(mat);
}

float2x2 invert(float2x2 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0f;
  return r_success ? inverse(mat) : float2x2(0.0f);
}
float3x3 invert(float3x3 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0f;
  return r_success ? inverse(mat) : float3x3(0.0f);
}
float4x4 invert(float4x4 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0f;
  return r_success ? inverse(mat) : float4x4(0.0f);
}

#  if defined(GPU_OPENGL) || defined(GPU_METAL)
float2 normalize(float2 a)
{
  return a * inversesqrt(length_squared(a));
}
float3 normalize(float3 a)
{
  return a * inversesqrt(length_squared(a));
}
float4 normalize(float4 a)
{
  return a * inversesqrt(length_squared(a));
}
#  endif

float2x2 normalize(float2x2 mat)
{
  float2x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  return ret;
}
float2x3 normalize(float2x3 mat)
{
  float2x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  return ret;
}
float2x4 normalize(float2x4 mat)
{
  float2x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  return ret;
}
float3x2 normalize(float3x2 mat)
{
  float3x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  ret[2] = normalize(mat[2].xy);
  return ret;
}
float3x3 normalize(float3x3 mat)
{
  float3x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  ret[2] = normalize(mat[2].xyz);
  return ret;
}
float3x4 normalize(float3x4 mat)
{
  float3x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  ret[2] = normalize(mat[2].xyzw);
  return ret;
}
float4x2 normalize(float4x2 mat)
{
  float4x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  ret[2] = normalize(mat[2].xy);
  ret[3] = normalize(mat[3].xy);
  return ret;
}
float4x3 normalize(float4x3 mat)
{
  float4x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  ret[2] = normalize(mat[2].xyz);
  ret[3] = normalize(mat[3].xyz);
  return ret;
}
float4x4 normalize(float4x4 mat)
{
  float4x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  ret[2] = normalize(mat[2].xyzw);
  ret[3] = normalize(mat[3].xyzw);
  return ret;
}

float2x2 normalize_and_get_size(float2x2 mat, out float2 r_size)
{
  float size_x = 0.0f, size_y = 0.0f;
  float2x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = float2(size_x, size_y);
  return ret;
}
float2x3 normalize_and_get_size(float2x3 mat, out float2 r_size)
{
  float size_x = 0.0f, size_y = 0.0f;
  float2x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = float2(size_x, size_y);
  return ret;
}
float2x4 normalize_and_get_size(float2x4 mat, out float2 r_size)
{
  float size_x = 0.0f, size_y = 0.0f;
  float2x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = float2(size_x, size_y);
  return ret;
}
float3x2 normalize_and_get_size(float3x2 mat, out float3 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f;
  float3x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = float3(size_x, size_y, size_z);
  return ret;
}
float3x3 normalize_and_get_size(float3x3 mat, out float3 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f;
  float3x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = float3(size_x, size_y, size_z);
  return ret;
}
float3x4 normalize_and_get_size(float3x4 mat, out float3 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f;
  float3x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = float3(size_x, size_y, size_z);
  return ret;
}
float4x2 normalize_and_get_size(float4x2 mat, out float4 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f, size_w = 0.0f;
  float4x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = float4(size_x, size_y, size_z, size_w);
  return ret;
}
float4x3 normalize_and_get_size(float4x3 mat, out float4 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f, size_w = 0.0f;
  float4x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = float4(size_x, size_y, size_z, size_w);
  return ret;
}
float4x4 normalize_and_get_size(float4x4 mat, out float4 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f, size_w = 0.0f;
  float4x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = float4(size_x, size_y, size_z, size_w);
  return ret;
}

float2x2 adjoint(float2x2 mat)
{
  float2x2 adj = float2x2(0.0f);
  for (int c = 0; c < 2; c++) {
    for (int r = 0; r < 2; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float tmp = 0.0f;
      for (int m_c = 0; m_c < 2; m_c++) {
        for (int m_r = 0; m_r < 2; m_r++) {
          if (m_c != c && m_r != r) {
            tmp = mat[m_c][m_r];
          }
        }
      }
      float minor = tmp;
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = (((c + r) & 1) != 0) ? -minor : minor;
    }
  }
  return adj;
}
float3x3 adjoint(float3x3 mat)
{
  float3x3 adj = float3x3(0.0f);
  for (int c = 0; c < 3; c++) {
    for (int r = 0; r < 3; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float2x2 tmp = float2x2(0.0f);
      for (int m_c = 0; m_c < 3; m_c++) {
        for (int m_r = 0; m_r < 3; m_r++) {
          if (m_c != c && m_r != r) {
            int d_c = (m_c < c) ? m_c : (m_c - 1);
            int d_r = (m_r < r) ? m_r : (m_r - 1);
            tmp[d_c][d_r] = mat[m_c][m_r];
          }
        }
      }
      float minor = determinant(tmp);
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = (((c + r) & 1) != 0) ? -minor : minor;
    }
  }
  return adj;
}
float4x4 adjoint(float4x4 mat)
{
  float4x4 adj = float4x4(0.0f);
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float3x3 tmp = float3x3(0.0f);
      for (int m_c = 0; m_c < 4; m_c++) {
        for (int m_r = 0; m_r < 4; m_r++) {
          if (m_c != c && m_r != r) {
            int d_c = (m_c < c) ? m_c : (m_c - 1);
            int d_r = (m_r < r) ? m_r : (m_r - 1);
            tmp[d_c][d_r] = mat[m_c][m_r];
          }
        }
      }
      float minor = determinant(tmp);
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = (((c + r) & 1) != 0) ? -minor : minor;
    }
  }
  return adj;
}

float4x4 translate(float4x4 mat, float3 translation)
{
  mat[3].xyz += translation[0] * mat[0].xyz;
  mat[3].xyz += translation[1] * mat[1].xyz;
  mat[3].xyz += translation[2] * mat[2].xyz;
  return mat;
}
float4x4 translate(float4x4 mat, float2 translation)
{
  mat[3].xyz += translation[0] * mat[0].xyz;
  mat[3].xyz += translation[1] * mat[1].xyz;
  return mat;
}

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

float3x3 scale(float3x3 mat, float2 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  return mat;
}
float3x3 scale(float3x3 mat, float3 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  mat[2] *= scale[2];
  return mat;
}
float4x4 scale(float4x4 mat, float2 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  return mat;
}
float4x4 scale(float4x4 mat, float3 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  mat[2] *= scale[2];
  return mat;
}

float4x4 from_location(float3 location)
{
  float4x4 ret = float4x4(1.0f);
  ret[3].xyz = location;
  return ret;
}

float2x2 from_scale(float2 scale)
{
  float2x2 ret = float2x2(0.0f);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  return ret;
}
float3x3 from_scale(float3 scale)
{
  float3x3 ret = float3x3(0.0f);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  ret[2][2] = scale[2];
  return ret;
}
float4x4 from_scale(float4 scale)
{
  float4x4 ret = float4x4(0.0f);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  ret[2][2] = scale[2];
  ret[3][3] = scale[3];
  return ret;
}

float2x2 from_rotation(Angle rotation)
{
  float c = cos(rotation.angle);
  float s = sin(rotation.angle);
  return float2x2(c, -s, s, c);
}

float3x3 from_rotation(EulerXYZ rotation)
{
  float ci = cos(rotation.x);
  float cj = cos(rotation.y);
  float ch = cos(rotation.z);
  float si = sin(rotation.x);
  float sj = sin(rotation.y);
  float sh = sin(rotation.z);
  float cc = ci * ch;
  float cs = ci * sh;
  float sc = si * ch;
  float ss = si * sh;

  float3x3 mat;
  mat[0][0] = cj * ch;
  mat[1][0] = sj * sc - cs;
  mat[2][0] = sj * cc + ss;

  mat[0][1] = cj * sh;
  mat[1][1] = sj * ss + cc;
  mat[2][1] = sj * cs - sc;

  mat[0][2] = -sj;
  mat[1][2] = cj * si;
  mat[2][2] = cj * ci;
  return mat;
}

float3x3 from_rotation(Quaternion rotation)
{
  /* NOTE: Should be double but support isn't native on most GPUs. */
  float q0 = M_SQRT2 * float(rotation.x);
  float q1 = M_SQRT2 * float(rotation.y);
  float q2 = M_SQRT2 * float(rotation.z);
  float q3 = M_SQRT2 * float(rotation.w);

  float qda = q0 * q1;
  float qdb = q0 * q2;
  float qdc = q0 * q3;
  float qaa = q1 * q1;
  float qab = q1 * q2;
  float qac = q1 * q3;
  float qbb = q2 * q2;
  float qbc = q2 * q3;
  float qcc = q3 * q3;

  float3x3 mat;
  mat[0][0] = float(1.0f - qbb - qcc);
  mat[0][1] = float(qdc + qab);
  mat[0][2] = float(-qdb + qac);

  mat[1][0] = float(-qdc + qab);
  mat[1][1] = float(1.0f - qaa - qcc);
  mat[1][2] = float(qda + qbc);

  mat[2][0] = float(qdb + qac);
  mat[2][1] = float(-qda + qbc);
  mat[2][2] = float(1.0f - qaa - qbb);
  return mat;
}

float3x3 from_rotation(AxisAngle rotation)
{
  float angle_sin = sin(rotation.angle);
  float angle_cos = cos(rotation.angle);
  float3 axis = rotation.axis;

  float ico = (float(1) - angle_cos);
  float3 nsi = axis * angle_sin;

  float3 n012 = (axis * axis) * ico;
  float n_01 = (axis[0] * axis[1]) * ico;
  float n_02 = (axis[0] * axis[2]) * ico;
  float n_12 = (axis[1] * axis[2]) * ico;

  float3x3 mat = from_scale(n012 + angle_cos);
  mat[0][1] = n_01 + nsi[2];
  mat[0][2] = n_02 - nsi[1];
  mat[1][0] = n_01 - nsi[2];
  mat[1][2] = n_12 + nsi[0];
  mat[2][0] = n_02 + nsi[1];
  mat[2][1] = n_12 - nsi[0];
  return mat;
}

float3x3 from_rot_scale(EulerXYZ rotation, float3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}
float3x3 from_rot_scale(Quaternion rotation, float3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}
float3x3 from_rot_scale(AxisAngle rotation, float3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}

float4x4 from_loc_rot(float3 location, EulerXYZ rotation)
{
  float4x4 ret = to_float4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}
float4x4 from_loc_rot(float3 location, Quaternion rotation)
{
  float4x4 ret = to_float4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}
float4x4 from_loc_rot(float3 location, AxisAngle rotation)
{
  float4x4 ret = to_float4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}

float4x4 from_loc_rot_scale(float3 location, EulerXYZ rotation, float3 scale)
{
  float4x4 ret = to_float4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}
float4x4 from_loc_rot_scale(float3 location, Quaternion rotation, float3 scale)
{
  float4x4 ret = to_float4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}
float4x4 from_loc_rot_scale(float3 location, AxisAngle rotation, float3 scale)
{
  float4x4 ret = to_float4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}

float2x2 from_direction(float2 direction)
{
  float cos_angle = direction.x;
  float sin_angle = direction.y;
  return float2x2(cos_angle, sin_angle, -sin_angle, cos_angle);
}

float3x3 from_up_axis(float3 up)
{
  /* Duff, Tom, et al. "Building an orthonormal basis, revisited." JCGT 6.1 (2017). */
  float z_sign = up.z >= 0.0f ? 1.0f : -1.0f;
  float a = -1.0f / (z_sign + up.z);
  float b = up.x * up.y * a;

  float3x3 basis;
  basis[0] = float3(1.0f + z_sign * square(up.x) * a, z_sign * b, -z_sign * up.x);
  basis[1] = float3(b, z_sign + square(up.y) * a, -up.y);
  basis[2] = up;
  return basis;
}

void detail_normalized_to_eul2(float3x3 mat, out EulerXYZ eul1, out EulerXYZ eul2)
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

EulerXYZ to_euler(float3x3 mat)
{
  return to_euler(mat, true);
}
EulerXYZ to_euler(float3x3 mat, const bool normalized)
{
  if (!normalized) {
    mat = normalize(mat);
  }
  EulerXYZ eul1, eul2;
  detail_normalized_to_eul2(mat, eul1, eul2);
  /* Return best, which is just the one with lowest values it in. */
  return (length_manhattan(as_vec3(eul1)) > length_manhattan(as_vec3(eul2))) ? eul2 : eul1;
}
EulerXYZ to_euler(float4x4 mat)
{
  return to_euler(to_float3x3(mat));
}
EulerXYZ to_euler(float4x4 mat, const bool normalized)
{
  return to_euler(to_float3x3(mat), normalized);
}

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

Quaternion detail_normalized_to_quat_with_checks(float3x3 mat)
{
  float det = determinant(mat);
  if (!isfinite(det)) {
    return Quaternion_identity();
  }
  else if (det < 0.0f) {
    return normalized_to_quat_fast(-mat);
  }
  return normalized_to_quat_fast(mat);
}

Quaternion to_quaternion(float3x3 mat)
{
  return detail_normalized_to_quat_with_checks(normalize(mat));
}
Quaternion to_quaternion(float3x3 mat, const bool normalized)
{
  if (!normalized) {
    mat = normalize(mat);
  }
  return to_quaternion(mat);
}
Quaternion to_quaternion(float4x4 mat)
{
  return to_quaternion(to_float3x3(mat));
}
Quaternion to_quaternion(float4x4 mat, const bool normalized)
{
  return to_quaternion(to_float3x3(mat), normalized);
}

float3 to_scale(float3x3 mat)
{
  return float3(length(mat[0]), length(mat[1]), length(mat[2]));
}
float3 to_scale(float4x4 mat)
{
  return to_scale(to_float3x3(mat));
}
template<typename MatT, bool allow_negative_scale> float3 to_scale(MatT mat)
{
  float3 result = to_scale(mat);
  if (allow_negative_scale) {
    if (is_negative(mat)) {
      result = -result;
    }
  }
  return result;
}
template float3 to_scale<float3x3, true>(float3x3 mat);
template float3 to_scale<float3x3, false>(float3x3 mat);
template float3 to_scale<float4x4, true>(float4x4 mat);
template float3 to_scale<float4x4, false>(float4x4 mat);

void to_rot_scale(float3x3 mat, out EulerXYZ r_rotation, out float3 r_scale)
{
  r_scale = to_scale(mat);
  r_rotation = to_euler(mat, true);
}
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
  r_rotation = to_euler(mat, true);
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
  r_rotation = to_quaternion(mat, true);
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

float3 transform_point(float3x3 mat, float3 point)
{
  return mat * point;
}

float3 transform_point(float4x4 mat, float3 point)
{
  return (mat * float4(point, 1.0f)).xyz;
}

float3 transform_direction(float3x3 mat, float3 direction)
{
  return mat * direction;
}

float3 transform_direction(float4x4 mat, float3 direction)
{
  return to_float3x3(mat) * direction;
}

float2 project_point(float3x3 mat, float2 point)
{
  float3 tmp = mat * float3(point, 1.0f);
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return tmp.xy / abs(tmp.z);
}
float3 project_point(float4x4 mat, float3 point)
{
  float4 tmp = mat * float4(point, 1.0f);
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return tmp.xyz / abs(tmp.w);
}

float4x4 interpolate_fast(float4x4 a, float4x4 b, float t)
{
  float3 a_loc, b_loc;
  float3 a_scale, b_scale;
  Quaternion a_quat, b_quat;
  to_loc_rot_scale(a, a_loc, a_quat, a_scale);
  to_loc_rot_scale(b, b_loc, b_quat, b_scale);

  float3 location = interpolate(a_loc, b_loc, t);
  float3 scale = interpolate(a_scale, b_scale, t);
  Quaternion rotation = interpolate(a_quat, b_quat, t);
  return from_loc_rot_scale(location, rotation, scale);
}

float4x4 projection_orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip)
{
  float x_delta = right - left;
  float y_delta = top - bottom;
  float z_delta = far_clip - near_clip;

  float4x4 mat = float4x4(1.0f);
  if (x_delta != 0.0f && y_delta != 0.0f && z_delta != 0.0f) {
    mat[0][0] = 2.0f / x_delta;
    mat[3][0] = -(right + left) / x_delta;
    mat[1][1] = 2.0f / y_delta;
    mat[3][1] = -(top + bottom) / y_delta;
    mat[2][2] = -2.0f / z_delta; /* NOTE: negate Z. */
    mat[3][2] = -(far_clip + near_clip) / z_delta;
  }
  return mat;
}

float4x4 projection_perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip)
{
  float x_delta = right - left;
  float y_delta = top - bottom;
  float z_delta = far_clip - near_clip;

  float4x4 mat = float4x4(1.0f);
  if (x_delta != 0.0f && y_delta != 0.0f && z_delta != 0.0f) {
    mat[0][0] = near_clip * 2.0f / x_delta;
    mat[1][1] = near_clip * 2.0f / y_delta;
    mat[2][0] = (right + left) / x_delta; /* NOTE: negate Z. */
    mat[2][1] = (top + bottom) / y_delta;
    mat[2][2] = -(far_clip + near_clip) / z_delta;
    mat[2][3] = -1.0f;
    mat[3][2] = (-2.0f * near_clip * far_clip) / z_delta;
    mat[3][3] = 0.0f;
  }
  return mat;
}

float4x4 projection_perspective_fov(float angle_left,
                                    float angle_right,
                                    float angle_bottom,
                                    float angle_top,
                                    float near_clip,
                                    float far_clip)
{
  float4x4 mat = projection_perspective(
      tan(angle_left), tan(angle_right), tan(angle_bottom), tan(angle_top), near_clip, far_clip);
  mat[0][0] /= near_clip;
  mat[1][1] /= near_clip;
  return mat;
}

bool is_zero(float3x3 a)
{
  if (is_zero(a[0])) {
    if (is_zero(a[1])) {
      if (is_zero(a[2])) {
        return true;
      }
    }
  }
  return false;
}
bool is_zero(float4x4 a)
{
  if (is_zero(a[0])) {
    if (is_zero(a[1])) {
      if (is_zero(a[2])) {
        if (is_zero(a[3])) {
          return true;
        }
      }
    }
  }
  return false;
}

bool is_negative(float3x3 mat)
{
  return determinant(mat) < 0.0f;
}
bool is_negative(float4x4 mat)
{
  return is_negative(to_float3x3(mat));
}

bool is_equal(float2x2 a, float2x2 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      return true;
    }
  }
  return false;
}
bool is_equal(float3x3 a, float3x3 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      if (is_equal(a[2], b[2], epsilon)) {
        return true;
      }
    }
  }
  return false;
}
bool is_equal(float4x4 a, float4x4 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      if (is_equal(a[2], b[2], epsilon)) {
        if (is_equal(a[3], b[3], epsilon)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool is_orthogonal(float3x3 mat)
{
  if (abs(dot(mat[0], mat[1])) > 1e-5f) {
    return false;
  }
  if (abs(dot(mat[1], mat[2])) > 1e-5f) {
    return false;
  }
  if (abs(dot(mat[2], mat[0])) > 1e-5f) {
    return false;
  }
  return true;
}

bool is_orthonormal(float3x3 mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  if (abs(length_squared(mat[0]) - 1.0f) > 1e-5f) {
    return false;
  }
  if (abs(length_squared(mat[1]) - 1.0f) > 1e-5f) {
    return false;
  }
  if (abs(length_squared(mat[2]) - 1.0f) > 1e-5f) {
    return false;
  }
  return true;
}

bool is_uniformly_scaled(float3x3 mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  constexpr float eps = 1e-7f;
  float x = length_squared(mat[0]);
  float y = length_squared(mat[1]);
  float z = length_squared(mat[2]);
  return (abs(x - y) < eps) && abs(x - z) < eps;
}

bool is_orthogonal(float4x4 mat)
{
  return is_orthogonal(to_float3x3(mat));
}
bool is_orthonormal(float4x4 mat)
{
  return is_orthonormal(to_float3x3(mat));
}
bool is_uniformly_scaled(float4x4 mat)
{
  return is_uniformly_scaled(to_float3x3(mat));
}

/* Returns true if each individual columns are unit scaled. Mainly for assert usage. */
bool is_unit_scale(float4x4 m)
{
  if (is_unit_scale(m[0])) {
    if (is_unit_scale(m[1])) {
      if (is_unit_scale(m[2])) {
        if (is_unit_scale(m[3])) {
          return true;
        }
      }
    }
  }
  return false;
}
bool is_unit_scale(float3x3 m)
{
  if (is_unit_scale(m[0])) {
    if (is_unit_scale(m[1])) {
      if (is_unit_scale(m[2])) {
        return true;
      }
    }
  }
  return false;
}
bool is_unit_scale(float2x2 m)
{
  if (is_unit_scale(m[0])) {
    if (is_unit_scale(m[1])) {
      return true;
    }
  }
  return false;
}

/** \} */

#endif /* GPU_SHADER_MATH_MATRIX_LIB_GLSL */
