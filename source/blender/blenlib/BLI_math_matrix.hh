/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Matrix Operations
 * \{ */

/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
template<typename T, int Size>
[[nodiscard]] MatBase<T, Size, Size> invert(const MatBase<T, Size, Size> &mat, bool &r_success);

/**
 * Flip the matrix across its diagonal. Also flips dimensions for non square matrices.
 */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> transpose(const MatBase<T, NumRow, NumCol> &mat);

/**
 * Normalize each column of the matrix individually.
 */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> normalize(const MatBase<T, NumCol, NumRow> &a);

/**
 * Normalize each column of the matrix individually.
 * Return the length of each column vector.
 */
template<typename T, int NumCol, int NumRow, typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> normalize_and_get_size(
    const MatBase<T, NumCol, NumRow> &a, VectorT &r_size);

/**
 * Returns the determinant of the matrix.
 * It can be interpreted as the signed volume (or area) of the unit cube after transformation.
 */
template<typename T, int Size> [[nodiscard]] T determinant(const MatBase<T, Size, Size> &mat);

/**
 * Returns the adjoint of the matrix (also known as adjugate matrix).
 */
template<typename T, int Size>
[[nodiscard]] MatBase<T, Size, Size> adjoint(const MatBase<T, Size, Size> &mat);

/**
 * Equivalent to `mat * from_location(translation)` but with fewer operation.
 */
template<typename T, int NumCol, int NumRow, typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> translate(const MatBase<T, NumCol, NumRow> &mat,
                                                   const VectorT &translation);

/**
 * Equivalent to `mat * from_rotation(rotation)` but with fewer operation.
 * Optimized for AxisAngle rotation on basis vector (i.e: AxisAngle({1, 0, 0}, 0.2)).
 */
template<typename T, int NumCol, int NumRow, typename RotationT>
[[nodiscard]] MatBase<T, NumCol, NumRow> rotate(const MatBase<T, NumCol, NumRow> &mat,
                                                const RotationT &rotation);

/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
template<typename T, int NumCol, int NumRow, typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> scale(const MatBase<T, NumCol, NumRow> &mat,
                                               const VectorT &scale);

/**
 * Interpolate each component linearly.
 */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> interpolate_linear(const MatBase<T, NumCol, NumRow> &a,
                                                            const MatBase<T, NumCol, NumRow> &b,
                                                            T t);

/**
 * A polar-decomposition-based interpolation between matrix A and matrix B.
 *
 * \note This code is about five times slower than the 'naive' interpolation
 * (it typically remains below 2 usec on an average i74700,
 * while naive implementation remains below 0.4 usec).
 * However, it gives expected results even with non-uniformly scaled matrices,
 * see T46418 for an example.
 *
 * Based on "Matrix Animation and Polar Decomposition", by Ken Shoemake & Tom Duff
 *
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
template<typename T>
[[nodiscard]] MatBase<T, 3, 3> interpolate(const MatBase<T, 3, 3> &a,
                                           const MatBase<T, 3, 3> &b,
                                           T t);

/**
 * Complete transform matrix interpolation,
 * based on polar-decomposition-based interpolation from #interpolate<T, 3, 3>.
 *
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> interpolate(const MatBase<T, 4, 4> &a,
                                           const MatBase<T, 4, 4> &b,
                                           T t);

/**
 * Naive interpolation implementation, faster than polar decomposition
 *
 * \note This code is about five times faster than the polar decomposition.
 * However, it gives un-expected results even with non-uniformly scaled matrices,
 * see T46418 for an example.
 *
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
template<typename T>
[[nodiscard]] MatBase<T, 3, 3> interpolate_fast(const MatBase<T, 3, 3> &a,
                                                const MatBase<T, 3, 3> &b,
                                                T t);

/**
 * Naive transform matrix interpolation,
 * based on naive-decomposition-based interpolation from #interpolate_fast<T, 3, 3>.
 *
 * \note This code is about five times faster than the polar decomposition.
 * However, it gives un-expected results even with non-uniformly scaled matrices,
 * see T46418 for an example.
 *
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> interpolate_fast(const MatBase<T, 4, 4> &a,
                                                const MatBase<T, 4, 4> &b,
                                                T t);

/**
 * Compute Moore-Penrose pseudo inverse of matrix.
 * Singular values below epsilon are ignored for stability (truncated SVD).
 * Gives a good enough approximation of the regular inverse matrix if the given matrix is
 * non-invertible (ex: degenerate transform).
 * The returned pseudo inverse matrix `A+` of input matrix `A`
 * will *not* satisfy `A+ * A = Identity`
 * but will satisfy `A * A+ * A = A`.
 * For more detail, see https://en.wikipedia.org/wiki/Moore%E2%80%93Penrose_inverse.
 */
template<typename T, int Size>
[[nodiscard]] MatBase<T, Size, Size> pseudo_invert(const MatBase<T, Size, Size> &mat,
                                                   T epsilon = 1e-8);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init helpers.
 * \{ */

/**
 * Create a translation only matrix. Matrix dimensions should be at least 4 col x 3 row.
 */
template<typename MatT> [[nodiscard]] MatT from_location(const typename MatT::vec3_type &location);

/**
 * Create a matrix whose diagonal is defined by the given scale vector.
 * If vector dimension is lower than matrix diagonal, the missing terms are filled with ones.
 */
template<typename MatT, int ScaleDim>
[[nodiscard]] MatT from_scale(const VecBase<typename MatT::base_type, ScaleDim> &scale);

/**
 * Create a rotation only matrix.
 */
template<typename MatT, typename RotationT>
[[nodiscard]] MatT from_rotation(const RotationT &rotation);

/**
 * Create a transform matrix with rotation and scale applied in this order.
 */
template<typename MatT, typename RotationT, typename VectorT>
[[nodiscard]] MatT from_rot_scale(const RotationT &rotation, const VectorT &scale);

/**
 * Create a transform matrix with translation and rotation applied in this order.
 */
template<typename MatT, typename RotationT>
[[nodiscard]] MatT from_loc_rot(const typename MatT::vec3_type &location,
                                const RotationT &rotation);

/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
template<typename MatT, typename RotationT, int ScaleDim>
[[nodiscard]] MatT from_loc_rot_scale(const typename MatT::vec3_type &location,
                                      const RotationT &rotation,
                                      const VecBase<typename MatT::base_type, ScaleDim> &scale);

/**
 * Create a rotation matrix from 2 basis vectors.
 * The matrix determinant is given to be positive and it can be converted to other rotation types.
 * \note `forward` and `up` must be normalized.
 */
template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_orthonormal_axes(const VectorT forward, const VectorT up);

/**
 * Create a transform matrix with translation and rotation from 2 basis vectors and a translation.
 * \note `forward` and `up` must be normalized.
 */
template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_orthonormal_axes(const VectorT location,
                                         const VectorT forward,
                                         const VectorT up);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion function.
 * \{ */

/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
template<typename T, bool Normalized = false>
[[nodiscard]] inline detail::EulerXYZ<T> to_euler(const MatBase<T, 3, 3> &mat);
template<typename T, bool Normalized = false>
[[nodiscard]] inline detail::EulerXYZ<T> to_euler(const MatBase<T, 4, 4> &mat);

/**
 * Extract quaternion rotation from transform matrix.
 */
template<typename T, bool Normalized = false>
[[nodiscard]] inline detail::Quaternion<T> to_quaternion(const MatBase<T, 3, 3> &mat);
template<typename T, bool Normalized = false>
[[nodiscard]] inline detail::Quaternion<T> to_quaternion(const MatBase<T, 4, 4> &mat);

/**
 * Extract the absolute 3d scale from a transform matrix.
 * \tparam AllowNegativeScale: if true, will compute determinant to know if matrix is negative.
 * This is a costly operation so it is disabled by default.
 */
template<bool AllowNegativeScale = false, typename T, int NumCol, int NumRow>
[[nodiscard]] inline VecBase<T, 3> to_scale(const MatBase<T, NumCol, NumRow> &mat);

/**
 * Decompose a matrix into location, rotation, and scale components.
 * \tparam AllowNegativeScale: if true, will compute determinant to know if matrix is negative.
 * Rotation and scale values will be flipped if it is negative.
 * This is a costly operation so it is disabled by default.
 */
template<bool AllowNegativeScale = false, typename T, typename RotationT>
inline void to_rot_scale(const MatBase<T, 3, 3> &mat,
                         RotationT &r_rotation,
                         VecBase<T, 3> &r_scale);
template<bool AllowNegativeScale = false, typename T, typename RotationT>
inline void to_loc_rot_scale(const MatBase<T, 4, 4> &mat,
                             VecBase<T, 3> &r_location,
                             RotationT &r_rotation,
                             VecBase<T, 3> &r_scale);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform functions.
 * \{ */

/**
 * Transform a 3d point using a 3x3 matrix (rotation & scale).
 */
template<typename T>
[[nodiscard]] VecBase<T, 3> transform_point(const MatBase<T, 3, 3> &mat,
                                            const VecBase<T, 3> &point);

/**
 * Transform a 3d point using a 4x4 matrix (location & rotation & scale).
 */
template<typename T>
[[nodiscard]] VecBase<T, 3> transform_point(const MatBase<T, 4, 4> &mat,
                                            const VecBase<T, 3> &point);

/**
 * Transform a 3d direction vector using a 3x3 matrix (rotation & scale).
 */
template<typename T>
[[nodiscard]] VecBase<T, 3> transform_direction(const MatBase<T, 3, 3> &mat,
                                                const VecBase<T, 3> &direction);

/**
 * Transform a 3d direction vector using a 4x4 matrix (rotation & scale).
 */
template<typename T>
[[nodiscard]] VecBase<T, 3> transform_direction(const MatBase<T, 4, 4> &mat,
                                                const VecBase<T, 3> &direction);

/**
 * Project a point using a matrix (location & rotation & scale & perspective divide).
 */
template<typename MatT, typename VectorT>
[[nodiscard]] VectorT project_point(const MatT &mat, const VectorT &point);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Projection Matrices.
 * \{ */

namespace projection {

/**
 * \brief Create an orthographic projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * The resulting matrix can be used with either #project_point or #transform_point.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> orthographic(
    T left, T right, T bottom, T top, T near_clip, T far_clip);

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * `left`, `right`, `bottom`, `top` are frustum side distances at `z=near_clip`.
 * The resulting matrix can be used with #project_point.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> perspective(
    T left, T right, T bottom, T top, T near_clip, T far_clip);

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * Uses field of view angles instead of plane distances.
 * The resulting matrix can be used with #project_point.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> perspective_fov(
    T angle_left, T angle_right, T angle_bottom, T angle_top, T near_clip, T far_clip);

}  // namespace projection

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compare / Test
 * \{ */

/**
 * Returns true if matrix has inverted handedness.
 *
 * \note It doesn't use determinant(mat4x4) as only the 3x3 components are needed
 * when the matrix is used as a transformation to represent location/scale/rotation.
 */
template<typename T, int Size> [[nodiscard]] bool is_negative(const MatBase<T, Size, Size> &mat)
{
  return determinant(mat) < T(0);
}
template<typename T> [[nodiscard]] bool is_negative(const MatBase<T, 4, 4> &mat);

/**
 * Returns true if matrices are equal within the given epsilon.
 */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] inline bool is_equal(const MatBase<T, NumCol, NumRow> &a,
                                   const MatBase<T, NumCol, NumRow> &b,
                                   const T epsilon = T(0))
{
  for (int i = 0; i < NumCol; i++) {
    for (int j = 0; j < NumRow; j++) {
      if (math::abs(a[i][j] - b[i][j]) > epsilon) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Test if the X, Y and Z axes are perpendicular with each other.
 */
template<typename MatT> [[nodiscard]] inline bool is_orthogonal(const MatT &mat)
{
  if (math::abs(math::dot(mat.x_axis(), mat.y_axis())) > 1e-5f) {
    return false;
  }
  if (math::abs(math::dot(mat.y_axis(), mat.z_axis())) > 1e-5f) {
    return false;
  }
  if (math::abs(math::dot(mat.z_axis(), mat.x_axis())) > 1e-5f) {
    return false;
  }
  return true;
}

/**
 * Test if the X, Y and Z axes are perpendicular with each other and unit length.
 */
template<typename MatT> [[nodiscard]] inline bool is_orthonormal(const MatT &mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  if (math::abs(math::length_squared(mat.x_axis()) - 1) > 1e-5f) {
    return false;
  }
  if (math::abs(math::length_squared(mat.y_axis()) - 1) > 1e-5f) {
    return false;
  }
  if (math::abs(math::length_squared(mat.z_axis()) - 1) > 1e-5f) {
    return false;
  }
  return true;
}

/**
 * Test if the X, Y and Z axes are perpendicular with each other and the same length.
 */
template<typename MatT> [[nodiscard]] inline bool is_uniformly_scaled(const MatT &mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  using T = typename MatT::base_type;
  const T eps = 1e-7;
  const T x = math::length_squared(mat.x_axis());
  const T y = math::length_squared(mat.y_axis());
  const T z = math::length_squared(mat.z_axis());
  return (math::abs(x - y) < eps) && math::abs(x - z) < eps;
}

template<typename T, int NumCol, int NumRow>
inline bool is_zero(const MatBase<T, NumCol, NumRow> &mat)
{
  for (int i = 0; i < NumCol; i++) {
    if (!is_zero(mat[i])) {
      return false;
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implementation.
 * \{ */

/* Implementation details. */
namespace detail {

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const EulerXYZ<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const Quaternion<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const AxisAngle<T> &rotation);

}  // namespace detail

/* Returns true if each individual columns are unit scaled. Mainly for assert usage. */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] inline bool is_unit_scale(const MatBase<T, NumCol, NumRow> &m)
{
  for (int i = 0; i < NumCol; i++) {
    if (!is_unit_scale(m[i])) {
      return false;
    }
  }
  return true;
}

template<typename T, int Size>
[[nodiscard]] MatBase<T, Size, Size> invert(const MatBase<T, Size, Size> &mat)
{
  bool success;
  return invert(mat, success);
}

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> transpose(const MatBase<T, NumRow, NumCol> &mat)
{
  MatBase<T, NumCol, NumRow> result;
  unroll<NumCol>([&](auto i) { unroll<NumRow>([&](auto j) { result[i][j] = mat[j][i]; }); });
  return result;
}

template<typename T, int NumCol, int NumRow, typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> translate(const MatBase<T, NumCol, NumRow> &mat,
                                                   const VectorT &translation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  BLI_STATIC_ASSERT(VectorT::type_length <= MatT::col_len - 1,
                    "Translation should be at least 1 column less than the matrix.");
  constexpr int location_col = MatT::col_len - 1;
  /* Avoid multiplying the last row if it exists.
   * Allows using non square matrices like float3x2 and saves computation. */
  using IntermediateVecT =
      VecBase<typename MatT::base_type,
              (MatT::row_len > MatT::col_len - 1) ? (MatT::col_len - 1) : MatT::row_len>;

  MatT result = mat;
  unroll<VectorT::type_length>([&](auto c) {
    *reinterpret_cast<IntermediateVecT *>(
        &result[location_col]) += translation[c] *
                                  *reinterpret_cast<const IntermediateVecT *>(&mat[c]);
  });
  return result;
}

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> rotate(const MatBase<T, NumCol, NumRow> &mat,
                                                const detail::AxisAngle<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using Vec3T = typename MatT::vec3_type;
  const T &angle_sin = rotation.angle_sin();
  const T &angle_cos = rotation.angle_cos();
  const Vec3T &axis_vec = rotation.axis();

  MatT result = mat;
  /* axis_vec is given to be normalized. */
  if (axis_vec.x == T(1)) {
    unroll<MatT::row_len>([&](auto c) {
      result[2][c] = -angle_sin * mat[1][c] + angle_cos * mat[2][c];
      result[1][c] = angle_cos * mat[1][c] + angle_sin * mat[2][c];
    });
  }
  else if (axis_vec.y == T(1)) {
    unroll<MatT::row_len>([&](auto c) {
      result[0][c] = angle_cos * mat[0][c] - angle_sin * mat[2][c];
      result[2][c] = angle_sin * mat[0][c] + angle_cos * mat[2][c];
    });
  }
  else if (axis_vec.z == T(1)) {
    unroll<MatT::row_len>([&](auto c) {
      result[0][c] = angle_cos * mat[0][c] + angle_sin * mat[1][c];
      result[1][c] = -angle_sin * mat[0][c] + angle_cos * mat[1][c];
    });
  }
  else {
    /* Un-optimized case. Arbitrary */
    result *= from_rotation<MatT>(rotation);
  }
  return result;
}

template<typename T, int NumCol, int NumRow, typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> scale(const MatBase<T, NumCol, NumRow> &mat,
                                               const VectorT &scale)
{
  BLI_STATIC_ASSERT(VectorT::type_length <= NumCol,
                    "Scale should be less or equal to the matrix in column count.");
  MatBase<T, NumCol, NumRow> result = mat;
  unroll<VectorT::type_length>([&](auto c) { result[c] *= scale[c]; });
  return result;
}

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> interpolate_linear(const MatBase<T, NumCol, NumRow> &a,
                                                            const MatBase<T, NumCol, NumRow> &b,
                                                            T t)
{
  MatBase<T, NumCol, NumRow> result;
  unroll<NumCol>([&](auto c) { result[c] = interpolate(a[c], b[c], t); });
  return result;
}

template<typename T,
         int NumCol,
         int NumRow,
         int SrcNumCol,
         int SrcNumRow,
         int SrcStartCol,
         int SrcStartRow,
         int SrcAlignment>
[[nodiscard]] MatBase<T, NumCol, NumRow> normalize(
    const MatView<T, NumCol, NumRow, SrcNumCol, SrcNumRow, SrcStartCol, SrcStartRow, SrcAlignment>
        &a)
{
  MatBase<T, NumCol, NumRow> result;
  unroll<NumCol>([&](auto i) { result[i] = math::normalize(a[i]); });
  return result;
}

template<typename T,
         int NumCol,
         int NumRow,
         int SrcNumCol,
         int SrcNumRow,
         int SrcStartCol,
         int SrcStartRow,
         int SrcAlignment,
         typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> normalize_and_get_size(
    const MatView<T, NumCol, NumRow, SrcNumCol, SrcNumRow, SrcStartCol, SrcStartRow, SrcAlignment>
        &a,
    VectorT &r_size)
{
  BLI_STATIC_ASSERT(VectorT::type_length == NumCol,
                    "r_size dimension should be equal to matrix column count.");
  MatBase<T, NumCol, NumRow> result;
  unroll<NumCol>([&](auto i) { result[i] = math::normalize_and_get_length(a[i], r_size[i]); });
  return result;
}

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> normalize(const MatBase<T, NumCol, NumRow> &a)
{
  MatBase<T, NumCol, NumRow> result;
  unroll<NumCol>([&](auto i) { result[i] = math::normalize(a[i]); });
  return result;
}

template<typename T, int NumCol, int NumRow, typename VectorT>
[[nodiscard]] MatBase<T, NumCol, NumRow> normalize_and_get_size(
    const MatBase<T, NumCol, NumRow> &a, VectorT &r_size)
{
  BLI_STATIC_ASSERT(VectorT::type_length == NumCol,
                    "r_size dimension should be equal to matrix column count.");
  MatBase<T, NumCol, NumRow> result;
  unroll<NumCol>([&](auto i) { result[i] = math::normalize_and_get_length(a[i], r_size[i]); });
  return result;
}

namespace detail {

template<typename T>
void normalized_to_eul2(const MatBase<T, 3, 3> &mat,
                        detail::EulerXYZ<T> &eul1,
                        detail::EulerXYZ<T> &eul2)
{
  BLI_assert(math::is_unit_scale(mat));

  const T cy = math::hypot(mat[0][0], mat[0][1]);
  if (cy > T(16) * FLT_EPSILON) {
    eul1.x = math::atan2(mat[1][2], mat[2][2]);
    eul1.y = math::atan2(-mat[0][2], cy);
    eul1.z = math::atan2(mat[0][1], mat[0][0]);

    eul2.x = math::atan2(-mat[1][2], -mat[2][2]);
    eul2.y = math::atan2(-mat[0][2], -cy);
    eul2.z = math::atan2(-mat[0][1], -mat[0][0]);
  }
  else {
    eul1.x = math::atan2(-mat[2][1], mat[1][1]);
    eul1.y = math::atan2(-mat[0][2], cy);
    eul1.z = 0.0f;

    eul2 = eul1;
  }
}

/* Using explicit template instantiations in order to reduce compilation time. */
extern template void normalized_to_eul2(const float3x3 &mat,
                                        detail::EulerXYZ<float> &eul1,
                                        detail::EulerXYZ<float> &eul2);
extern template void normalized_to_eul2(const double3x3 &mat,
                                        detail::EulerXYZ<double> &eul1,
                                        detail::EulerXYZ<double> &eul2);

template<typename T> detail::Quaternion<T> normalized_to_quat_fast(const MatBase<T, 3, 3> &mat)
{
  BLI_assert(math::is_unit_scale(mat));
  /* Caller must ensure matrices aren't negative for valid results, see: T24291, T94231. */
  BLI_assert(!math::is_negative(mat));

  detail::Quaternion<T> q;

  /* Method outlined by Mike Day, ref: https://math.stackexchange.com/a/3183435/220949
   * with an additional `sqrtf(..)` for higher precision result.
   * Removing the `sqrt` causes tests to fail unless the precision is set to 1e-6 or larger. */

  if (mat[2][2] < 0.0f) {
    if (mat[0][0] > mat[1][1]) {
      const T trace = 1.0f + mat[0][0] - mat[1][1] - mat[2][2];
      T s = 2.0f * math::sqrt(trace);
      if (mat[1][2] < mat[2][1]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.y = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[1][2] - mat[2][1]) * s;
      q.z = (mat[0][1] + mat[1][0]) * s;
      q.w = (mat[2][0] + mat[0][2]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.x == 0.0f && q.z == 0.0f && q.w == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.y = 1.0f;
      }
    }
    else {
      const T trace = 1.0f - mat[0][0] + mat[1][1] - mat[2][2];
      T s = 2.0f * math::sqrt(trace);
      if (mat[2][0] < mat[0][2]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.z = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[2][0] - mat[0][2]) * s;
      q.y = (mat[0][1] + mat[1][0]) * s;
      q.w = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.x == 0.0f && q.y == 0.0f && q.w == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.z = 1.0f;
      }
    }
  }
  else {
    if (mat[0][0] < -mat[1][1]) {
      const T trace = 1.0f - mat[0][0] - mat[1][1] + mat[2][2];
      T s = 2.0f * math::sqrt(trace);
      if (mat[0][1] < mat[1][0]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.w = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[0][1] - mat[1][0]) * s;
      q.y = (mat[2][0] + mat[0][2]) * s;
      q.z = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.x == 0.0f && q.y == 0.0f && q.z == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.w = 1.0f;
      }
    }
    else {
      /* NOTE(@campbellbarton): A zero matrix will fall through to this block,
       * needed so a zero scaled matrices to return a quaternion without rotation, see: T101848.
       */
      const T trace = 1.0f + mat[0][0] + mat[1][1] + mat[2][2];
      T s = 2.0f * math::sqrt(trace);
      q.x = 0.25f * s;
      s = 1.0f / s;
      q.y = (mat[1][2] - mat[2][1]) * s;
      q.z = (mat[2][0] - mat[0][2]) * s;
      q.w = (mat[0][1] - mat[1][0]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.y == 0.0f && q.z == 0.0f && q.w == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.x = 1.0f;
      }
    }
  }

  BLI_assert(!(q.x < 0.0f));
  BLI_assert(math::is_unit_scale(VecBase<T, 4>(q)));
  return q;
}

template<typename T>
detail::Quaternion<T> normalized_to_quat_with_checks(const MatBase<T, 3, 3> &mat)
{
  const T det = math::determinant(mat);
  if (UNLIKELY(!isfinite(det))) {
    return detail::Quaternion<T>::identity();
  }
  else if (UNLIKELY(det < T(0))) {
    return normalized_to_quat_fast(-mat);
  }
  return normalized_to_quat_fast(mat);
}

/* Using explicit template instantiations in order to reduce compilation time. */
extern template Quaternion<float> normalized_to_quat_with_checks(const float3x3 &mat);
extern template Quaternion<double> normalized_to_quat_with_checks(const double3x3 &mat);

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const EulerXYZ<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using DoublePrecision = typename TypeTraits<T>::DoublePrecision;
  DoublePrecision ci = math::cos(rotation.x);
  DoublePrecision cj = math::cos(rotation.y);
  DoublePrecision ch = math::cos(rotation.z);
  DoublePrecision si = math::sin(rotation.x);
  DoublePrecision sj = math::sin(rotation.y);
  DoublePrecision sh = math::sin(rotation.z);
  DoublePrecision cc = ci * ch;
  DoublePrecision cs = ci * sh;
  DoublePrecision sc = si * ch;
  DoublePrecision ss = si * sh;

  MatT mat;
  mat[0][0] = T(cj * ch);
  mat[1][0] = T(sj * sc - cs);
  mat[2][0] = T(sj * cc + ss);

  mat[0][1] = T(cj * sh);
  mat[1][1] = T(sj * ss + cc);
  mat[2][1] = T(sj * cs - sc);

  mat[0][2] = T(-sj);
  mat[1][2] = T(cj * si);
  mat[2][2] = T(cj * ci);
  return mat;
}

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const Quaternion<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using DoublePrecision = typename TypeTraits<T>::DoublePrecision;
  DoublePrecision q0 = M_SQRT2 * DoublePrecision(rotation.x);
  DoublePrecision q1 = M_SQRT2 * DoublePrecision(rotation.y);
  DoublePrecision q2 = M_SQRT2 * DoublePrecision(rotation.z);
  DoublePrecision q3 = M_SQRT2 * DoublePrecision(rotation.w);

  DoublePrecision qda = q0 * q1;
  DoublePrecision qdb = q0 * q2;
  DoublePrecision qdc = q0 * q3;
  DoublePrecision qaa = q1 * q1;
  DoublePrecision qab = q1 * q2;
  DoublePrecision qac = q1 * q3;
  DoublePrecision qbb = q2 * q2;
  DoublePrecision qbc = q2 * q3;
  DoublePrecision qcc = q3 * q3;

  MatT mat;
  mat[0][0] = T(1.0 - qbb - qcc);
  mat[0][1] = T(qdc + qab);
  mat[0][2] = T(-qdb + qac);

  mat[1][0] = T(-qdc + qab);
  mat[1][1] = T(1.0 - qaa - qcc);
  mat[1][2] = T(qda + qbc);

  mat[2][0] = T(qdb + qac);
  mat[2][1] = T(-qda + qbc);
  mat[2][2] = T(1.0 - qaa - qbb);
  return mat;
}

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const AxisAngle<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using Vec3T = typename MatT::vec3_type;
  const T angle_sin = rotation.angle_sin();
  const T angle_cos = rotation.angle_cos();
  const Vec3T &axis = rotation.axis();

  BLI_assert(is_unit_scale(axis));

  T ico = (T(1) - angle_cos);
  Vec3T nsi = axis * angle_sin;

  Vec3T n012 = (axis * axis) * ico;
  T n_01 = (axis[0] * axis[1]) * ico;
  T n_02 = (axis[0] * axis[2]) * ico;
  T n_12 = (axis[1] * axis[2]) * ico;

  MatT mat = from_scale<MatT>(n012 + angle_cos);
  mat[0][1] = n_01 + nsi[2];
  mat[0][2] = n_02 - nsi[1];
  mat[1][0] = n_01 - nsi[2];
  mat[1][2] = n_12 + nsi[0];
  mat[2][0] = n_02 + nsi[1];
  mat[2][1] = n_12 - nsi[0];
  return mat;
}

/* Using explicit template instantiations in order to reduce compilation time. */
extern template MatBase<float, 3, 3> from_rotation(const EulerXYZ<float> &rotation);
extern template MatBase<float, 4, 4> from_rotation(const EulerXYZ<float> &rotation);
extern template MatBase<float, 3, 3> from_rotation(const Quaternion<float> &rotation);
extern template MatBase<float, 4, 4> from_rotation(const Quaternion<float> &rotation);
extern template MatBase<float, 3, 3> from_rotation(const AxisAngle<float> &rotation);
extern template MatBase<float, 4, 4> from_rotation(const AxisAngle<float> &rotation);

}  // namespace detail

template<typename T, bool Normalized>
[[nodiscard]] inline detail::EulerXYZ<T> to_euler(const MatBase<T, 3, 3> &mat)
{
  detail::EulerXYZ<T> eul1, eul2;
  if constexpr (Normalized) {
    detail::normalized_to_eul2(mat, eul1, eul2);
  }
  else {
    detail::normalized_to_eul2(normalize(mat), eul1, eul2);
  }
  /* Return best, which is just the one with lowest values it in. */
  return (length_manhattan(VecBase<T, 3>(eul1)) > length_manhattan(VecBase<T, 3>(eul2))) ? eul2 :
                                                                                           eul1;
}

template<typename T, bool Normalized>
[[nodiscard]] inline detail::EulerXYZ<T> to_euler(const MatBase<T, 4, 4> &mat)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_euler<T, Normalized>(MatBase<T, 3, 3>(mat));
}

template<typename T, bool Normalized>
[[nodiscard]] inline detail::Quaternion<T> to_quaternion(const MatBase<T, 3, 3> &mat)
{
  using namespace math;
  if constexpr (Normalized) {
    return detail::normalized_to_quat_with_checks(mat);
  }
  else {
    return detail::normalized_to_quat_with_checks(normalize(mat));
  }
}

template<typename T, bool Normalized>
[[nodiscard]] inline detail::Quaternion<T> to_quaternion(const MatBase<T, 4, 4> &mat)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_quaternion<T, Normalized>(MatBase<T, 3, 3>(mat));
}

template<bool AllowNegativeScale, typename T, int NumCol, int NumRow>
[[nodiscard]] inline VecBase<T, 3> to_scale(const MatBase<T, NumCol, NumRow> &mat)
{
  VecBase<T, 3> result = {length(mat.x_axis()), length(mat.y_axis()), length(mat.z_axis())};
  if constexpr (AllowNegativeScale) {
    if (UNLIKELY(is_negative(mat))) {
      result = -result;
    }
  }
  return result;
}

/* Implementation details. Use `to_euler` and `to_quaternion` instead. */
namespace detail {

template<typename T, bool Normalized>
inline void to_rotation(const MatBase<T, 3, 3> &mat, detail::Quaternion<T> &r_rotation)
{
  r_rotation = to_quaternion<T, Normalized>(mat);
}

template<typename T, bool Normalized>
inline void to_rotation(const MatBase<T, 3, 3> &mat, detail::EulerXYZ<T> &r_rotation)
{
  r_rotation = to_euler<T, Normalized>(mat);
}

}  // namespace detail

template<bool AllowNegativeScale, typename T, typename RotationT>
inline void to_rot_scale(const MatBase<T, 3, 3> &mat,
                         RotationT &r_rotation,
                         VecBase<T, 3> &r_scale)
{
  MatBase<T, 3, 3> normalized_mat = normalize_and_get_size(mat, r_scale);
  if constexpr (AllowNegativeScale) {
    if (UNLIKELY(is_negative(normalized_mat))) {
      normalized_mat = -normalized_mat;
      r_scale = -r_scale;
    }
  }
  detail::to_rotation<T, true>(normalized_mat, r_rotation);
}

template<bool AllowNegativeScale, typename T, typename RotationT>
inline void to_loc_rot_scale(const MatBase<T, 4, 4> &mat,
                             VecBase<T, 3> &r_location,
                             RotationT &r_rotation,
                             VecBase<T, 3> &r_scale)
{
  r_location = mat.location();
  to_rot_scale<AllowNegativeScale>(MatBase<T, 3, 3>(mat), r_rotation, r_scale);
}

template<typename MatT> [[nodiscard]] MatT from_location(const typename MatT::vec3_type &location)
{
  MatT mat = MatT::identity();
  mat.location() = location;
  return mat;
}

template<typename MatT, int ScaleDim>
[[nodiscard]] MatT from_scale(const VecBase<typename MatT::base_type, ScaleDim> &scale)
{
  BLI_STATIC_ASSERT(ScaleDim <= MatT::min_dim,
                    "Scale dimension should fit the matrix diagonal length.");
  MatT result{};
  unroll<MatT::min_dim>(
      [&](auto i) { result[i][i] = (i < ScaleDim) ? scale[i] : typename MatT::base_type(1); });
  return result;
}

template<typename MatT, typename RotationT>
[[nodiscard]] MatT from_rotation(const RotationT &rotation)
{
  return detail::from_rotation<typename MatT::base_type, MatT::col_len, MatT::row_len>(rotation);
}

template<typename MatT, typename RotationT, typename VectorT>
[[nodiscard]] MatT from_rot_scale(const RotationT &rotation, const VectorT &scale)
{
  return from_rotation<MatT>(rotation) * from_scale<MatT>(scale);
}

template<typename MatT, typename RotationT, int ScaleDim>
[[nodiscard]] MatT from_loc_rot_scale(const typename MatT::vec3_type &location,
                                      const RotationT &rotation,
                                      const VecBase<typename MatT::base_type, ScaleDim> &scale)
{
  using Mat3x3 = MatBase<typename MatT::base_type, 3, 3>;
  MatT mat = MatT(from_rot_scale<Mat3x3>(rotation, scale));
  mat.location() = location;
  return mat;
}

template<typename MatT, typename RotationT>
[[nodiscard]] MatT from_loc_rot(const typename MatT::vec3_type &location,
                                const RotationT &rotation)
{
  using Mat3x3 = MatBase<typename MatT::base_type, 3, 3>;
  MatT mat = MatT(from_rotation<Mat3x3>(rotation));
  mat.location() = location;
  return mat;
}

template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_orthonormal_axes(const VectorT forward, const VectorT up)
{
  BLI_assert(is_unit_scale(forward));
  BLI_assert(is_unit_scale(up));

  MatT matrix;
  matrix.x_axis() = forward;
  /* Beware of handedness! Blender uses right-handedness.
   * Resulting matrix should have determinant of 1. */
  matrix.y_axis() = math::cross(up, forward);
  matrix.z_axis() = up;
  return matrix;
}

template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_orthonormal_axes(const VectorT location,
                                         const VectorT forward,
                                         const VectorT up)
{
  using Mat3x3 = MatBase<typename MatT::base_type, 3, 3>;
  MatT matrix = MatT(from_orthonormal_axes<Mat3x3>(forward, up));
  matrix.location() = location;
  return matrix;
}

template<typename T>
VecBase<T, 3> transform_point(const MatBase<T, 3, 3> &mat, const VecBase<T, 3> &point)
{
  return mat * point;
}

template<typename T>
VecBase<T, 3> transform_point(const MatBase<T, 4, 4> &mat, const VecBase<T, 3> &point)
{
  return mat.template view<3, 3>() * point + mat.location();
}

template<typename T>
VecBase<T, 3> transform_direction(const MatBase<T, 3, 3> &mat, const VecBase<T, 3> &direction)
{
  return mat * direction;
}

template<typename T>
VecBase<T, 3> transform_direction(const MatBase<T, 4, 4> &mat, const VecBase<T, 3> &direction)
{
  return mat.template view<3, 3>() * direction;
}

template<typename T, int N, int NumRow>
VecBase<T, N> project_point(const MatBase<T, N + 1, NumRow> &mat, const VecBase<T, N> &point)
{
  VecBase<T, N + 1> tmp(point, T(1));
  tmp = mat * tmp;
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return VecBase<T, N>(tmp) / math::abs(tmp[N]);
}

extern template float3 transform_point(const float3x3 &mat, const float3 &point);
extern template float3 transform_point(const float4x4 &mat, const float3 &point);
extern template float3 transform_direction(const float3x3 &mat, const float3 &direction);
extern template float3 transform_direction(const float4x4 &mat, const float3 &direction);
extern template float3 project_point(const float4x4 &mat, const float3 &point);
extern template float2 project_point(const float3x3 &mat, const float2 &point);

namespace projection {

template<typename T>
MatBase<T, 4, 4> orthographic(T left, T right, T bottom, T top, T near_clip, T far_clip)
{
  const T x_delta = right - left;
  const T y_delta = top - bottom;
  const T z_delta = far_clip - near_clip;

  MatBase<T, 4, 4> mat = MatBase<T, 4, 4>::identity();
  if (x_delta != 0 && y_delta != 0 && z_delta != 0) {
    mat[0][0] = T(2.0) / x_delta;
    mat[3][0] = -(right + left) / x_delta;
    mat[1][1] = T(2.0) / y_delta;
    mat[3][1] = -(top + bottom) / y_delta;
    mat[2][2] = -T(2.0) / z_delta; /* NOTE: negate Z. */
    mat[3][2] = -(far_clip + near_clip) / z_delta;
  }
  return mat;
}

template<typename T>
MatBase<T, 4, 4> perspective(T left, T right, T bottom, T top, T near_clip, T far_clip)
{
  const T x_delta = right - left;
  const T y_delta = top - bottom;
  const T z_delta = far_clip - near_clip;

  MatBase<T, 4, 4> mat = MatBase<T, 4, 4>::identity();
  if (x_delta != 0 && y_delta != 0 && z_delta != 0) {
    mat[0][0] = near_clip * T(2.0) / x_delta;
    mat[1][1] = near_clip * T(2.0) / y_delta;
    mat[2][0] = (right + left) / x_delta; /* NOTE: negate Z. */
    mat[2][1] = (top + bottom) / y_delta;
    mat[2][2] = -(far_clip + near_clip) / z_delta;
    mat[2][3] = -1.0f;
    mat[3][2] = (-2.0f * near_clip * far_clip) / z_delta;
  }
  return mat;
}

template<typename T>
[[nodiscard]] MatBase<T, 4, 4> perspective_fov(
    T angle_left, T angle_right, T angle_bottom, T angle_top, T near_clip, T far_clip)
{
  MatBase<T, 4, 4> mat = perspective(math::tan(angle_left),
                                     math::tan(angle_right),
                                     math::tan(angle_bottom),
                                     math::tan(angle_top),
                                     near_clip,
                                     far_clip);
  mat[0][0] /= near_clip;
  mat[1][1] /= near_clip;
  return mat;
}

extern template float4x4 orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip);
extern template float4x4 perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip);

}  // namespace projection

/** \} */

}  // namespace blender::math
