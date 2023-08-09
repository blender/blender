/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_rotation.hh"
#include "BLI_math_rotation_legacy.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

namespace blender::math {

template EulerXYZ to_euler(const AxisAngle &);
template EulerXYZ to_euler(const AxisAngleCartesian &);
template EulerXYZ to_euler(const Quaternion &);
template Euler3 to_euler(const AxisAngle &, EulerOrder);
template Euler3 to_euler(const AxisAngleCartesian &, EulerOrder);
template Euler3 to_euler(const Quaternion &, EulerOrder);
template Quaternion to_quaternion(const AxisAngle &);
template Quaternion to_quaternion(const AxisAngleCartesian &);
template Quaternion to_quaternion(const Euler3 &);
template Quaternion to_quaternion(const EulerXYZ &);
template AxisAngleCartesian to_axis_angle(const Euler3 &);
template AxisAngleCartesian to_axis_angle(const EulerXYZ &);
template AxisAngleCartesian to_axis_angle(const Quaternion &);
template AxisAngle to_axis_angle(const Euler3 &);
template AxisAngle to_axis_angle(const EulerXYZ &);
template AxisAngle to_axis_angle(const Quaternion &);

#if 0 /* Only for reference. */
void generate_axes_to_quaternion_switch_cases()
{
  std::cout << "default: *this = identity(); break;" << std::endl;
  /* Go through all 32 cases. Only 23 valid and 1 is identity. */
  for (int i : IndexRange(6)) {
    for (int j : IndexRange(6)) {
      const AxisSigned forward = AxisSigned(i);
      const AxisSigned up = AxisSigned(j);
      /* Filter the 12 invalid cases. Fall inside the default case. */
      if (Axis(forward) == Axis(up)) {
        continue;
      }
      /* Filter the identity case. Fall inside the default case. */
      if (forward == AxisSigned::Y_POS && up == AxisSigned::Z_POS) {
        continue;
      }

      VecBase<AxisSigned, 3> axes{cross(forward, up), forward, up};

      float3x3 mat;
      mat.x_axis() = float3(axes.x);
      mat.y_axis() = float3(axes.y);
      mat.z_axis() = float3(axes.z);

      math::Quaternion q = to_quaternion(mat);
      /* Create a integer value out of the 4 possible component values (+sign). */
      int4 p = int4(round(sign(float4(q)) * min(pow(float4(q), 2.0f), float4(0.75)) * 4.0));

      auto format_component = [](int value) {
        switch (abs(value)) {
          default:
          case 0:
            return "T(0)";
          case 1:
            return (value > 0) ? "T(0.5)" : "T(-0.5)";
          case 2:
            return (value > 0) ? "T(M_SQRT1_2)" : "T(-M_SQRT1_2)";
          case 3:
            return (value > 0) ? "T(1)" : "T(-1)";
        }
      };
      auto format_axis = [](AxisSigned axis) {
        switch (axis) {
          default:
          case AxisSigned::X_POS:
            return "AxisSigned::X_POS";
          case AxisSigned::Y_POS:
            return "AxisSigned::Y_POS";
          case AxisSigned::Z_POS:
            return "AxisSigned::Z_POS";
          case AxisSigned::X_NEG:
            return "AxisSigned::X_NEG";
          case AxisSigned::Y_NEG:
            return "AxisSigned::Y_NEG";
          case AxisSigned::Z_NEG:
            return "AxisSigned::Z_NEG";
        }
      };
      /* Use same code function as in the switch case. */
      std::cout << "case ";
      std::cout << format_axis(axes.x) << " << 16 | ";
      std::cout << format_axis(axes.y) << " << 8 | ";
      std::cout << format_axis(axes.z);
      std::cout << ": *this = {";
      std::cout << format_component(p.x) << ", ";
      std::cout << format_component(p.y) << ", ";
      std::cout << format_component(p.z) << ", ";
      std::cout << format_component(p.w) << "}; break;";
      std::cout << std::endl;
    }
  }
}
#endif

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

std::ostream &operator<<(std::ostream &stream, EulerOrder order)
{
  switch (order) {
    default:
    case XYZ:
      return stream << "XYZ";
    case XZY:
      return stream << "XZY";
    case YXZ:
      return stream << "YXZ";
    case YZX:
      return stream << "YZX";
    case ZXY:
      return stream << "ZXY";
    case ZYX:
      return stream << "ZYX";
  }
}

}  // namespace blender::math
