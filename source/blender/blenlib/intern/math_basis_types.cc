/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_basis_types.hh"

#include <ostream>

namespace blender::math {

std::ostream &operator<<(std::ostream &stream, const Axis axis)
{
  switch (axis.axis_) {
    default:
      BLI_assert_unreachable();
      return stream << "Invalid Axis";
    case Axis::Value::X:
      return stream << 'X';
    case Axis::Value::Y:
      return stream << 'Y';
    case Axis::Value::Z:
      return stream << 'Z';
  }
}
std::ostream &operator<<(std::ostream &stream, const AxisSigned axis)
{
  switch (axis.axis_) {
    default:
      BLI_assert_unreachable();
      return stream << "Invalid AxisSigned";
    case AxisSigned::Value::X_POS:
    case AxisSigned::Value::Y_POS:
    case AxisSigned::Value::Z_POS:
    case AxisSigned::Value::X_NEG:
    case AxisSigned::Value::Y_NEG:
    case AxisSigned::Value::Z_NEG:
      return stream << axis.axis() << (axis.sign() == -1 ? '-' : '+');
  }
}
std::ostream &operator<<(std::ostream &stream, const CartesianBasis &rot)
{
  return stream << "CartesianBasis" << rot.axes;
}

}  // namespace blender::math
