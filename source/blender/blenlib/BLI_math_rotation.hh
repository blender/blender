/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_rotation_types.hh"

#include "BLI_math_axis_angle.hh"
#include "BLI_math_euler.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_math_vector.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Rotation helpers
 * \{ */

/**
 * Rotate \a a by \a b. In other word, insert the \a b rotation before \a a.
 *
 * \note Since \a a is a #Quaternion it will cast \a b to a #Quaternion.
 * This might introduce some precision loss and have performance implication.
 */
template<typename T, typename RotT>
[[nodiscard]] QuaternionBase<T> rotate(const QuaternionBase<T> &a, const RotT &b);

/**
 * Rotate \a a by \a b. In other word, insert the \a b rotation before \a a.
 *
 * \note Since \a a is an #AxisAngle it will cast both \a a and \a b to #Quaternion.
 * This might introduce some precision loss and have performance implication.
 */
template<typename T, typename RotT, typename AngleT>
[[nodiscard]] AxisAngleBase<T, AngleT> rotate(const AxisAngleBase<T, AngleT> &a, const RotT &b);

/**
 * Rotate \a a by \a b. In other word, insert the \a b rotation before \a a.
 *
 * \note Since \a a is an #EulerXYZ it will cast both \a a and \a b to #MatBase<T, 3, 3>.
 * This might introduce some precision loss and have performance implication.
 */
template<typename T, typename RotT>
[[nodiscard]] EulerXYZBase<T> rotate(const EulerXYZBase<T> &a, const RotT &b);

/**
 * Rotate \a a by \a b. In other word, insert the \a b rotation before \a a.
 *
 * \note Since \a a is an #Euler3 it will cast both \a a and \a b to #MatBase<T, 3, 3>.
 * This might introduce some precision loss and have performance implication.
 */
template<typename T, typename RotT>
[[nodiscard]] Euler3Base<T> rotate(const Euler3Base<T> &a, const RotT &b);

/**
 * Return rotation from orientation \a a  to orientation \a b into another quaternion.
 */
template<typename T>
[[nodiscard]] QuaternionBase<T> rotation_between(const QuaternionBase<T> &a,
                                                 const QuaternionBase<T> &b);

/**
 * Create a orientation from a triangle plane and the axis formed by the segment(v1, v2).
 * Takes pre-computed \a normal from the triangle.
 * Used for Ngons when their normal is known.
 */
template<typename T>
[[nodiscard]] QuaternionBase<T> from_triangle(const VecBase<T, 3> &v1,
                                              const VecBase<T, 3> &v2,
                                              const VecBase<T, 3> &v3,
                                              const VecBase<T, 3> &normal);

/**
 * Create a orientation from a triangle plane and the axis formed by the segment(v1, v2).
 */
template<typename T>
[[nodiscard]] QuaternionBase<T> from_triangle(const VecBase<T, 3> &v1,
                                              const VecBase<T, 3> &v2,
                                              const VecBase<T, 3> &v3);

/**
 * Create a rotation from a vector and a basis rotation.
 * Used for tracking.
 * \a track_flag is supposed to be #Object.trackflag
 * \a up_flag is supposed to be #Object.upflag
 */
template<typename T>
[[nodiscard]] QuaternionBase<T> from_vector(const VecBase<T, 3> &vector,
                                            const AxisSigned track_flag,
                                            const Axis up_flag);

/**
 * Returns a quaternion for converting local space to tracking space.
 * This is slightly different than from_axis_conversion for legacy reasons.
 */
template<typename T>
[[nodiscard]] QuaternionBase<T> from_tracking(AxisSigned forward_axis, Axis up_axis);

/**
 * Convert euler rotation to gimbal rotation matrix.
 */
template<typename T> [[nodiscard]] MatBase<T, 3, 3> to_gimbal_axis(const Euler3Base<T> &rotation);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Angles
 * \{ */

/**
 * Extract rotation angle from a unit quaternion.
 * Returned angle is in [0..2pi] range.
 *
 * Unlike the angle between vectors, this does *NOT* return the shortest angle.
 * See `angle_of_signed` below for this.
 */
template<typename T> [[nodiscard]] AngleRadianBase<T> angle_of(const QuaternionBase<T> &q)
{
  BLI_assert(is_unit_scale(q));
  return T(2) * math::safe_acos(q.w);
}

/**
 * Extract rotation angle from a unit quaternion. Always return the shortest angle.
 * Returned angle is in [-pi..pi] range.
 *
 * `angle_of` with quaternion can exceed PI radians. Having signed versions of these functions
 * allows to use 'abs(angle_of_signed(...))' to get the shortest angle between quaternions with
 * higher precision than subtracting 2pi afterwards.
 */
template<typename T> [[nodiscard]] AngleRadianBase<T> angle_of_signed(const QuaternionBase<T> &q)
{
  BLI_assert(is_unit_scale(q));
  return T(2) * ((q.w >= T(0)) ? math::safe_acos(q.w) : -math::safe_acos(-q.w));
}

/**
 * Extract angle between 2 orientations.
 * For #Quaternion, the returned angle is in [0..2pi] range.
 * For other types, the returned angle is in [0..pi] range.
 * See `angle_of` for more detail.
 */
template<typename T>
[[nodiscard]] AngleRadianBase<T> angle_between(const QuaternionBase<T> &a,
                                               const QuaternionBase<T> &b)
{
  return angle_of(rotation_between(a, b));
}
template<typename T>
[[nodiscard]] AngleRadianBase<T> angle_between(const VecBase<T, 3> &a, const VecBase<T, 3> &b)
{
  BLI_assert(is_unit_scale(a));
  BLI_assert(is_unit_scale(b));
  return math::safe_acos(dot(a, b));
}
template<typename T>
[[nodiscard]] AngleFraction<T> angle_between(const AxisSigned a, const AxisSigned b)
{
  if (a == b) {
    return AngleFraction<T>::identity();
  }
  if (abs(a) == abs(b)) {
    return AngleFraction<T>::pi();
  }
  return AngleFraction<T>::pi() / 2;
}

/**
 * Extract angle between 2 orientations.
 * Returned angle is in [-pi..pi] range.
 * See `angle_of_signed` for more detail.
 */
template<typename T>
[[nodiscard]] AngleRadianBase<T> angle_between_signed(const QuaternionBase<T> &a,
                                                      const QuaternionBase<T> &b)
{
  return angle_of_signed(rotation_between(a, b));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Template implementations
 * \{ */

template<typename T, typename RotT>
[[nodiscard]] QuaternionBase<T> rotate(const QuaternionBase<T> &a, const RotT &b)
{
  return a * QuaternionBase<T>(b);
}

template<typename T, typename RotT, typename AngleT>
[[nodiscard]] AxisAngleBase<T, AngleT> rotate(const AxisAngleBase<T, AngleT> &a, const RotT &b)
{
  return AxisAngleBase<T, AngleT>(QuaternionBase<T>(a) * QuaternionBase<T>(b));
}

template<typename T, typename RotT>
[[nodiscard]] EulerXYZBase<T> rotate(const EulerXYZBase<T> &a, const RotT &b)
{
  MatBase<T, 3, 3> tmp = from_rotation<MatBase<T, 3, 3>>(a) * from_rotation<MatBase<T, 3, 3>>(b);
  return to_euler(tmp);
}

template<typename T, typename RotT>
[[nodiscard]] Euler3Base<T> rotate(const Euler3Base<T> &a, const RotT &b)
{
  const MatBase<T, 3, 3> tmp = from_rotation<MatBase<T, 3, 3>>(a) *
                               from_rotation<MatBase<T, 3, 3>>(b);
  return to_euler(tmp, a.order());
}

template<typename T>
[[nodiscard]] QuaternionBase<T> rotation_between(const QuaternionBase<T> &a,
                                                 const QuaternionBase<T> &b)
{
  return invert(a) * b;
}

template<typename T>
[[nodiscard]] QuaternionBase<T> from_triangle(const VecBase<T, 3> &v1,
                                              const VecBase<T, 3> &v2,
                                              const VecBase<T, 3> &v3,
                                              const VecBase<T, 3> &normal)
{
  /* Force to used an unused var to avoid the same function signature as the version without
   * `normal` argument. */
  UNUSED_VARS(v3);

  using Vec3T = VecBase<T, 3>;

  /* Move z-axis to face-normal. */
  const Vec3T z_axis = normal;
  Vec3T nor = normalize(Vec3T(z_axis.y, -z_axis.x, T(0)));
  if (is_zero(nor.xy())) {
    nor.x = T(1);
  }

  T angle = T(-0.5) * math::safe_acos(z_axis.z);
  T si = math::sin(angle);
  QuaternionBase<T> q1(math::cos(angle), nor.x * si, nor.y * si, T(0));

  /* Rotate back line v1-v2. */
  Vec3T line = transform_point(conjugate(q1), (v2 - v1));
  /* What angle has this line with x-axis? */
  line = normalize(Vec3T(line.x, line.y, T(0)));

  angle = T(0.5) * math::atan2(line.y, line.x);
  QuaternionBase<T> q2(math::cos(angle), 0.0, 0.0, math::sin(angle));

  return q1 * q2;
}

template<typename T>
[[nodiscard]] QuaternionBase<T> from_triangle(const VecBase<T, 3> &v1,
                                              const VecBase<T, 3> &v2,
                                              const VecBase<T, 3> &v3)
{
  return from_triangle(v1, v2, v3, normal_tri(v1, v2, v3));
}

template<typename T>
[[nodiscard]] QuaternionBase<T> from_vector(const VecBase<T, 3> &vector,
                                            const AxisSigned track_flag,
                                            const Axis up_flag)
{
  using Vec2T = VecBase<T, 2>;
  using Vec3T = VecBase<T, 3>;
  using Vec4T = VecBase<T, 4>;

  const T vec_len = length(vector);

  if (UNLIKELY(vec_len == 0.0f)) {
    return QuaternionBase<T>::identity();
  }

  const Axis axis = track_flag.axis();
  const Vec3T vec = track_flag.is_negative() ? vector : -vector;

  Vec3T rotation_axis;
  constexpr T eps = T(1e-4);
  T axis_len;
  switch (axis) {
    case Axis::X:
      rotation_axis = normalize_and_get_length(Vec3T(T(0), -vec.z, vec.y), axis_len);
      if (axis_len < eps) {
        rotation_axis = Vec3T(0, 1, 0);
      }
      break;
    case Axis::Y:
      rotation_axis = normalize_and_get_length(Vec3T(vec.z, T(0), -vec.x), axis_len);
      if (axis_len < eps) {
        rotation_axis = Vec3T(0, 0, 1);
      }
      break;
    default:
    case Axis::Z:
      rotation_axis = normalize_and_get_length(Vec3T(-vec.y, vec.x, T(0)), axis_len);
      if (axis_len < eps) {
        rotation_axis = Vec3T(1, 0, 0);
      }
      break;
  }
  /* TODO(fclem): Can optimize here by initializing AxisAngle using the cos an sin directly.
   * Avoiding the need for safe_acos and deriving sin from cos. */
  const T rotation_angle = math::safe_acos(vec[axis.as_int()] / vec_len);

  const QuaternionBase<T> q1 = to_quaternion(
      AxisAngleBase<T, AngleRadianBase<T>>(rotation_axis, rotation_angle));

  if (axis == up_flag) {
    /* Nothing else to do. */
    return q1;
  }

  /* Extract rotation between the up axis of the rotated space and the up axis. */
  /* There might be an easier way to get this angle directly from the quaternion representation. */
  const Vec3T rotated_up = transform_point(q1, Vec3T(0, 0, 1));

  /* Project using axes index instead of arithmetic. It's much faster and more precise. */
  const AxisSigned y_axis_signed = math::cross(AxisSigned(axis), AxisSigned(up_flag));
  const Axis x_axis = up_flag;
  const Axis y_axis = y_axis_signed.axis();

  Vec2T projected = normalize(Vec2T(rotated_up[x_axis.as_int()], rotated_up[y_axis.as_int()]));
  /* Flip sign for flipped axis. */
  if (y_axis_signed.is_negative()) {
    projected.y = -projected.y;
  }
  /* Not sure if this was a bug or not in the previous implementation.
   * Carry over this weird behavior to avoid regressions. */
  if (axis == Axis::Z) {
    projected = -projected;
  }

  const AngleCartesianBase<T> angle(projected.x, projected.y);
  const AngleCartesianBase<T> half_angle = angle / T(2);

  const QuaternionBase<T> q2(Vec4T(half_angle.cos(), vec * (half_angle.sin() / vec_len)));

  return q2 * q1;
}

template<typename T>
[[nodiscard]] QuaternionBase<T> from_tracking(AxisSigned forward_axis, Axis up_axis)
{
  BLI_assert(forward_axis.axis() != up_axis);

  /* Curve have Z forward, Y up, X left. */
  return QuaternionBase<T>(
      rotation_between(from_orthonormal_axes(AxisSigned::Z_POS, AxisSigned::Y_POS),
                       from_orthonormal_axes(forward_axis, AxisSigned(up_axis))));
}

template<typename T> [[nodiscard]] MatBase<T, 3, 3> to_gimbal_axis(const Euler3Base<T> &rotation)
{
  using Mat3T = MatBase<T, 3, 3>;
  using Vec3T = VecBase<T, 3>;
  const int i_index = rotation.i_index();
  const int j_index = rotation.j_index();
  const int k_index = rotation.k_index();

  Mat3T result;
  /* First axis is local. */
  result[i_index] = from_rotation<Mat3T>(rotation)[i_index];
  /* Second axis is local minus first rotation. */
  Euler3Base<T> tmp_rot = rotation;
  tmp_rot.i() = T(0);
  result[j_index] = from_rotation<Mat3T>(tmp_rot)[j_index];
  /* Last axis is global. */
  result[k_index] = Vec3T(0);
  result[k_index][k_index] = T(1);

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion from Cartesian Basis
 * \{ */

/**
 * Creates a quaternion from an axis triple.
 * This is faster and more precise than converting from another representation.
 */
template<typename T> QuaternionBase<T> to_quaternion(const CartesianBasis &rotation)
{
  /**
   * There are only 6 * 4 = 24 possible valid orthonormal orientations.
   * We precompute them and store them inside this switch using a key.
   * Generated using `generate_axes_to_quaternion_switch_cases()`.
   */
  constexpr auto map = [](AxisSigned x, AxisSigned y, AxisSigned z) {
    return x.as_int() << 16 | y.as_int() << 8 | z.as_int();
  };

  switch (map(rotation.axes.x, rotation.axes.y, rotation.axes.z)) {
    default:
      return QuaternionBase<T>::identity();
    case map(AxisSigned::Z_POS, AxisSigned::X_POS, AxisSigned::Y_POS):
      return QuaternionBase<T>{T(0.5), T(-0.5), T(-0.5), T(-0.5)};
    case map(AxisSigned::Y_NEG, AxisSigned::X_POS, AxisSigned::Z_POS):
      return QuaternionBase<T>{T(M_SQRT1_2), T(0), T(0), T(-M_SQRT1_2)};
    case map(AxisSigned::Z_NEG, AxisSigned::X_POS, AxisSigned::Y_NEG):
      return QuaternionBase<T>{T(0.5), T(0.5), T(0.5), T(-0.5)};
    case map(AxisSigned::Y_POS, AxisSigned::X_POS, AxisSigned::Z_NEG):
      return QuaternionBase<T>{T(0), T(M_SQRT1_2), T(M_SQRT1_2), T(0)};
    case map(AxisSigned::Z_NEG, AxisSigned::Y_POS, AxisSigned::X_POS):
      return QuaternionBase<T>{T(M_SQRT1_2), T(0), T(M_SQRT1_2), T(0)};
    case map(AxisSigned::Z_POS, AxisSigned::Y_POS, AxisSigned::X_NEG):
      return QuaternionBase<T>{T(M_SQRT1_2), T(0), T(-M_SQRT1_2), T(0)};
    case map(AxisSigned::X_NEG, AxisSigned::Y_POS, AxisSigned::Z_NEG):
      return QuaternionBase<T>{T(0), T(0), T(1), T(0)};
    case map(AxisSigned::Y_POS, AxisSigned::Z_POS, AxisSigned::X_POS):
      return QuaternionBase<T>{T(0.5), T(0.5), T(0.5), T(0.5)};
    case map(AxisSigned::X_NEG, AxisSigned::Z_POS, AxisSigned::Y_POS):
      return QuaternionBase<T>{T(0), T(0), T(M_SQRT1_2), T(M_SQRT1_2)};
    case map(AxisSigned::Y_NEG, AxisSigned::Z_POS, AxisSigned::X_NEG):
      return QuaternionBase<T>{T(0.5), T(0.5), T(-0.5), T(-0.5)};
    case map(AxisSigned::X_POS, AxisSigned::Z_POS, AxisSigned::Y_NEG):
      return QuaternionBase<T>{T(M_SQRT1_2), T(M_SQRT1_2), T(0), T(0)};
    case map(AxisSigned::Z_NEG, AxisSigned::X_NEG, AxisSigned::Y_POS):
      return QuaternionBase<T>{T(0.5), T(-0.5), T(0.5), T(0.5)};
    case map(AxisSigned::Y_POS, AxisSigned::X_NEG, AxisSigned::Z_POS):
      return QuaternionBase<T>{T(M_SQRT1_2), T(0), T(0), T(M_SQRT1_2)};
    case map(AxisSigned::Z_POS, AxisSigned::X_NEG, AxisSigned::Y_NEG):
      return QuaternionBase<T>{T(0.5), T(0.5), T(-0.5), T(0.5)};
    case map(AxisSigned::Y_NEG, AxisSigned::X_NEG, AxisSigned::Z_NEG):
      return QuaternionBase<T>{T(0), T(-M_SQRT1_2), T(M_SQRT1_2), T(0)};
    case map(AxisSigned::Z_POS, AxisSigned::Y_NEG, AxisSigned::X_POS):
      return QuaternionBase<T>{T(0), T(M_SQRT1_2), T(0), T(M_SQRT1_2)};
    case map(AxisSigned::X_NEG, AxisSigned::Y_NEG, AxisSigned::Z_POS):
      return QuaternionBase<T>{T(0), T(0), T(0), T(1)};
    case map(AxisSigned::Z_NEG, AxisSigned::Y_NEG, AxisSigned::X_NEG):
      return QuaternionBase<T>{T(0), T(-M_SQRT1_2), T(0), T(M_SQRT1_2)};
    case map(AxisSigned::X_POS, AxisSigned::Y_NEG, AxisSigned::Z_NEG):
      return QuaternionBase<T>{T(0), T(1), T(0), T(0)};
    case map(AxisSigned::Y_NEG, AxisSigned::Z_NEG, AxisSigned::X_POS):
      return QuaternionBase<T>{T(0.5), T(-0.5), T(0.5), T(-0.5)};
    case map(AxisSigned::X_POS, AxisSigned::Z_NEG, AxisSigned::Y_POS):
      return QuaternionBase<T>{T(M_SQRT1_2), T(-M_SQRT1_2), T(0), T(0)};
    case map(AxisSigned::Y_POS, AxisSigned::Z_NEG, AxisSigned::X_NEG):
      return QuaternionBase<T>{T(0.5), T(-0.5), T(-0.5), T(0.5)};
    case map(AxisSigned::X_NEG, AxisSigned::Z_NEG, AxisSigned::Y_NEG):
      return QuaternionBase<T>{T(0), T(0), T(-M_SQRT1_2), T(M_SQRT1_2)};
  }
}

/** \} */

}  // namespace blender::math

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Explicit Template Instantiations
 * \{ */

/* Using explicit template instantiations in order to reduce compilation time. */
extern template EulerXYZ to_euler(const AxisAngle &);
extern template EulerXYZ to_euler(const AxisAngleCartesian &);
extern template EulerXYZ to_euler(const Quaternion &);
extern template Euler3 to_euler(const AxisAngle &, EulerOrder);
extern template Euler3 to_euler(const AxisAngleCartesian &, EulerOrder);
extern template Euler3 to_euler(const Quaternion &, EulerOrder);
extern template Quaternion to_quaternion(const AxisAngle &);
extern template Quaternion to_quaternion(const AxisAngleCartesian &);
extern template Quaternion to_quaternion(const Euler3 &);
extern template Quaternion to_quaternion(const EulerXYZ &);
extern template AxisAngleCartesian to_axis_angle(const Euler3 &);
extern template AxisAngleCartesian to_axis_angle(const EulerXYZ &);
extern template AxisAngleCartesian to_axis_angle(const Quaternion &);
extern template AxisAngle to_axis_angle(const Euler3 &);
extern template AxisAngle to_axis_angle(const EulerXYZ &);
extern template AxisAngle to_axis_angle(const Quaternion &);

/** \} */

}  // namespace blender::math
