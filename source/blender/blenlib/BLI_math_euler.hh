/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_axis_angle_types.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_quaternion_types.hh"

#include "BLI_math_axis_angle.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name EulerXYZ
 * \{ */

template<typename T> EulerXYZBase<T> EulerXYZBase<T>::wrapped() const
{
  EulerXYZBase<T> result(*this);
  result.x() = AngleRadianBase<T>(result.x()).wrapped().radian();
  result.y() = AngleRadianBase<T>(result.y()).wrapped().radian();
  result.z() = AngleRadianBase<T>(result.z()).wrapped().radian();
  return result;
}

template<typename T>
EulerXYZBase<T> EulerXYZBase<T>::wrapped_around(const EulerXYZBase &reference) const
{
  EulerXYZBase<T> result(*this);
  result.x() = AngleRadianBase<T>(result.x()).wrapped_around(reference.x()).radian();
  result.y() = AngleRadianBase<T>(result.y()).wrapped_around(reference.y()).radian();
  result.z() = AngleRadianBase<T>(result.z()).wrapped_around(reference.z()).radian();
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion to Quaternions
 * \{ */

template<typename T> QuaternionBase<T> to_quaternion(const EulerXYZBase<T> &eul)
{
  using AngleT = typename EulerXYZBase<T>::AngleT;
  const AngleT h_angle_i = eul.x() / 2;
  const AngleT h_angle_j = eul.y() / 2;
  const AngleT h_angle_k = eul.z() / 2;
  const T cos_i = math::cos(h_angle_i);
  const T cos_j = math::cos(h_angle_j);
  const T cos_k = math::cos(h_angle_k);
  const T sin_i = math::sin(h_angle_i);
  const T sin_j = math::sin(h_angle_j);
  const T sin_k = math::sin(h_angle_k);
  const T cos_cos = cos_i * cos_k;
  const T cos_sin = cos_i * sin_k;
  const T sin_cos = sin_i * cos_k;
  const T sin_sin = sin_i * sin_k;

  QuaternionBase<T> quat;
  quat.w = cos_j * cos_cos + sin_j * sin_sin;
  quat.x = cos_j * sin_cos - sin_j * cos_sin;
  quat.y = cos_j * sin_sin + sin_j * cos_cos;
  quat.z = cos_j * cos_sin - sin_j * sin_cos;
  return quat;
}

template<typename T> QuaternionBase<T> to_quaternion(const Euler3Base<T> &eulO)
{
  /* Swizzle to XYZ. */
  EulerXYZBase<T> eul_xyz{eulO.ijk()};
  /* Flip with parity. */
  eul_xyz.y() = eulO.parity() ? -eul_xyz.y() : eul_xyz.y();
  /* Quaternion conversion. */
  QuaternionBase<T> quat = to_quaternion(eul_xyz);
  /* Swizzle back from XYZ. */
  VecBase<T, 3> quat_xyz;
  quat_xyz[eulO.i_index()] = quat.x;
  quat_xyz[eulO.j_index()] = eulO.parity() ? -quat.y : quat.y;
  quat_xyz[eulO.k_index()] = quat.z;

  return {quat.w, UNPACK3(quat_xyz)};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion to axis angles
 * \{ */

template<typename T, typename AngleT = AngleRadian>
AxisAngleBase<T, AngleT> to_axis_angle(const EulerXYZBase<T> &euler)
{
  /* Use quaternions as intermediate representation for now... */
  return to_axis_angle<T, AngleT>(to_quaternion(euler));
}

template<typename T, typename AngleT = AngleRadian>
AxisAngleBase<T, AngleT> to_axis_angle(const Euler3Base<T> &euler)
{
  /* Use quaternions as intermediate representation for now... */
  return to_axis_angle<T, AngleT>(to_quaternion(euler));
}

/** \} */

}  // namespace blender::math
