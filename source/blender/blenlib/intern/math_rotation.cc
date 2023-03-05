/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"
#include "BLI_math_rotation_legacy.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

namespace blender::math::detail {

template AxisAngle<float>::operator EulerXYZ<float>() const;
template AxisAngle<float>::operator Quaternion<float>() const;
template EulerXYZ<float>::operator AxisAngle<float>() const;
template EulerXYZ<float>::operator Quaternion<float>() const;
template Quaternion<float>::operator AxisAngle<float>() const;
template Quaternion<float>::operator EulerXYZ<float>() const;

template AxisAngle<double>::operator EulerXYZ<double>() const;
template AxisAngle<double>::operator Quaternion<double>() const;
template EulerXYZ<double>::operator AxisAngle<double>() const;
template EulerXYZ<double>::operator Quaternion<double>() const;
template Quaternion<double>::operator AxisAngle<double>() const;
template Quaternion<double>::operator EulerXYZ<double>() const;

}  // namespace blender::math::detail

namespace blender::math {

float3 rotate_direction_around_axis(const float3 &direction, const float3 &axis, const float angle)
{
  BLI_ASSERT_UNIT_V3(direction);
  BLI_ASSERT_UNIT_V3(axis);

  const float3 axis_scaled = axis * math::dot(direction, axis);
  const float3 diff = direction - axis_scaled;
  const float3 cross = math::cross(axis, diff);

  return axis_scaled + diff * std::cos(angle) + cross * std::sin(angle);
}

float3 rotate_around_axis(const float3 &vector,
                          const float3 &center,
                          const float3 &axis,
                          const float angle)

{
  float3 result = vector - center;
  float mat[3][3];
  axis_angle_normalized_to_mat3(mat, axis, angle);
  mul_m3_v3(mat, result);
  return result + center;
}

}  // namespace blender::math
