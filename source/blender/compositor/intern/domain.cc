/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "COM_domain.hh"

namespace blender::compositor {

Domain::Domain(const int2 &size) : size(size), transformation(float3x3::identity()) {}

Domain::Domain(const int2 &size, const float3x3 &transformation)
    : size(size), transformation(transformation)
{
}

void Domain::transform(const float3x3 &input_transformation)
{
  transformation = input_transformation * transformation;
}

Domain Domain::identity()
{
  return Domain(int2(1), float3x3::identity());
}

bool Domain::is_equal(const Domain &a, const Domain &b, const float epsilon)
{
  return a.size == b.size && math::is_equal(a.transformation, b.transformation, epsilon);
}

bool operator==(const Domain &a, const Domain &b)
{
  return a.size == b.size && a.transformation == b.transformation;
}

bool operator!=(const Domain &a, const Domain &b)
{
  return !(a == b);
}

}  // namespace blender::compositor
