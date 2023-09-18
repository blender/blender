/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_rotation_lib.glsl)

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_MATRIX_LIB_GLSL
#  define GPU_SHADER_MATH_MATRIX_LIB_GLSL

/* -------------------------------------------------------------------- */
/** \name Static constructors
 * \{ */

mat2x2 mat2x2_diagonal(float v)
{
  return mat2x2(vec2(v, 0.0), vec2(0.0, v));
}
mat3x3 mat3x3_diagonal(float v)
{
  return mat3x3(vec3(v, 0.0, 0.0), vec3(0.0, v, 0.0), vec3(0.0, 0.0, v));
}
mat4x4 mat4x4_diagonal(float v)
{
  return mat4x4(vec4(v, 0.0, 0.0, 0.0),
                vec4(0.0, v, 0.0, 0.0),
                vec4(0.0, 0.0, v, 0.0),
                vec4(0.0, 0.0, 0.0, v));
}

mat2x2 mat2x2_all(float v)
{
  return mat2x2(vec2(v), vec2(v));
}
mat3x3 mat3x3_all(float v)
{
  return mat3x3(vec3(v), vec3(v), vec3(v));
}
mat4x4 mat4x4_all(float v)
{
  return mat4x4(vec4(v), vec4(v), vec4(v), vec4(v));
}

mat2x2 mat2x2_zero(float v)
{
  return mat2x2_all(0.0);
}
mat3x3 mat3x3_zero(float v)
{
  return mat3x3_all(0.0);
}
mat4x4 mat4x4_zero(float v)
{
  return mat4x4_all(0.0);
}

mat2x2 mat2x2_identity()
{
  return mat2x2_diagonal(1.0);
}
mat3x3 mat3x3_identity()
{
  return mat3x3_diagonal(1.0);
}
mat4x4 mat4x4_identity()
{
  return mat4x4_diagonal(1.0);
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
mat2x2 invert(mat2x2 mat);
mat3x3 invert(mat3x3 mat);
mat4x4 invert(mat4x4 mat);

mat2x2 invert(mat2x2 mat, out bool r_success);
mat3x3 invert(mat3x3 mat, out bool r_success);
mat4x4 invert(mat4x4 mat, out bool r_success);

/**
 * Flip the matrix across its diagonal. Also flips dimensions for non square matrices.
 */
// mat3x3 transpose(mat3x3 mat); /* Built-In in GLSL language. */

/**
 * Normalize each column of the matrix individually.
 */
mat2x2 normalize(mat2x2 mat);
mat2x3 normalize(mat2x3 mat);
mat2x4 normalize(mat2x4 mat);
mat3x2 normalize(mat3x2 mat);
mat3x3 normalize(mat3x3 mat);
mat3x4 normalize(mat3x4 mat);
mat4x2 normalize(mat4x2 mat);
mat4x3 normalize(mat4x3 mat);
mat4x4 normalize(mat4x4 mat);

/**
 * Normalize each column of the matrix individually.
 * Return the length of each column vector.
 */
mat2x2 normalize_and_get_size(mat2x2 mat, out vec2 r_size);
mat2x3 normalize_and_get_size(mat2x3 mat, out vec2 r_size);
mat2x4 normalize_and_get_size(mat2x4 mat, out vec2 r_size);
mat3x2 normalize_and_get_size(mat3x2 mat, out vec3 r_size);
mat3x3 normalize_and_get_size(mat3x3 mat, out vec3 r_size);
mat3x4 normalize_and_get_size(mat3x4 mat, out vec3 r_size);
mat4x2 normalize_and_get_size(mat4x2 mat, out vec4 r_size);
mat4x3 normalize_and_get_size(mat4x3 mat, out vec4 r_size);
mat4x4 normalize_and_get_size(mat4x4 mat, out vec4 r_size);

/**
 * Returns the determinant of the matrix.
 * It can be interpreted as the signed volume (or area) of the unit cube after transformation.
 */
// float determinant(mat3x3 mat); /* Built-In in GLSL language. */

/**
 * Returns the adjoint of the matrix (also known as adjugate matrix).
 */
mat2x2 adjoint(mat2x2 mat);
mat3x3 adjoint(mat3x3 mat);
mat4x4 adjoint(mat4x4 mat);

/**
 * Equivalent to `mat * from_location(translation)` but with fewer operation.
 */
mat4x4 translate(mat4x4 mat, vec2 translation);
mat4x4 translate(mat4x4 mat, vec3 translation);

/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2)).
 */
mat3x3 rotate(mat3x3 mat, AxisAngle rotation);
mat3x3 rotate(mat3x3 mat, EulerXYZ rotation);
mat4x4 rotate(mat4x4 mat, AxisAngle rotation);
mat4x4 rotate(mat4x4 mat, EulerXYZ rotation);

/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
mat3x3 scale(mat3x3 mat, vec2 scale);
mat3x3 scale(mat3x3 mat, vec3 scale);
mat4x4 scale(mat4x4 mat, vec2 scale);
mat4x4 scale(mat4x4 mat, vec3 scale);

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
mat3x3 interpolate_fast(mat3x3 a, mat3x3 b, float t);

/**
 * Naive transform matrix interpolation,
 * based on naive-decomposition-based interpolation from #interpolate_fast<T, 3, 3>.
 */
mat4x4 interpolate_fast(mat4x4 a, mat4x4 b, float t);

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
mat4x4 from_location(vec3 location);

/**
 * Create a matrix whose diagonal is defined by the given scale vector.
 */
mat2x2 from_scale(vec2 scale);
mat3x3 from_scale(vec3 scale);
mat4x4 from_scale(vec4 scale);

/**
 * Create a rotation only matrix.
 */
mat2x2 from_rotation(Angle rotation);
mat3x3 from_rotation(EulerXYZ rotation);
mat3x3 from_rotation(Quaternion rotation);
mat3x3 from_rotation(AxisAngle rotation);

/**
 * Create a transform matrix with rotation and scale applied in this order.
 */
mat3x3 from_rot_scale(EulerXYZ rotation, vec3 scale);

/**
 * Create a transform matrix with translation and rotation applied in this order.
 */
mat4x4 from_loc_rot(vec3 location, EulerXYZ rotation);

/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
mat4x4 from_loc_rot_scale(vec3 location, EulerXYZ rotation, vec3 scale);

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion function.
 * \{ */

/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
EulerXYZ to_euler(mat3x3 mat);
EulerXYZ to_euler(mat3x3 mat, const bool normalized);
EulerXYZ to_euler(mat4x4 mat);
EulerXYZ to_euler(mat4x4 mat, const bool normalized);

/**
 * Extract quaternion rotation from transform matrix.
 * \note normalized is set to false by default.
 */
Quaternion to_quaternion(mat3x3 mat);
Quaternion to_quaternion(mat3x3 mat, const bool normalized);
Quaternion to_quaternion(mat4x4 mat);
Quaternion to_quaternion(mat4x4 mat, const bool normalized);

/**
 * Extract the absolute 3d scale from a transform matrix.
 */
vec3 to_scale(mat3x3 mat);
vec3 to_scale(mat3x3 mat, const bool allow_negative_scale);
vec3 to_scale(mat4x4 mat);
vec3 to_scale(mat4x4 mat, const bool allow_negative_scale);

/**
 * Decompose a matrix into location, rotation, and scale components.
 * \tparam allow_negative_scale: if true, will compute determinant to know if matrix is negative.
 * Rotation and scale values will be flipped if it is negative.
 * This is a costly operation so it is disabled by default.
 */
void to_rot_scale(mat3x3 mat, out EulerXYZ r_rotation, out vec3 r_scale);
void to_rot_scale(mat3x3 mat, out Quaternion r_rotation, out vec3 r_scale);
void to_rot_scale(mat3x3 mat, out AxisAngle r_rotation, out vec3 r_scale);
void to_loc_rot_scale(mat4x4 mat, out vec3 r_location, out EulerXYZ r_rotation, out vec3 r_scale);
void to_loc_rot_scale(mat4x4 mat,
                      out vec3 r_location,
                      out Quaternion r_rotation,
                      out vec3 r_scale);
void to_loc_rot_scale(mat4x4 mat, out vec3 r_location, out AxisAngle r_rotation, out vec3 r_scale);

void to_rot_scale(mat3x3 mat,
                  out EulerXYZ r_rotation,
                  out vec3 r_scale,
                  const bool allow_negative_scale);
void to_rot_scale(mat3x3 mat,
                  out Quaternion r_rotation,
                  out vec3 r_scale,
                  const bool allow_negative_scale);
void to_rot_scale(mat3x3 mat,
                  out AxisAngle r_rotation,
                  out vec3 r_scale,
                  const bool allow_negative_scale);
void to_loc_rot_scale(mat4x4 mat,
                      out vec3 r_location,
                      out EulerXYZ r_rotation,
                      out vec3 r_scale,
                      const bool allow_negative_scale);
void to_loc_rot_scale(mat4x4 mat,
                      out vec3 r_location,
                      out Quaternion r_rotation,
                      out vec3 r_scale,
                      const bool allow_negative_scale);
void to_loc_rot_scale(mat4x4 mat,
                      out vec3 r_location,
                      out AxisAngle r_rotation,
                      out vec3 r_scale,
                      const bool allow_negative_scale);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform function.
 * \{ */

/**
 * Transform a 3d point using a 3x3 matrix (rotation & scale).
 */
vec3 transform_point(mat3x3 mat, vec3 point);

/**
 * Transform a 3d point using a 4x4 matrix (location & rotation & scale).
 */
vec3 transform_point(mat4x4 mat, vec3 point);

/**
 * Transform a 3d direction vector using a 3x3 matrix (rotation & scale).
 */
vec3 transform_direction(mat3x3 mat, vec3 direction);

/**
 * Transform a 3d direction vector using a 4x4 matrix (rotation & scale).
 */
vec3 transform_direction(mat4x4 mat, vec3 direction);

/**
 * Project a point using a matrix (location & rotation & scale & perspective divide).
 */
vec2 project_point(mat3x3 mat, vec2 point);
vec3 project_point(mat4x4 mat, vec3 point);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Projection Matrices.
 * \{ */

/**
 * \brief Create an orthographic projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * The resulting matrix can be used with either #project_point or #transform_point.
 */
mat4x4 projection_orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip);

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * `left`, `right`, `bottom`, `top` are frustum side distances at `z=near_clip`.
 * The resulting matrix can be used with #project_point.
 */
mat4x4 projection_perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip);

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * Uses field of view angles instead of plane distances.
 * The resulting matrix can be used with #project_point.
 */
mat4x4 projection_perspective_fov(float angle_left,
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
bool is_zero(mat3x3 a);
bool is_zero(mat4x4 a);

/**
 * Returns true if matrix has inverted handedness.
 *
 * \note It doesn't use determinant(mat4x4) as only the 3x3 components are needed
 * when the matrix is used as a transformation to represent location/scale/rotation.
 */
bool is_negative(mat3x3 mat);
bool is_negative(mat4x4 mat);

/**
 * Returns true if matrices are equal within the given epsilon.
 */
bool is_equal(mat2x2 a, mat2x2 b, float epsilon);
bool is_equal(mat3x3 a, mat3x3 b, float epsilon);
bool is_equal(mat4x4 a, mat4x4 b, float epsilon);

/**
 * Test if the X, Y and Z axes are perpendicular with each other.
 */
bool is_orthogonal(mat3x3 mat);
bool is_orthogonal(mat4x4 mat);

/**
 * Test if the X, Y and Z axes are perpendicular with each other and unit length.
 */
bool is_orthonormal(mat3x3 mat);
bool is_orthonormal(mat4x4 mat);

/**
 * Test if the X, Y and Z axes are perpendicular with each other and the same length.
 */
bool is_uniformly_scaled(mat3x3 mat);

/** \} */

#  endif /* GPU_METAL */

/* ---------------------------------------------------------------------- */
/** \name Implementation
 * \{ */

mat2x2 invert(mat2x2 mat)
{
  return inverse(mat);
}
mat3x3 invert(mat3x3 mat)
{
  return inverse(mat);
}
mat4x4 invert(mat4x4 mat)
{
  return inverse(mat);
}

mat2x2 invert(mat2x2 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0;
  return r_success ? inverse(mat) : mat2x2(0.0);
}
mat3x3 invert(mat3x3 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0;
  return r_success ? inverse(mat) : mat3x3(0.0);
}
mat4x4 invert(mat4x4 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0;
  return r_success ? inverse(mat) : mat4x4(0.0);
}

#  if defined(GPU_OPENGL) || defined(GPU_METAL)
vec2 normalize(vec2 a)
{
  return a * inversesqrt(length_squared(a));
}
vec3 normalize(vec3 a)
{
  return a * inversesqrt(length_squared(a));
}
vec4 normalize(vec4 a)
{
  return a * inversesqrt(length_squared(a));
}
#  endif

mat2x2 normalize(mat2x2 mat)
{
  mat2x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  return ret;
}
mat2x3 normalize(mat2x3 mat)
{
  mat2x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  return ret;
}
mat2x4 normalize(mat2x4 mat)
{
  mat2x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  return ret;
}
mat3x2 normalize(mat3x2 mat)
{
  mat3x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  ret[2] = normalize(mat[2].xy);
  return ret;
}
mat3x3 normalize(mat3x3 mat)
{
  mat3x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  ret[2] = normalize(mat[2].xyz);
  return ret;
}
mat3x4 normalize(mat3x4 mat)
{
  mat3x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  ret[2] = normalize(mat[2].xyzw);
  return ret;
}
mat4x2 normalize(mat4x2 mat)
{
  mat4x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  ret[2] = normalize(mat[2].xy);
  ret[3] = normalize(mat[3].xy);
  return ret;
}
mat4x3 normalize(mat4x3 mat)
{
  mat4x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  ret[2] = normalize(mat[2].xyz);
  ret[3] = normalize(mat[3].xyz);
  return ret;
}
mat4x4 normalize(mat4x4 mat)
{
  mat4x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  ret[2] = normalize(mat[2].xyzw);
  ret[3] = normalize(mat[3].xyzw);
  return ret;
}

mat2x2 normalize_and_get_size(mat2x2 mat, out vec2 r_size)
{
  float size_x, size_y;
  mat2x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = vec2(size_x, size_y);
  return ret;
}
mat2x3 normalize_and_get_size(mat2x3 mat, out vec2 r_size)
{
  float size_x, size_y;
  mat2x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = vec2(size_x, size_y);
  return ret;
}
mat2x4 normalize_and_get_size(mat2x4 mat, out vec2 r_size)
{
  float size_x, size_y;
  mat2x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = vec2(size_x, size_y);
  return ret;
}
mat3x2 normalize_and_get_size(mat3x2 mat, out vec3 r_size)
{
  float size_x, size_y, size_z;
  mat3x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = vec3(size_x, size_y, size_z);
  return ret;
}
mat3x3 normalize_and_get_size(mat3x3 mat, out vec3 r_size)
{
  float size_x, size_y, size_z;
  mat3x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = vec3(size_x, size_y, size_z);
  return ret;
}
mat3x4 normalize_and_get_size(mat3x4 mat, out vec3 r_size)
{
  float size_x, size_y, size_z;
  mat3x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = vec3(size_x, size_y, size_z);
  return ret;
}
mat4x2 normalize_and_get_size(mat4x2 mat, out vec4 r_size)
{
  float size_x, size_y, size_z, size_w;
  mat4x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = vec4(size_x, size_y, size_z, size_w);
  return ret;
}
mat4x3 normalize_and_get_size(mat4x3 mat, out vec4 r_size)
{
  float size_x, size_y, size_z, size_w;
  mat4x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = vec4(size_x, size_y, size_z, size_w);
  return ret;
}
mat4x4 normalize_and_get_size(mat4x4 mat, out vec4 r_size)
{
  float size_x, size_y, size_z, size_w;
  mat4x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = vec4(size_x, size_y, size_z, size_w);
  return ret;
}

mat2x2 adjoint(mat2x2 mat)
{
  mat2x2 adj = mat2x2(0.0);
  for (int c = 0; c < 2; c++) {
    for (int r = 0; r < 2; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float tmp = 0.0;
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
mat3x3 adjoint(mat3x3 mat)
{
  mat3x3 adj = mat3x3(0.0);
  for (int c = 0; c < 3; c++) {
    for (int r = 0; r < 3; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      mat2x2 tmp = mat2x2(0.0);
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
mat4x4 adjoint(mat4x4 mat)
{
  mat4x4 adj = mat4x4(0.0);
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      mat3x3 tmp = mat3x3(0.0);
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

mat4x4 translate(mat4x4 mat, vec3 translation)
{
  mat[3].xyz += translation[0] * mat[0].xyz;
  mat[3].xyz += translation[1] * mat[1].xyz;
  mat[3].xyz += translation[2] * mat[2].xyz;
  return mat;
}
mat4x4 translate(mat4x4 mat, vec2 translation)
{
  mat[3].xyz += translation[0] * mat[0].xyz;
  mat[3].xyz += translation[1] * mat[1].xyz;
  return mat;
}

mat3x3 rotate(mat3x3 mat, AxisAngle rotation)
{
  mat3x3 result;
  /* axis_vec is given to be normalized. */
  if (rotation.axis.x == 1.0) {
    float angle_cos = cos(rotation.angle);
    float angle_sin = sin(rotation.angle);
    for (int c = 0; c < 3; c++) {
      result[0][c] = mat[0][c];
      result[1][c] = angle_cos * mat[1][c] + angle_sin * mat[2][c];
      result[2][c] = -angle_sin * mat[1][c] + angle_cos * mat[2][c];
    }
  }
  else if (rotation.axis.y == 1.0) {
    float angle_cos = cos(rotation.angle);
    float angle_sin = sin(rotation.angle);
    for (int c = 0; c < 3; c++) {
      result[0][c] = angle_cos * mat[0][c] - angle_sin * mat[2][c];
      result[1][c] = mat[1][c];
      result[2][c] = angle_sin * mat[0][c] + angle_cos * mat[2][c];
    }
  }
  else if (rotation.axis.z == 1.0) {
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
mat3x3 rotate(mat3x3 mat, EulerXYZ rotation)
{
  AxisAngle axis_angle;
  if (rotation.y == 0.0 && rotation.z == 0.0) {
    axis_angle = AxisAngle(vec3(1.0, 0.0, 0.0), rotation.x);
  }
  else if (rotation.x == 0.0 && rotation.z == 0.0) {
    axis_angle = AxisAngle(vec3(0.0, 1.0, 0.0), rotation.y);
  }
  else if (rotation.x == 0.0 && rotation.y == 0.0) {
    axis_angle = AxisAngle(vec3(0.0, 0.0, 1.0), rotation.z);
  }
  else {
    /* Un-optimized case. Arbitrary rotation. */
    return mat * from_rotation(rotation);
  }
  return rotate(mat, axis_angle);
}

mat4x4 rotate(mat4x4 mat, AxisAngle rotation)
{
  mat4x4 result = mat4x4(rotate(mat3x3(mat), rotation));
  result[0][3] = mat[0][3];
  result[1][3] = mat[1][3];
  result[2][3] = mat[2][3];
  result[3][0] = mat[3][0];
  result[3][1] = mat[3][1];
  result[3][2] = mat[3][2];
  result[3][3] = mat[3][3];
  return result;
}
mat4x4 rotate(mat4x4 mat, EulerXYZ rotation)
{
  mat4x4 result = mat4x4(rotate(mat3x3(mat), rotation));
  result[0][3] = mat[0][3];
  result[1][3] = mat[1][3];
  result[2][3] = mat[2][3];
  result[3][0] = mat[3][0];
  result[3][1] = mat[3][1];
  result[3][2] = mat[3][2];
  result[3][3] = mat[3][3];
  return result;
}

mat3x3 scale(mat3x3 mat, vec2 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  return mat;
}
mat3x3 scale(mat3x3 mat, vec3 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  mat[2] *= scale[2];
  return mat;
}
mat4x4 scale(mat4x4 mat, vec2 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  return mat;
}
mat4x4 scale(mat4x4 mat, vec3 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  mat[2] *= scale[2];
  return mat;
}

mat4x4 from_location(vec3 location)
{
  mat4x4 ret = mat4x4(1.0);
  ret[3].xyz = location;
  return ret;
}

mat2x2 from_scale(vec2 scale)
{
  mat2x2 ret = mat2x2(0.0);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  return ret;
}
mat3x3 from_scale(vec3 scale)
{
  mat3x3 ret = mat3x3(0.0);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  ret[2][2] = scale[2];
  return ret;
}
mat4x4 from_scale(vec4 scale)
{
  mat4x4 ret = mat4x4(0.0);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  ret[2][2] = scale[2];
  ret[3][3] = scale[3];
  return ret;
}

mat2x2 from_rotation(Angle rotation)
{
  float c = cos(rotation.angle);
  float s = sin(rotation.angle);
  return mat2x2(c, -s, s, c);
}

mat3x3 from_rotation(EulerXYZ rotation)
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

  mat3x3 mat;
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

mat3x3 from_rotation(Quaternion rotation)
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

  mat3x3 mat;
  mat[0][0] = float(1.0 - qbb - qcc);
  mat[0][1] = float(qdc + qab);
  mat[0][2] = float(-qdb + qac);

  mat[1][0] = float(-qdc + qab);
  mat[1][1] = float(1.0 - qaa - qcc);
  mat[1][2] = float(qda + qbc);

  mat[2][0] = float(qdb + qac);
  mat[2][1] = float(-qda + qbc);
  mat[2][2] = float(1.0 - qaa - qbb);
  return mat;
}

mat3x3 from_rotation(AxisAngle rotation)
{
  float angle_sin = sin(rotation.angle);
  float angle_cos = cos(rotation.angle);
  vec3 axis = rotation.axis;

  float ico = (float(1) - angle_cos);
  vec3 nsi = axis * angle_sin;

  vec3 n012 = (axis * axis) * ico;
  float n_01 = (axis[0] * axis[1]) * ico;
  float n_02 = (axis[0] * axis[2]) * ico;
  float n_12 = (axis[1] * axis[2]) * ico;

  mat3 mat = from_scale(n012 + angle_cos);
  mat[0][1] = n_01 + nsi[2];
  mat[0][2] = n_02 - nsi[1];
  mat[1][0] = n_01 - nsi[2];
  mat[1][2] = n_12 + nsi[0];
  mat[2][0] = n_02 + nsi[1];
  mat[2][1] = n_12 - nsi[0];
  return mat;
}

mat3x3 from_rot_scale(EulerXYZ rotation, vec3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}
mat3x3 from_rot_scale(Quaternion rotation, vec3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}
mat3x3 from_rot_scale(AxisAngle rotation, vec3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}

mat4x4 from_loc_rot(vec3 location, EulerXYZ rotation)
{
  mat4x4 ret = mat4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}
mat4x4 from_loc_rot(vec3 location, Quaternion rotation)
{
  mat4x4 ret = mat4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}
mat4x4 from_loc_rot(vec3 location, AxisAngle rotation)
{
  mat4x4 ret = mat4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}

mat4x4 from_loc_rot_scale(vec3 location, EulerXYZ rotation, vec3 scale)
{
  mat4x4 ret = mat4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}
mat4x4 from_loc_rot_scale(vec3 location, Quaternion rotation, vec3 scale)
{
  mat4x4 ret = mat4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}
mat4x4 from_loc_rot_scale(vec3 location, AxisAngle rotation, vec3 scale)
{
  mat4x4 ret = mat4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}

void detail_normalized_to_eul2(mat3 mat, out EulerXYZ eul1, out EulerXYZ eul2)
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
    eul1.z = 0.0;

    eul2 = eul1;
  }
}

EulerXYZ to_euler(mat3x3 mat)
{
  return to_euler(mat, true);
}
EulerXYZ to_euler(mat3x3 mat, const bool normalized)
{
  if (!normalized) {
    mat = normalize(mat);
  }
  EulerXYZ eul1, eul2;
  detail_normalized_to_eul2(mat, eul1, eul2);
  /* Return best, which is just the one with lowest values it in. */
  return (length_manhattan(as_vec3(eul1)) > length_manhattan(as_vec3(eul2))) ? eul2 : eul1;
}
EulerXYZ to_euler(mat4x4 mat)
{
  return to_euler(mat3(mat));
}
EulerXYZ to_euler(mat4x4 mat, const bool normalized)
{
  return to_euler(mat3(mat), normalized);
}

Quaternion normalized_to_quat_fast(mat3 mat)
{
  /* Caller must ensure matrices aren't negative for valid results, see: #24291, #94231. */
  Quaternion q;

  /* Method outlined by Mike Day, ref: https://math.stackexchange.com/a/3183435/220949
   * with an additional `sqrtf(..)` for higher precision result.
   * Removing the `sqrt` causes tests to fail unless the precision is set to 1e-6 or larger. */

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

Quaternion detail_normalized_to_quat_with_checks(mat3x3 mat)
{
  float det = determinant(mat);
  if (!isfinite(det)) {
    return Quaternion_identity();
  }
  else if (det < 0.0) {
    return normalized_to_quat_fast(-mat);
  }
  return normalized_to_quat_fast(mat);
}

Quaternion to_quaternion(mat3x3 mat)
{
  return detail_normalized_to_quat_with_checks(normalize(mat));
}
Quaternion to_quaternion(mat3x3 mat, const bool normalized)
{
  if (!normalized) {
    mat = normalize(mat);
  }
  return to_quaternion(mat);
}
Quaternion to_quaternion(mat4x4 mat)
{
  return to_quaternion(mat3(mat));
}
Quaternion to_quaternion(mat4x4 mat, const bool normalized)
{
  return to_quaternion(mat3(mat), normalized);
}

vec3 to_scale(mat3x3 mat)
{
  return vec3(length(mat[0]), length(mat[1]), length(mat[2]));
}
vec3 to_scale(mat3x3 mat, const bool allow_negative_scale)
{
  vec3 result = to_scale(mat);
  if (allow_negative_scale) {
    if (is_negative(mat)) {
      result = -result;
    }
  }
  return result;
}
vec3 to_scale(mat4x4 mat)
{
  return to_scale(mat3(mat));
}
vec3 to_scale(mat4x4 mat, const bool allow_negative_scale)
{
  return to_scale(mat3(mat), allow_negative_scale);
}

void to_rot_scale(mat3x3 mat, out EulerXYZ r_rotation, out vec3 r_scale)
{
  r_scale = to_scale(mat);
  r_rotation = to_euler(mat, true);
}
void to_rot_scale(mat3x3 mat,
                  out EulerXYZ r_rotation,
                  out vec3 r_scale,
                  const bool allow_negative_scale)
{
  mat3x3 normalized_mat = normalize_and_get_size(mat, r_scale);
  if (allow_negative_scale) {
    if (is_negative(normalized_mat)) {
      normalized_mat = -normalized_mat;
      r_scale = -r_scale;
    }
  }
  r_rotation = to_euler(mat, true);
}
void to_rot_scale(mat3x3 mat, out Quaternion r_rotation, out vec3 r_scale)
{
  r_scale = to_scale(mat);
  r_rotation = to_quaternion(mat, true);
}
void to_rot_scale(mat3x3 mat,
                  out Quaternion r_rotation,
                  out vec3 r_scale,
                  const bool allow_negative_scale)
{
  mat3x3 normalized_mat = normalize_and_get_size(mat, r_scale);
  if (allow_negative_scale) {
    if (is_negative(normalized_mat)) {
      normalized_mat = -normalized_mat;
      r_scale = -r_scale;
    }
  }
  r_rotation = to_quaternion(mat, true);
}

void to_loc_rot_scale(mat4x4 mat, out vec3 r_location, out EulerXYZ r_rotation, out vec3 r_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(mat3(mat), r_rotation, r_scale);
}
void to_loc_rot_scale(mat4x4 mat,
                      out vec3 r_location,
                      out EulerXYZ r_rotation,
                      out vec3 r_scale,
                      const bool allow_negative_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(mat3(mat), r_rotation, r_scale, allow_negative_scale);
}
void to_loc_rot_scale(mat4x4 mat, out vec3 r_location, out Quaternion r_rotation, out vec3 r_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(mat3(mat), r_rotation, r_scale);
}
void to_loc_rot_scale(mat4x4 mat,
                      out vec3 r_location,
                      out Quaternion r_rotation,
                      out vec3 r_scale,
                      const bool allow_negative_scale)
{
  r_location = mat[3].xyz;
  to_rot_scale(mat3(mat), r_rotation, r_scale, allow_negative_scale);
}

vec3 transform_point(mat3x3 mat, vec3 point)
{
  return mat * point;
}

vec3 transform_point(mat4x4 mat, vec3 point)
{
  return (mat * vec4(point, 1.0)).xyz;
}

vec3 transform_direction(mat3x3 mat, vec3 direction)
{
  return mat * direction;
}

vec3 transform_direction(mat4x4 mat, vec3 direction)
{
  return mat3x3(mat) * direction;
}

vec2 project_point(mat3x3 mat, vec2 point)
{
  vec3 tmp = mat * vec3(point, 1.0);
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return tmp.xy / abs(tmp.z);
}
vec3 project_point(mat4x4 mat, vec3 point)
{
  vec4 tmp = mat * vec4(point, 1.0);
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return tmp.xyz / abs(tmp.w);
}

mat4x4 interpolate_fast(mat4x4 a, mat4x4 b, float t)
{
  vec3 a_loc, b_loc;
  vec3 a_scale, b_scale;
  Quaternion a_quat, b_quat;
  to_loc_rot_scale(a, a_loc, a_quat, a_scale);
  to_loc_rot_scale(b, b_loc, b_quat, b_scale);

  vec3 location = interpolate(a_loc, b_loc, t);
  vec3 scale = interpolate(a_scale, b_scale, t);
  Quaternion rotation = interpolate(a_quat, b_quat, t);
  return from_loc_rot_scale(location, rotation, scale);
}

mat4x4 projection_orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip)
{
  float x_delta = right - left;
  float y_delta = top - bottom;
  float z_delta = far_clip - near_clip;

  mat4x4 mat = mat4x4(1.0);
  if (x_delta != 0.0 && y_delta != 0.0 && z_delta != 0.0) {
    mat[0][0] = 2.0 / x_delta;
    mat[3][0] = -(right + left) / x_delta;
    mat[1][1] = 2.0 / y_delta;
    mat[3][1] = -(top + bottom) / y_delta;
    mat[2][2] = -2.0 / z_delta; /* NOTE: negate Z. */
    mat[3][2] = -(far_clip + near_clip) / z_delta;
  }
  return mat;
}

mat4x4 projection_perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip)
{
  float x_delta = right - left;
  float y_delta = top - bottom;
  float z_delta = far_clip - near_clip;

  mat4x4 mat = mat4x4(1.0);
  if (x_delta != 0.0 && y_delta != 0.0 && z_delta != 0.0) {
    mat[0][0] = near_clip * 2.0 / x_delta;
    mat[1][1] = near_clip * 2.0 / y_delta;
    mat[2][0] = (right + left) / x_delta; /* NOTE: negate Z. */
    mat[2][1] = (top + bottom) / y_delta;
    mat[2][2] = -(far_clip + near_clip) / z_delta;
    mat[2][3] = -1.0;
    mat[3][2] = (-2.0 * near_clip * far_clip) / z_delta;
    mat[3][3] = 0.0;
  }
  return mat;
}

mat4x4 projection_perspective_fov(float angle_left,
                                  float angle_right,
                                  float angle_bottom,
                                  float angle_top,
                                  float near_clip,
                                  float far_clip)
{
  mat4x4 mat = projection_perspective(
      tan(angle_left), tan(angle_right), tan(angle_bottom), tan(angle_top), near_clip, far_clip);
  mat[0][0] /= near_clip;
  mat[1][1] /= near_clip;
  return mat;
}

bool is_zero(mat3x3 a)
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
bool is_zero(mat4x4 a)
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

bool is_negative(mat3x3 mat)
{
  return determinant(mat) < 0.0;
}
bool is_negative(mat4x4 mat)
{
  return is_negative(mat3x3(mat));
}

bool is_equal(mat2x2 a, mat2x2 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      return true;
    }
  }
  return false;
}
bool is_equal(mat3x3 a, mat3x3 b, float epsilon)
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
bool is_equal(mat4x4 a, mat4x4 b, float epsilon)
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

bool is_orthogonal(mat3x3 mat)
{
  if (abs(dot(mat[0], mat[1])) > 1e-5) {
    return false;
  }
  if (abs(dot(mat[1], mat[2])) > 1e-5) {
    return false;
  }
  if (abs(dot(mat[2], mat[0])) > 1e-5) {
    return false;
  }
  return true;
}

bool is_orthonormal(mat3x3 mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  if (abs(length_squared(mat[0]) - 1.0) > 1e-5) {
    return false;
  }
  if (abs(length_squared(mat[1]) - 1.0) > 1e-5) {
    return false;
  }
  if (abs(length_squared(mat[2]) - 1.0) > 1e-5) {
    return false;
  }
  return true;
}

bool is_uniformly_scaled(mat3x3 mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  const float eps = 1e-7;
  float x = length_squared(mat[0]);
  float y = length_squared(mat[1]);
  float z = length_squared(mat[2]);
  return (abs(x - y) < eps) && abs(x - z) < eps;
}

bool is_orthogonal(mat4x4 mat)
{
  return is_orthogonal(mat3x3(mat));
}
bool is_orthonormal(mat4x4 mat)
{
  return is_orthonormal(mat3x3(mat));
}
bool is_uniformly_scaled(mat4x4 mat)
{
  return is_uniformly_scaled(mat3x3(mat));
}

/* Returns true if each individual columns are unit scaled. Mainly for assert usage. */
bool is_unit_scale(mat4x4 m)
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
bool is_unit_scale(mat3x3 m)
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
bool is_unit_scale(mat2x2 m)
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
