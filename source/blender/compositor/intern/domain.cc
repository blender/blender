/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_interp.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "COM_domain.hh"
#include "GPU_texture.hh"
#include <utility>

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

Domain Domain::transposed() const
{
  Domain domain = *this;
  domain.size = int2(this->size.y, this->size.x);
  return domain;
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

math::InterpWrapMode map_extension_mode_to_wrap_mode(const ExtensionMode &mode)
{
  switch (mode) {
    case ExtensionMode::Clip:
      return math::InterpWrapMode::Border;
    case ExtensionMode::Repeat:
      return math::InterpWrapMode::Repeat;
    case ExtensionMode::Extend:
      return math::InterpWrapMode::Extend;
  }
  BLI_assert_unreachable();
  return math::InterpWrapMode::Border;
}

GPUSamplerExtendMode map_extension_mode_to_extend_mode(const ExtensionMode &mode)
{
  switch (mode) {
    case blender::compositor::ExtensionMode::Clip:
      return GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;

    case blender::compositor::ExtensionMode::Extend:
      return GPU_SAMPLER_EXTEND_MODE_EXTEND;

    case blender::compositor::ExtensionMode::Repeat:
      return GPU_SAMPLER_EXTEND_MODE_REPEAT;
  }

  BLI_assert_unreachable();
  return GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
}

}  // namespace blender::compositor
