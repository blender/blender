/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_axis_angle_types.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_quaternion_types.hh"

#include "BLI_math_matrix.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Quaternion functions.
 * \{ */

/**
 * Dot product between two quaternions.
 * Equivalent to vector dot product.
 * Equivalent to component wise multiplication followed by summation of the result.
 */
template<typename T>
[[nodiscard]] inline T dot(const QuaternionBase<T> &a, const QuaternionBase<T> &b);

/**
 * Raise a unit #Quaternion \a q to the real \a y exponent.
 * \note This only works on unit quaternions and y != 0.
 * \note This is not a per component power.
 */
template<typename T> [[nodiscard]] QuaternionBase<T> pow(const QuaternionBase<T> &q, const T &y);

/**
 * Return the conjugate of the given quaternion.
 * If the quaternion \a q represent the rotation from A to B,
 * then the conjugate of \a q represents the rotation from B to A.
 */
template<typename T> [[nodiscard]] inline QuaternionBase<T> conjugate(const QuaternionBase<T> &a);

/**
 * Negate the quaternion if real component (w) is negative.
 */
template<typename T>
[[nodiscard]] inline QuaternionBase<T> canonicalize(const QuaternionBase<T> &q);

/**
 * Return invert of \a q or identity if \a q is ill-formed.
 * The invert allows quaternion division.
 * \note The inverse of \a q isn't the opposite rotation. This would be the conjugate.
 */
template<typename T> [[nodiscard]] inline QuaternionBase<T> invert(const QuaternionBase<T> &q);

/**
 * Return invert of \a q assuming it is a unit quaternion.
 * In this case, the inverse is just the conjugate. `conjugate(q)` could be use directly,
 * but this function shows the intent better, and asserts if \a q ever becomes non-unit-length.
 */
template<typename T>
[[nodiscard]] inline QuaternionBase<T> invert_normalized(const QuaternionBase<T> &q);

/**
 * Return a unit quaternion representing the same rotation as \a q or
 * the identity quaternion if \a q is ill-formed.
 */
template<typename T> [[nodiscard]] inline QuaternionBase<T> normalize(const QuaternionBase<T> &q);
template<typename T>
[[nodiscard]] inline QuaternionBase<T> normalize_and_get_length(const QuaternionBase<T> &q,
                                                                T &out_length);

/**
 * Use spherical interpolation between two quaternions.
 * Always interpolate along the shortest angle.
 */
template<typename T>
[[nodiscard]] inline QuaternionBase<T> interpolate(const QuaternionBase<T> &a,
                                                   const QuaternionBase<T> &b,
                                                   T t);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform functions.
 * \{ */

/**
 * Transform \a v by rotation using the quaternion \a q .
 */
template<typename T>
[[nodiscard]] inline VecBase<T, 3> transform_point(const QuaternionBase<T> &q,
                                                   const VecBase<T, 3> &v);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test functions.
 * \{ */

/**
 * Returns true if all components are exactly equal to 0.
 */
template<typename T> [[nodiscard]] inline bool is_zero(const QuaternionBase<T> &q)
{
  return q.w == T(0) && q.x == T(0) && q.y == T(0) && q.z == T(0);
}

/**
 * Returns true if the quaternions are equal within the given epsilon. Return false otherwise.
 */
template<typename T>
[[nodiscard]] inline bool is_equal(const QuaternionBase<T> &a,
                                   const QuaternionBase<T> &b,
                                   const T epsilon = T(0))
{
  return math::abs(a.w - b.w) <= epsilon && math::abs(a.y - b.y) <= epsilon &&
         math::abs(a.x - b.x) <= epsilon && math::abs(a.z - b.z) <= epsilon;
}

template<typename T> [[nodiscard]] inline bool is_unit_scale(const QuaternionBase<T> &q)
{
  /* Checks are flipped so NAN doesn't assert because we're making sure the value was
   * normalized and in the case we don't want NAN to be raising asserts since there
   * is nothing to be done in that case. */
  const T test_unit = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  return (!(math::abs(test_unit - T(1)) >= AssertUnitEpsilon<QuaternionBase<T>>::value) ||
          !(math::abs(test_unit) >= AssertUnitEpsilon<QuaternionBase<T>>::value));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion
 * \{ */

/* -------------- Conversions -------------- */

template<typename T> AngleRadianBase<T> QuaternionBase<T>::twist_angle(const Axis axis) const
{
  /* The calculation requires a canonical quaternion. */
  const VecBase<T, 4> input_vec(canonicalize(*this));

  return T(2) * AngleRadianBase<T>(input_vec[0], input_vec.yzw()[axis.as_int()]);
}

template<typename T> QuaternionBase<T> QuaternionBase<T>::swing(const Axis axis) const
{
  /* The calculation requires a canonical quaternion. */
  const QuaternionBase<T> input = canonicalize(*this);
  /* Compute swing by multiplying the original quaternion by inverted twist. */
  QuaternionBase<T> swing = input * invert_normalized(input.twist(axis));

  BLI_assert(math::abs(VecBase<T, 4>(swing)[axis.as_int() + 1]) < BLI_ASSERT_UNIT_EPSILON);
  return swing;
}

template<typename T> QuaternionBase<T> QuaternionBase<T>::twist(const Axis axis) const
{
  /* The calculation requires a canonical quaternion. */
  const VecBase<T, 4> input_vec(canonicalize(*this));

  AngleCartesianBase<T> half_angle = AngleCartesianBase<T>::from_point(
      input_vec[0], input_vec.yzw()[axis.as_int()]);

  VecBase<T, 4> twist(half_angle.cos(), T(0), T(0), T(0));
  twist[axis.as_int() + 1] = half_angle.sin();
  return QuaternionBase<T>(twist);
}

/* -------------- Methods -------------- */

template<typename T>
QuaternionBase<T> QuaternionBase<T>::wrapped_around(const QuaternionBase<T> &reference) const
{
  BLI_assert(is_unit_scale(*this));
  const QuaternionBase<T> &input = *this;
  T len;
  QuaternionBase<T> reference_normalized = normalize_and_get_length(reference, len);
  /* Skips degenerate case. */
  if (len < 1e-4f) {
    return input;
  }
  QuaternionBase<T> result = reference * invert_normalized(reference_normalized) * input;
  return (distance_squared(VecBase<T, 4>(-result), VecBase<T, 4>(reference)) <
          distance_squared(VecBase<T, 4>(result), VecBase<T, 4>(reference))) ?
             -result :
             result;
}

/* -------------- Functions -------------- */

template<typename T>
[[nodiscard]] inline T dot(const QuaternionBase<T> &a, const QuaternionBase<T> &b)
{
  return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

template<typename T> [[nodiscard]] QuaternionBase<T> pow(const QuaternionBase<T> &q, const T &y)
{
  BLI_assert(is_unit_scale(q));
  /* Reference material:
   * https://en.wikipedia.org/wiki/Quaternion
   *
   * The power of a quaternion raised to an arbitrary (real) exponent y is given by:
   * `q^x = ||q||^y * (cos(y * angle * 0.5) + n * sin(y * angle * 0.5))`
   * where `n` is the unit vector from the imaginary part of the quaternion and
   * where `angle` is the angle of the rotation given by `angle = 2 * acos(q.w)`.
   *
   * q being a unit quaternion, ||q||^y becomes 1 and is canceled out.
   *
   * `y * angle * 0.5` expands to `y * 2 * acos(q.w) * 0.5` which simplifies to `y * acos(q.w)`.
   */
  const T half_angle = y * math::safe_acos(q.w);
  return {math::cos(half_angle), math::sin(half_angle) * normalize(q.imaginary_part())};
}

template<typename T> [[nodiscard]] inline QuaternionBase<T> conjugate(const QuaternionBase<T> &a)
{
  return {a.w, -a.x, -a.y, -a.z};
}

template<typename T>
[[nodiscard]] inline QuaternionBase<T> canonicalize(const QuaternionBase<T> &q)
{
  return (q.w < T(0)) ? -q : q;
}

template<typename T> [[nodiscard]] inline QuaternionBase<T> invert(const QuaternionBase<T> &q)
{
  const T length_squared = dot(q, q);
  if (length_squared == T(0)) {
    return QuaternionBase<T>::identity();
  }
  return conjugate(q) * (T(1) / length_squared);
}

template<typename T>
[[nodiscard]] inline QuaternionBase<T> invert_normalized(const QuaternionBase<T> &q)
{
  BLI_assert(is_unit_scale(q));
  return conjugate(q);
}

template<typename T>
[[nodiscard]] inline QuaternionBase<T> normalize_and_get_length(const QuaternionBase<T> &q,
                                                                T &out_length)
{
  out_length = math::sqrt(dot(q, q));
  return (out_length != T(0)) ? (q * (T(1) / out_length)) : QuaternionBase<T>::identity();
}

template<typename T> [[nodiscard]] inline QuaternionBase<T> normalize(const QuaternionBase<T> &q)
{
  T len;
  return normalize_and_get_length(q, len);
}

/**
 * Generic function for implementing slerp
 * (quaternions and spherical vector coords).
 *
 * \param t: factor in [0..1]
 * \param cosom: dot product from normalized quaternions.
 * \return calculated weights.
 */
template<typename T>
[[nodiscard]] inline VecBase<T, 2> interpolate_dot_slerp(const T t, const T cosom)
{
  const T eps = T(1e-4);

  BLI_assert(IN_RANGE_INCL(cosom, T(-1.0001), T(1.0001)));

  VecBase<T, 2> w;
  T abs_cosom = math::abs(cosom);
  /* Within [-1..1] range, avoid aligned axis. */
  if (LIKELY(abs_cosom < (T(1) - eps))) {
    const T omega = math::acos(abs_cosom);
    const T sinom = math::sin(omega);

    w[0] = math::sin((T(1) - t) * omega) / sinom;
    w[1] = math::sin(t * omega) / sinom;
  }
  else {
    /* Fallback to lerp */
    w[0] = T(1) - t;
    w[1] = t;
  }

  /* Rotate around shortest angle. */
  if (cosom < T(0)) {
    w[0] = -w[0];
  }
  return w;
}

template<typename T>
[[nodiscard]] inline QuaternionBase<T> interpolate(const QuaternionBase<T> &a,
                                                   const QuaternionBase<T> &b,
                                                   T t)
{
  using Vec4T = VecBase<T, 4>;
  BLI_assert(is_unit_scale(a));
  BLI_assert(is_unit_scale(b));
  VecBase<T, 2> w = interpolate_dot_slerp(t, dot(a, b));
  return QuaternionBase<T>(w[0] * Vec4T(a) + w[1] * Vec4T(b));
}

template<typename T>
[[nodiscard]] inline VecBase<T, 3> transform_point(const QuaternionBase<T> &q,
                                                   const VecBase<T, 3> &v)
{
#if 0 /* Reference. */
  QuaternionBase<T> V(T(0), UNPACK3(v));
  QuaternionBase<T> R = q * V * conjugate(q);
  return {R.x, R.y, R.z};
#else
  /* `S = q * V` */
  QuaternionBase<T> S;
  S.w = /* q.w * 0.0  */ -q.x * v.x - q.y * v.y - q.z * v.z;
  S.x = q.w * v.x /* + q.x * 0.0 */ + q.y * v.z - q.z * v.y;
  S.y = q.w * v.y /* + q.y * 0.0 */ + q.z * v.x - q.x * v.z;
  S.z = q.w * v.z /* + q.z * 0.0 */ + q.x * v.y - q.y * v.x;
  /* `R = S * conjugate(q)` */
  VecBase<T, 3> R;
  /* R.w = S.w * q.w + S.x * q.x + S.y * q.y + S.z * q.z = 0.0; */
  R.x = S.w * -q.x + S.x * q.w - S.y * q.z + S.z * q.y;
  R.y = S.w * -q.y + S.y * q.w - S.z * q.x + S.x * q.z;
  R.z = S.w * -q.z + S.z * q.w - S.x * q.y + S.y * q.x;
  return R;
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dual-Quaternion
 * \{ */

/* -------------- Constructors -------------- */

template<typename T>
DualQuaternionBase<T>::DualQuaternionBase(const QuaternionBase<T> &non_dual,
                                          const QuaternionBase<T> &dual)
    : quat(non_dual), trans(dual), scale_weight(0), quat_weight(1)
{
  BLI_assert(is_unit_scale(non_dual));
}

template<typename T>
DualQuaternionBase<T>::DualQuaternionBase(const QuaternionBase<T> &non_dual,
                                          const QuaternionBase<T> &dual,
                                          const MatBase<T, 4, 4> &scale_mat)
    : quat(non_dual), trans(dual), scale(scale_mat), scale_weight(1), quat_weight(1)
{
  BLI_assert(is_unit_scale(non_dual));
}

/* -------------- Operators -------------- */

template<typename T>
DualQuaternionBase<T> &DualQuaternionBase<T>::operator+=(const DualQuaternionBase<T> &b)
{
  DualQuaternionBase<T> &a = *this;
  /* Sum rotation and translation. */

  /* Make sure we interpolate quaternions in the right direction. */
  if (dot(a.quat, b.quat) < 0) {
    a.quat.w -= b.quat.w;
    a.quat.x -= b.quat.x;
    a.quat.y -= b.quat.y;
    a.quat.z -= b.quat.z;

    a.trans.w -= b.trans.w;
    a.trans.x -= b.trans.x;
    a.trans.y -= b.trans.y;
    a.trans.z -= b.trans.z;
  }
  else {
    a.quat.w += b.quat.w;
    a.quat.x += b.quat.x;
    a.quat.y += b.quat.y;
    a.quat.z += b.quat.z;

    a.trans.w += b.trans.w;
    a.trans.x += b.trans.x;
    a.trans.y += b.trans.y;
    a.trans.z += b.trans.z;
  }

  a.quat_weight += b.quat_weight;

  if (b.scale_weight > T(0)) {
    if (a.scale_weight > T(0)) {
      /* Weighted sum of scale matrices (sum of components). */
      a.scale += b.scale;
      a.scale_weight += b.scale_weight;
    }
    else {
      /* No existing scale. Replace. */
      a.scale = b.scale;
      a.scale_weight = b.scale_weight;
    }
  }
  return *this;
}

template<typename T> DualQuaternionBase<T> &DualQuaternionBase<T>::operator*=(const T &t)
{
  BLI_assert(t >= 0);
  DualQuaternionBase<T> &q = *this;

  q.quat.w *= t;
  q.quat.x *= t;
  q.quat.y *= t;
  q.quat.z *= t;

  q.trans.w *= t;
  q.trans.x *= t;
  q.trans.y *= t;
  q.trans.z *= t;

  q.quat_weight *= t;

  if (q.scale_weight > T(0)) {
    q.scale *= t;
    q.scale_weight *= t;
  }
  return *this;
}

/* -------------- Functions -------------- */

/**
 * Apply all accumulated weights to the dual-quaternions.
 * Also make sure the rotation quaternions is normalized.
 * \note The C version of this function does not normalize the quaternion. This makes other
 * operations like transform and matrix conversion more complex.
 * \note Returns identity #DualQuaternion if degenerate.
 */
template<typename T>
[[nodiscard]] DualQuaternionBase<T> normalize(const DualQuaternionBase<T> &dual_quat)
{
  const T norm_weighted = math::sqrt(dot(dual_quat.quat, dual_quat.quat));
  /* NOTE(fclem): Should this be an epsilon? */
  if (norm_weighted == T(0)) {
    /* The dual-quaternion was zero initialized or is degenerate. Return identity. */
    return DualQuaternionBase<T>::identity();
  }

  const T inv_norm_weighted = T(1) / norm_weighted;

  DualQuaternionBase<T> dq = dual_quat;
  dq.quat = dq.quat * inv_norm_weighted;
  dq.trans = dq.trans * inv_norm_weighted;

  /* Handle scale if needed. */
  if (dq.scale_weight > T(0)) {
    /* Compensate for any dual quaternions added without scale.
     * This is an optimization so that we can skip the scale part when not needed. */
    const float missing_uniform_scale = dq.quat_weight - dq.scale_weight;

    if (missing_uniform_scale > T(0)) {
      dq.scale[0][0] += missing_uniform_scale;
      dq.scale[1][1] += missing_uniform_scale;
      dq.scale[2][2] += missing_uniform_scale;
      dq.scale[3][3] += missing_uniform_scale;
    }
    /* Per component scalar product. */
    dq.scale *= T(1) / dq.quat_weight;
    dq.scale_weight = T(1);
  }
  dq.quat_weight = T(1);
  return dq;
}

/**
 * Transform \a point using the dual-quaternion \a dq .
 * Applying the #DualQuaternion transform can only happen if the #DualQuaternion was normalized
 * first. Optionally outputs crazy space matrix.
 */
template<typename T>
[[nodiscard]] VecBase<T, 3> transform_point(const DualQuaternionBase<T> &dq,
                                            const VecBase<T, 3> &point,
                                            MatBase<T, 3, 3> *r_crazy_space_mat = nullptr)
{
  BLI_assert(is_normalized(dq));
  BLI_assert(is_unit_scale(dq.quat));
  /**
   * From:
   * "Skinning with Dual Quaternions"
   * Ladislav Kavan, Steven Collins, Jiri Zara, Carol O'Sullivan
   * Trinity College Dublin, Czech Technical University in Prague
   */
  /* Follow the paper notation. */
  const T &w0 = dq.quat.w, &x0 = dq.quat.x, &y0 = dq.quat.y, &z0 = dq.quat.z;
  const T &we = dq.trans.w, &xe = dq.trans.x, &ye = dq.trans.y, &ze = dq.trans.z;
  /* Part 3.4 - The Final Algorithm. */
  VecBase<T, 3> t;
  t[0] = T(2) * (-we * x0 + xe * w0 - ye * z0 + ze * y0);
  t[1] = T(2) * (-we * y0 + xe * z0 + ye * w0 - ze * x0);
  t[2] = T(2) * (-we * z0 - xe * y0 + ye * x0 + ze * w0);
  /* Isolate rotation matrix to easily output crazy-space mat. */
  MatBase<T, 3, 3> M;
  M[0][0] = (w0 * w0) + (x0 * x0) - (y0 * y0) - (z0 * z0); /* Same as `1 - 2y0^2 - 2z0^2`. */
  M[0][1] = T(2) * ((x0 * y0) + (w0 * z0));
  M[0][2] = T(2) * ((x0 * z0) - (w0 * y0));

  M[1][0] = T(2) * ((x0 * y0) - (w0 * z0));
  M[1][1] = (w0 * w0) + (y0 * y0) - (x0 * x0) - (z0 * z0); /* Same as `1 - 2x0^2 - 2z0^2`. */
  M[1][2] = T(2) * ((y0 * z0) + (w0 * x0));

  M[2][1] = T(2) * ((y0 * z0) - (w0 * x0));
  M[2][2] = (w0 * w0) + (z0 * z0) - (x0 * x0) - (y0 * y0); /* Same as `1 - 2x0^2 - 2y0^2`. */
  M[2][0] = T(2) * ((x0 * z0) + (w0 * y0));

  VecBase<T, 3> result = point;
  /* Apply scaling. */
  if (dq.scale_weight != T(0)) {
    /* NOTE(fclem): This is weird that this is also adding translation even if it is marked as
     * scale matrix. Follows the old C implementation for now... */
    result = transform_point(dq.scale, result);
  }
  /* Apply rotation and translation. */
  result = transform_point(M, result) + t;
  /* Compute crazy-space correction matrix. */
  if (r_crazy_space_mat != nullptr) {
    if (dq.scale_weight) {
      *r_crazy_space_mat = M * dq.scale.template view<3, 3>();
    }
    else {
      *r_crazy_space_mat = M;
    }
  }
  return result;
}

/**
 * Convert transformation \a mat with parent transform \a basemat into a dual-quaternion
 * representation.
 *
 * This allows volume preserving deformation for skinning.
 */
template<typename T>
[[nodiscard]] DualQuaternionBase<T> to_dual_quaternion(const MatBase<T, 4, 4> &mat,
                                                       const MatBase<T, 4, 4> &basemat)
{
  /**
   * Conversion routines between (regular quaternion, translation) and dual quaternion.
   *
   * Version 1.0.0, February 7th, 2007
   *
   * SPDX-License-Identifier: Zlib
   * Copyright 2006-2007 University of Dublin, Trinity College, All Rights Reserved.
   *
   * Changes for Blender:
   * - renaming, style changes and optimizations
   * - added support for scaling
   */
  using Mat4T = MatBase<T, 4, 4>;
  using Vec3T = VecBase<T, 3>;

  /* Split scaling and rotation.
   * There is probably a faster way to do this. It is currently done like this to correctly get
   * negative scaling. */
  Mat4T baseRS = mat * basemat;

  Mat4T R, scale;
  const bool has_scale = !is_orthonormal(mat) || is_negative(mat) ||
                         length_squared(to_scale(baseRS) - T(1)) > square_f(1e-4f);
  if (has_scale) {
    /* Extract Rotation and Scale. */
    const Mat4T baseinv = invert(basemat);

    /* Extra orthogonalize, to avoid flipping with stretched bones. */
    QuaternionBase<T> basequat = to_quaternion(normalize(orthogonalize(baseRS, Axis::Y)));

    Mat4T baseR = from_rotation<Mat4T>(basequat);
    baseR.location() = baseRS.location();

    R = baseR * baseinv;

    const Mat4T S = invert(baseR) * baseRS;
    /* Set scaling part. */
    scale = basemat * S * baseinv;
  }
  else {
    /* Input matrix does not contain scaling. */
    R = mat;
  }

  /* Non-dual part. */
  const QuaternionBase<T> q = to_quaternion(normalize(R));

  /* Dual part. */
  const Vec3T &t = R.location().xyz();
  QuaternionBase<T> d;
  d.w = T(-0.5) * (+t.x * q.x + t.y * q.y + t.z * q.z);
  d.x = T(+0.5) * (+t.x * q.w + t.y * q.z - t.z * q.y);
  d.y = T(+0.5) * (-t.x * q.z + t.y * q.w + t.z * q.x);
  d.z = T(+0.5) * (+t.x * q.y - t.y * q.x + t.z * q.w);

  if (has_scale) {
    return DualQuaternionBase<T>(q, d, scale);
  }

  return DualQuaternionBase<T>(q, d);
}

/** \} */

}  // namespace blender::math

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Conversion to Euler
 * \{ */

template<typename T, typename AngleT = AngleRadian>
AxisAngleBase<T, AngleT> to_axis_angle(const QuaternionBase<T> &quat)
{
  BLI_assert(is_unit_scale(quat));

  VecBase<T, 3> axis = VecBase<T, 3>(quat.x, quat.y, quat.z);
  T cos_half_angle = quat.w;
  T sin_half_angle = math::length(axis);
  /* Prevent division by zero for axis conversion. */
  if (sin_half_angle < T(0.0005)) {
    sin_half_angle = T(1);
    axis[1] = T(1);
  }
  /* Normalize the axis. */
  axis /= sin_half_angle;

  /* Leverage AngleT implementation of double angle. */
  AngleT angle = AngleT(cos_half_angle, sin_half_angle) * 2;

  return AxisAngleBase<T, AngleT>(axis, angle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion to Euler
 * \{ */

template<typename T> EulerXYZBase<T> to_euler(const QuaternionBase<T> &quat)
{
  using Mat3T = MatBase<T, 3, 3>;
  BLI_assert(is_unit_scale(quat));
  Mat3T unit_mat = from_rotation<Mat3T>(quat);
  return to_euler<T>(unit_mat);
}

template<typename T> Euler3Base<T> to_euler(const QuaternionBase<T> &quat, EulerOrder order)
{
  using Mat3T = MatBase<T, 3, 3>;
  BLI_assert(is_unit_scale(quat));
  Mat3T unit_mat = from_rotation<Mat3T>(quat);
  return to_euler<T>(unit_mat, order);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion from/to Expmap
 * \{ */

/* Prototype needed to avoid interdependencies of headers. */
template<typename T, typename AngleT>
QuaternionBase<T> to_quaternion(const AxisAngleBase<T, AngleT> &axis_angle);

template<typename T> QuaternionBase<T> QuaternionBase<T>::expmap(const VecBase<T, 3> &expmap)
{
  using AxisAngleT = AxisAngleBase<T, AngleRadianBase<T>>;
  /* Obtain axis/angle representation. */
  T angle;
  const VecBase<T, 3> axis = normalize_and_get_length(expmap, angle);
  if (LIKELY(angle != T(0))) {
    return to_quaternion(AxisAngleT(axis, angle_wrap_rad(angle)));
  }
  return QuaternionBase<T>::identity();
}

template<typename T> VecBase<T, 3> QuaternionBase<T>::expmap() const
{
  using AxisAngleT = AxisAngleBase<T, AngleRadianBase<T>>;
  BLI_assert(is_unit_scale(*this));
  const AxisAngleT axis_angle = to_axis_angle(*this);
  return axis_angle.axis() * axis_angle.angle().radian();
}

/** \} */

}  // namespace blender::math
