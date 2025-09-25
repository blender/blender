/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_unroll.hh"

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
 * see #46418 for an example.
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
 * see #46418 for an example.
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
 * see #46418 for an example.
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
template<typename MatT> [[nodiscard]] MatT from_location(const typename MatT::loc_type &location);

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
[[nodiscard]] MatT from_loc_rot(const typename MatT::loc_type &location,
                                const RotationT &rotation);

/**
 * Create a transform matrix with translation and scale applied in this order.
 */
template<typename MatT, int ScaleDim>
[[nodiscard]] MatT from_loc_scale(const typename MatT::loc_type &location,
                                  const VecBase<typename MatT::base_type, ScaleDim> &scale);

/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
template<typename MatT, typename RotationT, int ScaleDim>
[[nodiscard]] MatT from_loc_rot_scale(const typename MatT::loc_type &location,
                                      const RotationT &rotation,
                                      const VecBase<typename MatT::base_type, ScaleDim> &scale);

/**
 * Create a rotation matrix with the angle that the given direction makes with the x axis. Assumes
 * the direction vector is normalized.
 */
template<typename T> [[nodiscard]] MatBase<T, 2, 2> from_direction(const VecBase<T, 2> &direction);

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

/**
 * Create a rotation matrix from only one \a up axis.
 * The other axes are chosen to always be orthogonal. The resulting matrix is a basis matrix.
 * \note `up` must be normalized.
 * \note This can be used to create a tangent basis from a normal vector.
 * \note The output of this function is not given to be same across blender version. Prefer using
 * `from_orthonormal_axes` for more stable output.
 */
template<typename MatT, typename VectorT> [[nodiscard]] MatT from_up_axis(const VectorT up);

/**
 * This returns a version of \a mat with orthonormal basis axes.
 * This leaves the given \a axis untouched.
 *
 * In other words this removes the shear of the matrix. However this doesn't properly account for
 * volume preservation, and so, the axes keep their respective length.
 *
 * \note Prefer using `from_up_axis` to create a orthogonal basis around a vector.
 */
template<typename MatT> [[nodiscard]] MatT orthogonalize(const MatT &mat, const Axis axis);

/**
 * Construct a transformation that is pivoted around the given origin point. So for instance,
 * from_origin_transform<MatT>(from_rotation(numbers::pi * 0.5), float2(0.0f, 2.0f))
 * will construct a transformation representing a 90 degree rotation around the point (0, 2).
 */
template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_origin_transform(const MatT &transform, const VectorT origin);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion function.
 * \{ */

/**
 * Extract euler rotation from transform matrix.
 * \return the rotation with the smallest values from the potential candidates.
 */
template<typename T> [[nodiscard]] inline AngleRadianBase<T> to_angle(const MatBase<T, 2, 2> &mat);
template<typename T> [[nodiscard]] inline EulerXYZBase<T> to_euler(const MatBase<T, 3, 3> &mat);
template<typename T> [[nodiscard]] inline EulerXYZBase<T> to_euler(const MatBase<T, 4, 4> &mat);
template<typename T>
[[nodiscard]] inline Euler3Base<T> to_euler(const MatBase<T, 3, 3> &mat, EulerOrder order);
template<typename T>
[[nodiscard]] inline Euler3Base<T> to_euler(const MatBase<T, 4, 4> &mat, EulerOrder order);

/**
 * Extract euler rotation from transform matrix.
 * The returned euler triple is given to be the closest from the \a reference.
 * It avoids axis flipping for animated f-curves for eg.
 * \return the rotation with the smallest values from the potential candidates.
 * \note this correspond to the C API "to_compatible" functions.
 */
template<typename T>
[[nodiscard]] inline EulerXYZBase<T> to_nearest_euler(const MatBase<T, 3, 3> &mat,
                                                      const EulerXYZBase<T> &reference);
template<typename T>
[[nodiscard]] inline EulerXYZBase<T> to_nearest_euler(const MatBase<T, 4, 4> &mat,
                                                      const EulerXYZBase<T> &reference);
template<typename T>
[[nodiscard]] inline Euler3Base<T> to_nearest_euler(const MatBase<T, 3, 3> &mat,
                                                    const Euler3Base<T> &reference);
template<typename T>
[[nodiscard]] inline Euler3Base<T> to_nearest_euler(const MatBase<T, 4, 4> &mat,
                                                    const Euler3Base<T> &reference);

/**
 * Extract quaternion rotation from transform matrix.
 */
template<typename T>
[[nodiscard]] inline QuaternionBase<T> to_quaternion(const MatBase<T, 3, 3> &mat);
template<typename T>
[[nodiscard]] inline QuaternionBase<T> to_quaternion(const MatBase<T, 4, 4> &mat);

/**
 * Extract quaternion rotation from transform matrix.
 * Legacy version of #to_quaternion which has slightly different behavior.
 * Keep for particle-system & boids since replacing this will make subtle changes
 * that impact hair in existing files. See: D15772.
 */
[[nodiscard]] Quaternion to_quaternion_legacy(const float3x3 &mat);

/**
 * Extract the absolute 3d scale from a transform matrix.
 * \tparam AllowNegativeScale: if true, will compute determinant to know if matrix is negative.
 * This is a costly operation so it is disabled by default.
 */
template<bool AllowNegativeScale = false, typename T, int NumCol, int NumRow>
[[nodiscard]] inline VecBase<T, 3> to_scale(const MatBase<T, NumCol, NumRow> &mat);
template<bool AllowNegativeScale = false, typename T>
[[nodiscard]] inline VecBase<T, 2> to_scale(const MatBase<T, 2, 2> &mat);

/**
 * Decompose a matrix into location, rotation, and scale components.
 * \tparam AllowNegativeScale: if true, will compute determinant to know if matrix is negative.
 * Rotation and scale values will be flipped if it is negative.
 * This is a costly operation so it is disabled by default.
 */
template<bool AllowNegativeScale = false, typename T>
inline void to_rot_scale(const MatBase<T, 2, 2> &mat,
                         AngleRadianBase<T> &r_rotation,
                         VecBase<T, 2> &r_scale);
template<bool AllowNegativeScale = false, typename T>
inline void to_loc_rot_scale(const MatBase<T, 3, 3> &mat,
                             VecBase<T, 2> &r_location,
                             AngleRadianBase<T> &r_rotation,
                             VecBase<T, 2> &r_scale);
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
 * Transform a 2d point using a 2x2 matrix (rotation & scale).
 */
template<typename T>
[[nodiscard]] VecBase<T, 2> transform_point(const MatBase<T, 2, 2> &mat,
                                            const VecBase<T, 2> &point);

/**
 * Transform a 2d point using a 3x3 matrix (location & rotation & scale).
 */
template<typename T>
[[nodiscard]] VecBase<T, 2> transform_point(const MatBase<T, 3, 3> &mat,
                                            const VecBase<T, 2> &point);

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
 * \brief Create an orthographic projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes except Z.
 * The Z axis is almost collapsed to 0 which eliminates the depth component.
 * So it should not be used with depth testing.
 * The resulting matrix can be used with either #project_point or #transform_point.
 */
template<typename T> MatBase<T, 4, 4> orthographic_infinite(T left, T right, T bottom, T top);

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

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes except for the Z where [near_clip..inf] is
 * mapped to [-1..1].
 * `left`, `right`, `bottom`, `top` are frustum side distances at `z=near_clip`.
 * The resulting matrix can be used with #project_point.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> perspective_infinite(T left, T right, T bottom, T top, T near_clip);

/**
 * \brief Translate a projection matrix after creation in the screen plane.
 *  Usually used for anti-aliasing jittering.
 * `offset` is the translation vector in projected space.
 */
template<typename T>
[[nodiscard]] MatBase<T, 4, 4> translate(const MatBase<T, 4, 4> &mat, const VecBase<T, 2> &offset);

}  // namespace projection

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compare / Test
 * \{ */

/**
 * Returns true if matrix has inverted handedness.
 *
 * \note It doesn't use determinant(mat4x4) as only the 3x3 components are needed assuming
 * the matrix is used as a transformation to represent 3D location/scale/rotation.
 */
template<typename T> [[nodiscard]] bool is_negative(const MatBase<T, 3, 3> &mat);
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
 * Returns true if the matrix is exactly the identity matrix.
 */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] inline bool is_identity(const MatBase<T, NumCol, NumRow> &mat)
{
  for (int i = 0; i < NumCol; i++) {
    for (int j = 0; j < NumRow; j++) {
      if (mat[i][j] != (i != j ? 0.0f : 1.0f)) {
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
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const AngleRadianBase<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const EulerXYZBase<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const Euler3Base<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const QuaternionBase<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const DualQuaternionBase<T> &rotation);

template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const CartesianBasis &rotation);

template<typename T, int NumCol, int NumRow, typename AngleT>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const AxisAngleBase<T, AngleT> &rotation);

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
  /* Explicit template parameter to please MSVC. */
  return invert<T, Size>(mat, success);
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

template<typename T, int NumCol, int NumRow, typename AngleT>
[[nodiscard]] MatBase<T, NumCol, NumRow> rotate(const MatBase<T, NumCol, NumRow> &mat,
                                                const AxisAngleBase<T, AngleT> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using Vec3T = typename MatT::vec3_type;
  const T angle_sin = sin(rotation.angle());
  const T angle_cos = cos(rotation.angle());
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

template<typename T> AngleRadianBase<T> normalized_to_angle(const MatBase<T, 2, 2> &mat)
{
  BLI_assert(math::is_unit_scale(mat));
  return AngleRadianBase(mat[0][0], mat[0][1]);
}

template<typename T>
void normalized_to_eul2(const MatBase<T, 3, 3> &mat, EulerXYZBase<T> &eul1, EulerXYZBase<T> &eul2)
{
  BLI_assert(math::is_unit_scale(mat));

  const T cy = math::hypot(mat[0][0], mat[0][1]);
  if (cy > T(16) * std::numeric_limits<T>::epsilon()) {
    eul1.x() = math::atan2(mat[1][2], mat[2][2]);
    eul1.y() = math::atan2(-mat[0][2], cy);
    eul1.z() = math::atan2(mat[0][1], mat[0][0]);

    eul2.x() = math::atan2(-mat[1][2], -mat[2][2]);
    eul2.y() = math::atan2(-mat[0][2], -cy);
    eul2.z() = math::atan2(-mat[0][1], -mat[0][0]);
  }
  else {
    eul1.x() = math::atan2(-mat[2][1], mat[1][1]);
    eul1.y() = math::atan2(-mat[0][2], cy);
    eul1.z() = 0.0f;

    eul2 = eul1;
  }
}
template<typename T>
void normalized_to_eul2(const MatBase<T, 3, 3> &mat, Euler3Base<T> &eul1, Euler3Base<T> &eul2)
{
  BLI_assert(math::is_unit_scale(mat));
  const int i_index = eul1.i_index();
  const int j_index = eul1.j_index();
  const int k_index = eul1.k_index();

  const T cy = math::hypot(mat[i_index][i_index], mat[i_index][j_index]);
  if (cy > T(16) * std::numeric_limits<T>::epsilon()) {
    eul1.i() = math::atan2(mat[j_index][k_index], mat[k_index][k_index]);
    eul1.j() = math::atan2(-mat[i_index][k_index], cy);
    eul1.k() = math::atan2(mat[i_index][j_index], mat[i_index][i_index]);

    eul2.i() = math::atan2(-mat[j_index][k_index], -mat[k_index][k_index]);
    eul2.j() = math::atan2(-mat[i_index][k_index], -cy);
    eul2.k() = math::atan2(-mat[i_index][j_index], -mat[i_index][i_index]);
  }
  else {
    eul1.i() = math::atan2(-mat[k_index][j_index], mat[j_index][j_index]);
    eul1.j() = math::atan2(-mat[i_index][k_index], cy);
    eul1.k() = 0.0f;

    eul2 = eul1;
  }

  if (eul1.parity()) {
    eul1 = -eul1;
    eul2 = -eul2;
  }
}

/* Using explicit template instantiations in order to reduce compilation time. */
extern template void normalized_to_eul2(const float3x3 &mat,
                                        Euler3Base<float> &eul1,
                                        Euler3Base<float> &eul2);
extern template void normalized_to_eul2(const float3x3 &mat,
                                        EulerXYZBase<float> &eul1,
                                        EulerXYZBase<float> &eul2);
extern template void normalized_to_eul2(const double3x3 &mat,
                                        EulerXYZBase<double> &eul1,
                                        EulerXYZBase<double> &eul2);

template<typename T> QuaternionBase<T> normalized_to_quat_fast(const MatBase<T, 3, 3> &mat)
{
  BLI_assert(math::is_unit_scale(mat));
  /* Caller must ensure matrices aren't negative for valid results, see: #24291, #94231. */
  BLI_assert(!math::is_negative(mat));

  QuaternionBase<T> q;

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
      q.x = 0.25f * s;
      s = 1.0f / s;
      q.w = (mat[1][2] - mat[2][1]) * s;
      q.y = (mat[0][1] + mat[1][0]) * s;
      q.z = (mat[2][0] + mat[0][2]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.w == 0.0f && q.y == 0.0f && q.z == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.x = 1.0f;
      }
    }
    else {
      const T trace = 1.0f - mat[0][0] + mat[1][1] - mat[2][2];
      T s = 2.0f * math::sqrt(trace);
      if (mat[2][0] < mat[0][2]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q.y = 0.25f * s;
      s = 1.0f / s;
      q.w = (mat[2][0] - mat[0][2]) * s;
      q.x = (mat[0][1] + mat[1][0]) * s;
      q.z = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.w == 0.0f && q.x == 0.0f && q.z == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.y = 1.0f;
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
      q.z = 0.25f * s;
      s = 1.0f / s;
      q.w = (mat[0][1] - mat[1][0]) * s;
      q.x = (mat[2][0] + mat[0][2]) * s;
      q.y = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.w == 0.0f && q.x == 0.0f && q.y == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.z = 1.0f;
      }
    }
    else {
      /* NOTE(@ideasman42): A zero matrix will fall through to this block,
       * needed so a zero scaled matrices to return a quaternion without rotation, see: #101848.
       */
      const T trace = 1.0f + mat[0][0] + mat[1][1] + mat[2][2];
      T s = 2.0f * math::sqrt(trace);
      q.w = 0.25f * s;
      s = 1.0f / s;
      q.x = (mat[1][2] - mat[2][1]) * s;
      q.y = (mat[2][0] - mat[0][2]) * s;
      q.z = (mat[0][1] - mat[1][0]) * s;
      if (UNLIKELY((trace == 1.0f) && (q.x == 0.0f && q.y == 0.0f && q.z == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q.w = 1.0f;
      }
    }
  }

  BLI_assert(!(q.w < 0.0f));

  /* Sometimes normalization is necessary due to round-off errors in the above
   * calculations. The comparison here uses tighter tolerances than
   * BLI_ASSERT_UNIT_QUAT(), so it's likely that even after a few more
   * transformations the quaternion will still be considered unit-ish. */
  const T q_len_squared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  const T threshold = 0.0002f /* #BLI_ASSERT_UNIT_EPSILON */ * 3;
  if (math::abs(q_len_squared - 1.0f) >= threshold) {
    const T q_len_inv = 1.0 / math::sqrt(q_len_squared);
    q.x *= q_len_inv;
    q.y *= q_len_inv;
    q.z *= q_len_inv;
    q.w *= q_len_inv;
  }

  BLI_assert(math::is_unit_scale(VecBase<T, 4>(q)));
  return q;
}

template<typename T> QuaternionBase<T> normalized_to_quat_with_checks(const MatBase<T, 3, 3> &mat)
{
  const T det = math::determinant(mat);
  if (UNLIKELY(!std::isfinite(det))) {
    return QuaternionBase<T>::identity();
  }
  if (UNLIKELY(det < T(0))) {
    return normalized_to_quat_fast(-mat);
  }
  return normalized_to_quat_fast(mat);
}

/* Using explicit template instantiations in order to reduce compilation time. */
extern template QuaternionBase<float> normalized_to_quat_with_checks(const float3x3 &mat);
extern template QuaternionBase<double> normalized_to_quat_with_checks(const double3x3 &mat);

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const EulerXYZBase<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using DoublePrecision = typename TypeTraits<T>::DoublePrecision;
  const DoublePrecision cos_i = math::cos(DoublePrecision(rotation.x().radian()));
  const DoublePrecision cos_j = math::cos(DoublePrecision(rotation.y().radian()));
  const DoublePrecision cos_k = math::cos(DoublePrecision(rotation.z().radian()));
  const DoublePrecision sin_i = math::sin(DoublePrecision(rotation.x().radian()));
  const DoublePrecision sin_j = math::sin(DoublePrecision(rotation.y().radian()));
  const DoublePrecision sin_k = math::sin(DoublePrecision(rotation.z().radian()));
  const DoublePrecision cos_i_cos_k = cos_i * cos_k;
  const DoublePrecision cos_i_sin_k = cos_i * sin_k;
  const DoublePrecision sin_i_cos_k = sin_i * cos_k;
  const DoublePrecision sin_i_sin_k = sin_i * sin_k;

  MatT mat = MatT::identity();
  mat[0][0] = T(cos_j * cos_k);
  mat[1][0] = T(sin_j * sin_i_cos_k - cos_i_sin_k);
  mat[2][0] = T(sin_j * cos_i_cos_k + sin_i_sin_k);

  mat[0][1] = T(cos_j * sin_k);
  mat[1][1] = T(sin_j * sin_i_sin_k + cos_i_cos_k);
  mat[2][1] = T(sin_j * cos_i_sin_k - sin_i_cos_k);

  mat[0][2] = T(-sin_j);
  mat[1][2] = T(cos_j * sin_i);
  mat[2][2] = T(cos_j * cos_i);
  return mat;
}

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const Euler3Base<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  const int i_index = rotation.i_index();
  const int j_index = rotation.j_index();
  const int k_index = rotation.k_index();
#if 1 /* Reference. */
  EulerXYZBase<T> euler_xyz(rotation.ijk());
  const MatT mat = from_rotation<T, NumCol, NumRow>(rotation.parity() ? -euler_xyz : euler_xyz);
  MatT result = MatT::identity();
  result[i_index][i_index] = mat[0][0];
  result[j_index][i_index] = mat[1][0];
  result[k_index][i_index] = mat[2][0];
  result[i_index][j_index] = mat[0][1];
  result[j_index][j_index] = mat[1][1];
  result[k_index][j_index] = mat[2][1];
  result[i_index][k_index] = mat[0][2];
  result[j_index][k_index] = mat[1][2];
  result[k_index][k_index] = mat[2][2];
#else
  /* TODO(fclem): Manually inline and check performance difference. */
#endif
  return result;
}

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const QuaternionBase<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using DoublePrecision = typename TypeTraits<T>::DoublePrecision;
  const DoublePrecision q0 = numbers::sqrt2 * DoublePrecision(rotation.w);
  const DoublePrecision q1 = numbers::sqrt2 * DoublePrecision(rotation.x);
  const DoublePrecision q2 = numbers::sqrt2 * DoublePrecision(rotation.y);
  const DoublePrecision q3 = numbers::sqrt2 * DoublePrecision(rotation.z);

  const DoublePrecision qda = q0 * q1;
  const DoublePrecision qdb = q0 * q2;
  const DoublePrecision qdc = q0 * q3;
  const DoublePrecision qaa = q1 * q1;
  const DoublePrecision qab = q1 * q2;
  const DoublePrecision qac = q1 * q3;
  const DoublePrecision qbb = q2 * q2;
  const DoublePrecision qbc = q2 * q3;
  const DoublePrecision qcc = q3 * q3;

  MatT mat = MatT::identity();
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

/* Not technically speaking a simple rotation, but a whole transform. */
template<typename T, int NumCol, int NumRow>
[[nodiscard]] MatBase<T, NumCol, NumRow> from_rotation(const DualQuaternionBase<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  BLI_assert(is_normalized(rotation));
  /**
   * From:
   * "Skinning with Dual Quaternions"
   * Ladislav Kavan, Steven Collins, Jiri Zara, Carol O'Sullivan
   * Trinity College Dublin, Czech Technical University in Prague
   */
  /* Follow the paper notation. */
  const QuaternionBase<T> &c0 = rotation.quat;
  const QuaternionBase<T> &ce = rotation.trans;
  const T &w0 = c0.w, &x0 = c0.x, &y0 = c0.y, &z0 = c0.z;
  const T &we = ce.w, &xe = ce.x, &ye = ce.y, &ze = ce.z;
  /* Rotation. */
  MatT mat = from_rotation<T, NumCol, NumRow>(c0);
  /* Translation. */
  mat[3][0] = T(2) * (-we * x0 + xe * w0 - ye * z0 + ze * y0);
  mat[3][1] = T(2) * (-we * y0 + xe * z0 + ye * w0 - ze * x0);
  mat[3][2] = T(2) * (-we * z0 - xe * y0 + ye * x0 + ze * w0);
  /* Scale. */
  if (rotation.scale_weight != T(0)) {
    mat.template view<4, 4>() = mat * rotation.scale;
  }
  return mat;
}

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const CartesianBasis &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  MatT mat = MatT::identity();
  mat.x_axis() = to_vector<VecBase<T, 3>>(rotation.axes.x);
  mat.y_axis() = to_vector<VecBase<T, 3>>(rotation.axes.y);
  mat.z_axis() = to_vector<VecBase<T, 3>>(rotation.axes.z);
  return mat;
}

template<typename T, int NumCol, int NumRow, typename AngleT>
MatBase<T, NumCol, NumRow> from_rotation(const AxisAngleBase<T, AngleT> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  using Vec3T = typename MatT::vec3_type;
  const T angle_sin = sin(rotation.angle());
  const T angle_cos = cos(rotation.angle());
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

template<typename T, int NumCol, int NumRow>
MatBase<T, NumCol, NumRow> from_rotation(const AngleRadianBase<T> &rotation)
{
  using MatT = MatBase<T, NumCol, NumRow>;
  const T cos_i = cos(rotation);
  const T sin_i = sin(rotation);

  MatT mat = MatT::identity();
  mat[0][0] = cos_i;
  mat[1][0] = -sin_i;

  mat[0][1] = sin_i;
  mat[1][1] = cos_i;
  return mat;
}

/* Using explicit template instantiations in order to reduce compilation time. */
extern template MatBase<float, 2, 2> from_rotation(const AngleRadian &rotation);
extern template MatBase<float, 3, 3> from_rotation(const AngleRadian &rotation);
extern template MatBase<float, 3, 3> from_rotation(const EulerXYZ &rotation);
extern template MatBase<float, 4, 4> from_rotation(const EulerXYZ &rotation);
extern template MatBase<float, 3, 3> from_rotation(const Euler3 &rotation);
extern template MatBase<float, 4, 4> from_rotation(const Euler3 &rotation);
extern template MatBase<float, 3, 3> from_rotation(const Quaternion &rotation);
extern template MatBase<float, 4, 4> from_rotation(const Quaternion &rotation);
extern template MatBase<float, 3, 3> from_rotation(const AxisAngle &rotation);
extern template MatBase<float, 4, 4> from_rotation(const AxisAngle &rotation);
extern template MatBase<float, 3, 3> from_rotation(const AxisAngleCartesian &rotation);
extern template MatBase<float, 4, 4> from_rotation(const AxisAngleCartesian &rotation);

}  // namespace detail

template<typename T> [[nodiscard]] inline AngleRadianBase<T> to_angle(const MatBase<T, 2, 2> &mat)
{
  return detail::normalized_to_angle(mat);
}

template<typename T>
[[nodiscard]] inline Euler3Base<T> to_euler(const MatBase<T, 3, 3> &mat, EulerOrder order)
{
  Euler3Base<T> eul1(order), eul2(order);
  detail::normalized_to_eul2(mat, eul1, eul2);
  /* Return best, which is just the one with lowest values in it. */
  return (length_manhattan(VecBase<T, 3>(eul1)) > length_manhattan(VecBase<T, 3>(eul2))) ? eul2 :
                                                                                           eul1;
}

template<typename T> [[nodiscard]] inline EulerXYZBase<T> to_euler(const MatBase<T, 3, 3> &mat)
{
  EulerXYZBase<T> eul1, eul2;
  detail::normalized_to_eul2(mat, eul1, eul2);
  /* Return best, which is just the one with lowest values in it. */
  return (length_manhattan(VecBase<T, 3>(eul1)) > length_manhattan(VecBase<T, 3>(eul2))) ? eul2 :
                                                                                           eul1;
}

template<typename T>
[[nodiscard]] inline Euler3Base<T> to_euler(const MatBase<T, 4, 4> &mat, EulerOrder order)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_euler<T>(MatBase<T, 3, 3>(mat), order);
}

template<typename T> [[nodiscard]] inline EulerXYZBase<T> to_euler(const MatBase<T, 4, 4> &mat)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_euler<T>(MatBase<T, 3, 3>(mat));
}

template<typename T>
[[nodiscard]] inline Euler3Base<T> to_nearest_euler(const MatBase<T, 3, 3> &mat,
                                                    const Euler3Base<T> &reference)
{
  Euler3Base<T> eul1(reference.order()), eul2(reference.order());
  detail::normalized_to_eul2(mat, eul1, eul2);
  eul1 = eul1.wrapped_around(reference);
  eul2 = eul2.wrapped_around(reference);
  /* Return best, which is just the one with lowest values it in. */
  return (length_manhattan(VecBase<T, 3>(eul1) - VecBase<T, 3>(reference)) >
          length_manhattan(VecBase<T, 3>(eul2) - VecBase<T, 3>(reference))) ?
             eul2 :
             eul1;
}

template<typename T>
[[nodiscard]] inline EulerXYZBase<T> to_nearest_euler(const MatBase<T, 3, 3> &mat,
                                                      const EulerXYZBase<T> &reference)
{
  EulerXYZBase<T> eul1, eul2;
  detail::normalized_to_eul2(mat, eul1, eul2);
  eul1 = eul1.wrapped_around(reference);
  eul2 = eul2.wrapped_around(reference);
  /* Return best, which is just the one with lowest values it in. */
  return (length_manhattan(VecBase<T, 3>(eul1) - VecBase<T, 3>(reference)) >
          length_manhattan(VecBase<T, 3>(eul2) - VecBase<T, 3>(reference))) ?
             eul2 :
             eul1;
}

template<typename T>
[[nodiscard]] inline Euler3Base<T> to_nearest_euler(const MatBase<T, 4, 4> &mat,
                                                    const Euler3Base<T> &reference)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_euler<T>(MatBase<T, 3, 3>(mat), reference);
}

template<typename T>
[[nodiscard]] inline EulerXYZBase<T> to_nearest_euler(const MatBase<T, 4, 4> &mat,
                                                      const EulerXYZBase<T> &reference)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_euler<T>(MatBase<T, 3, 3>(mat), reference);
}

template<typename T>
[[nodiscard]] inline QuaternionBase<T> to_quaternion(const MatBase<T, 3, 3> &mat)
{
  return detail::normalized_to_quat_with_checks(mat);
}

template<typename T>
[[nodiscard]] inline QuaternionBase<T> to_quaternion(const MatBase<T, 4, 4> &mat)
{
  /* TODO(fclem): Avoid the copy with 3x3 ref. */
  return to_quaternion<T>(MatBase<T, 3, 3>(mat));
}

/**
 * This is "safe" in the sense that the input matrix may not actually encode a rotation but can
 * also contain shearing etc.
 */
template<typename T>
[[nodiscard]] inline QuaternionBase<T> normalized_to_quaternion_safe(const MatBase<T, 3, 3> &mat)
{
  /* Conversion to quaternion asserts when the matrix contains some kinds of shearing, conversion
   * to euler does not. */
  /* TODO: Find a better algorithm that can convert untrusted matrices to quaternions directly. */
  return to_quaternion(to_euler(mat));
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

template<bool AllowNegativeScale, typename T>
[[nodiscard]] inline VecBase<T, 2> to_scale(const MatBase<T, 2, 2> &mat)
{
  VecBase<T, 2> result = {length(mat.x), length(mat.y)};
  if constexpr (AllowNegativeScale) {
    if (UNLIKELY(is_negative(mat))) {
      result = -result;
    }
  }
  return result;
}

/* Implementation details. Use `to_euler` and `to_quaternion` instead. */
namespace detail {

template<typename T>
inline void to_rotation(const MatBase<T, 2, 2> &mat, AngleRadianBase<T> &r_rotation)
{
  r_rotation = to_angle<T>(mat);
}

template<typename T>
inline void to_rotation(const MatBase<T, 3, 3> &mat, QuaternionBase<T> &r_rotation)
{
  r_rotation = to_quaternion<T>(mat);
}

template<typename T>
inline void to_rotation(const MatBase<T, 3, 3> &mat, EulerXYZBase<T> &r_rotation)
{
  r_rotation = to_euler<T>(mat);
}

template<typename T>
inline void to_rotation(const MatBase<T, 3, 3> &mat, Euler3Base<T> &r_rotation)
{
  r_rotation = to_euler<T>(mat, r_rotation.order());
}

}  // namespace detail

template<bool AllowNegativeScale, typename T>
inline void to_rot_scale(const MatBase<T, 2, 2> &mat,
                         AngleRadianBase<T> &r_rotation,
                         VecBase<T, 2> &r_scale)
{
  MatBase<T, 2, 2> normalized_mat = normalize_and_get_size(mat, r_scale);
  if constexpr (AllowNegativeScale) {
    if (UNLIKELY(is_negative(normalized_mat))) {
      normalized_mat = -normalized_mat;
      r_scale = -r_scale;
    }
  }
  detail::to_rotation<T>(normalized_mat, r_rotation);
}

template<bool AllowNegativeScale, typename T>
inline void to_loc_rot_scale(const MatBase<T, 3, 3> &mat,
                             VecBase<T, 2> &r_location,
                             AngleRadianBase<T> &r_rotation,
                             VecBase<T, 2> &r_scale)
{
  r_location = mat.location();
  to_rot_scale<AllowNegativeScale>(MatBase<T, 2, 2>(mat), r_rotation, r_scale);
}

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
  detail::to_rotation<T>(normalized_mat, r_rotation);
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

/**
 * Same as #to_loc_rot_scale but is handles matrices that are not only location, rotation and scale
 * more gracefully, e.g. when the matrix has skew.
 */
template<bool AllowNegativeScale, typename T, typename RotationT>
inline void to_loc_rot_scale_safe(const MatBase<T, 4, 4> &mat,
                                  VecBase<T, 3> &r_location,
                                  RotationT &r_rotation,
                                  VecBase<T, 3> &r_scale)
{
  EulerXYZBase<T> euler_rotation;
  to_loc_rot_scale<AllowNegativeScale>(mat, r_location, euler_rotation, r_scale);
  if constexpr (std::is_same_v<std::decay_t<RotationT>, QuaternionBase<T>>) {
    r_rotation = to_quaternion(euler_rotation);
  }
  else {
    r_rotation = RotationT(euler_rotation);
  }
}

template<typename MatT> [[nodiscard]] MatT from_location(const typename MatT::loc_type &location)
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
[[nodiscard]] MatT from_loc_rot_scale(const typename MatT::loc_type &location,
                                      const RotationT &rotation,
                                      const VecBase<typename MatT::base_type, ScaleDim> &scale)
{
  using MatRotT =
      MatBase<typename MatT::base_type, MatT::loc_type::type_length, MatT::loc_type::type_length>;
  MatT mat = MatT(from_rot_scale<MatRotT>(rotation, scale));
  mat.location() = location;
  return mat;
}

template<typename MatT, typename RotationT>
[[nodiscard]] MatT from_loc_rot(const typename MatT::loc_type &location, const RotationT &rotation)
{
  using MatRotT =
      MatBase<typename MatT::base_type, MatT::loc_type::type_length, MatT::loc_type::type_length>;
  MatT mat = MatT(from_rotation<MatRotT>(rotation));
  mat.location() = location;
  return mat;
}

template<typename MatT, int ScaleDim>
[[nodiscard]] MatT from_loc_scale(const typename MatT::loc_type &location,
                                  const VecBase<typename MatT::base_type, ScaleDim> &scale)
{
  MatT mat = MatT(from_scale<MatT>(scale));
  mat.location() = location;
  return mat;
}

template<typename T> MatBase<T, 2, 2> from_direction(const VecBase<T, 2> &direction)
{
  BLI_assert(is_unit_scale(direction));
  return MatBase<T, 2, 2>(direction,
                          VecBase<T, 2>(direction.y, direction.x) * VecBase<T, 2>(-1, 1));
}

template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_orthonormal_axes(const VectorT forward, const VectorT up)
{
  BLI_assert(is_unit_scale(forward));
  BLI_assert(is_unit_scale(up));

  /* TODO(fclem): This is wrong. Forward is Y. */
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

template<typename MatT, typename VectorT> [[nodiscard]] MatT from_up_axis(const VectorT up)
{
  BLI_assert(is_unit_scale(up));
  using T = typename MatT::base_type;
  using Vec3T = VecBase<T, 3>;
  /* Duff, Tom, et al. "Building an orthonormal basis, revisited." JCGT 6.1 (2017). */
  T sign = up.z >= T(0) ? T(1) : T(-1);
  T a = T(-1) / (sign + up.z);
  T b = up.x * up.y * a;

  MatBase<T, 3, 3> basis;
  basis.x_axis() = Vec3T(1.0f + sign * square(up.x) * a, sign * b, -sign * up.x);
  basis.y_axis() = Vec3T(b, sign + square(up.y) * a, -up.y);
  basis.z_axis() = up;
  return MatT(basis);
}

template<typename MatT> [[nodiscard]] MatT orthogonalize(const MatT &mat, const Axis axis)
{
  using T = typename MatT::base_type;
  using Vec3T = VecBase<T, 3>;
  Vec3T scale;
  MatBase<T, 3, 3> R;
  R.x = normalize_and_get_length(mat.x_axis(), scale.x);
  R.y = normalize_and_get_length(mat.y_axis(), scale.y);
  R.z = normalize_and_get_length(mat.z_axis(), scale.z);
  /* NOTE(fclem) This is a direct port from `orthogonalize_m4()`.
   * To select the secondary axis, it checks if the candidate axis is not colinear.
   * The issue is that the candidate axes are not normalized so this dot product
   * check is kind of pointless.
   * Because of this, the target axis could still be colinear but pass the check. */
#if 1 /* Reproduce C API behavior. Do not normalize other axes. */
  switch (axis) {
    case Axis::X:
      R.y = mat.y_axis();
      R.z = mat.z_axis();
      break;
    case Axis::Y:
      R.x = mat.x_axis();
      R.z = mat.z_axis();
      break;
    case Axis::Z:
      R.x = mat.x_axis();
      R.y = mat.y_axis();
      break;
  }
#endif

  /**
   * The secondary axis is chosen as follow (X->Y, Y->X, Z->X).
   * If this axis is co-planar try the third axis.
   * If also co-planar, make up an axis by shuffling the primary axis coordinates (XYZ > YZX).
   */
  switch (axis) {
    case Axis::X:
      if (dot(R.x, R.y) < T(1)) {
        R.z = normalize(cross(R.x, R.y));
        R.y = cross(R.z, R.x);
      }
      else if (dot(R.x, R.z) < T(1)) {
        R.y = normalize(cross(R.z, R.x));
        R.z = cross(R.x, R.y);
      }
      else {
        R.z = normalize(cross(R.x, Vec3T(R.x.y, R.x.z, R.x.x)));
        R.y = cross(R.z, R.x);
      }
      break;
    case Axis::Y:
      if (dot(R.y, R.x) < T(1)) {
        R.z = normalize(cross(R.x, R.y));
        R.x = cross(R.y, R.z);
      }
      /* FIXME(fclem): THIS IS WRONG. Should be dot(R.y, R.z). Following C code for now... */
      else if (dot(R.x, R.z) < T(1)) {
        R.x = normalize(cross(R.y, R.z));
        R.z = cross(R.x, R.y);
      }
      else {
        R.x = normalize(cross(R.y, Vec3T(R.y.y, R.y.z, R.y.x)));
        R.z = cross(R.x, R.y);
      }
      break;
    case Axis::Z:
      if (dot(R.z, R.x) < T(1)) {
        R.y = normalize(cross(R.z, R.x));
        R.x = cross(R.y, R.z);
      }
      else if (dot(R.z, R.y) < T(1)) {
        R.x = normalize(cross(R.y, R.z));
        R.y = cross(R.z, R.x);
      }
      else {
        R.x = normalize(cross(Vec3T(R.z.y, R.z.z, R.z.x), R.z));
        R.y = cross(R.z, R.x);
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  /* Reapply the lost scale. */
  R.x *= scale.x;
  R.y *= scale.y;
  R.z *= scale.z;

  MatT result(R);
  result.location() = mat.location();
  return result;
}

template<typename MatT, typename VectorT>
[[nodiscard]] MatT from_origin_transform(const MatT &transform, const VectorT origin)
{
  return from_location<MatT>(origin) * transform * from_location<MatT>(-origin);
}

template<typename T>
VecBase<T, 2> transform_point(const MatBase<T, 2, 2> &mat, const VecBase<T, 2> &point)
{
  return mat * point;
}

template<typename T>
VecBase<T, 2> transform_point(const MatBase<T, 3, 3> &mat, const VecBase<T, 2> &point)
{
  return mat.template view<2, 2>() * point + mat.location();
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
MatBase<T, 4, 4> orthographic_infinite(T left, T right, T bottom, T top, T near_clip)
{
  const T x_delta = right - left;
  const T y_delta = top - bottom;

  MatBase<T, 4, 4> mat = MatBase<T, 4, 4>::identity();
  if (x_delta != 0 && y_delta != 0) {
    mat[0][0] = T(2.0) / x_delta;
    mat[3][0] = -(right + left) / x_delta;
    mat[1][1] = T(2.0) / y_delta;
    mat[3][1] = -(top + bottom) / y_delta;
    /* Page 17. Choosing an epsilon for 32 bit floating-point precision. */
    constexpr float eps = 2.4e-7f;
    /* From "Projection Matrix Tricks" by Eric Lengyel GDC 2007.
     * Following same procedure as the reference but for orthographic matrix.
     * This avoids degenerate matrix (0 determinant). */
    mat[2][2] = -eps;
    mat[3][2] = -1.0f - eps * near_clip;
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
    mat[3][3] = 0.0f;
  }
  return mat;
}

template<typename T>
MatBase<T, 4, 4> perspective_infinite(T left, T right, T bottom, T top, T near_clip)
{
  const T x_delta = right - left;
  const T y_delta = top - bottom;

  /* From "Projection Matrix Tricks" by Eric Lengyel GDC 2007. */
  MatBase<T, 4, 4> mat = MatBase<T, 4, 4>::identity();
  if (x_delta != 0 && y_delta != 0) {
    mat[0][0] = near_clip * T(2.0) / x_delta;
    mat[1][1] = near_clip * T(2.0) / y_delta;
    mat[2][0] = (right + left) / x_delta; /* NOTE: negate Z. */
    mat[2][1] = (top + bottom) / y_delta;
    /* Page 17. Choosing an epsilon for 32 bit floating-point precision. */
    constexpr float eps = 2.4e-7f;
    mat[2][2] = -1.0f;
    mat[2][3] = (eps - 1.0f);
    mat[3][2] = (eps - 2.0f) * near_clip;
    mat[3][3] = 0.0f;
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

template<typename T>
[[nodiscard]] MatBase<T, 4, 4> translate(const MatBase<T, 4, 4> &mat, const VecBase<T, 2> &offset)
{
  MatBase<T, 4, 4> result = mat;
  const bool is_perspective = mat[2][3] == -1.0f;
  const bool is_perspective_infinite = mat[2][2] == -1.0f;
  if (is_perspective || is_perspective_infinite) {
    result[2][0] -= mat[0][0] * offset.x / math::length(float3(mat[0][0], mat[1][0], mat[2][0]));
    result[2][1] -= mat[1][1] * offset.y / math::length(float3(mat[0][1], mat[1][1], mat[2][1]));
  }
  else {
    result[3][0] += offset.x;
    result[3][1] += offset.y;
  }
  return result;
}

extern template float4x4 orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip);
extern template float4x4 perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip);

}  // namespace projection

/** \} */

/**
 * Transform normal vectors, maintaining their unit length status, but implementing some
 * optimizations for identity matrix and uniform scaling.
 */
void transform_normals(const float3x3 &transform, MutableSpan<float3> normals);
void transform_normals(Span<float3> src, const float3x3 &transform, MutableSpan<float3> dst);

/** Transform point vectors with matrix multiplication, optionally using multi-threading. */
void transform_points(const float4x4 &transform,
                      MutableSpan<float3> points,
                      bool use_threading = true);
void transform_points(Span<float3> src,
                      const float4x4 &transform,
                      MutableSpan<float3> dst,
                      bool use_threading = true);

}  // namespace blender::math
