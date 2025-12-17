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

Domain::Domain(const int2 &size)
    : data_size(size),
      display_size(size),
      data_offset(int2(0)),
      transformation(float3x3::identity())
{
}

Domain::Domain(const int2 &size, const float3x3 &transformation)
    : data_size(size), display_size(size), data_offset(int2(0)), transformation(transformation)
{
}

void Domain::transform(const float3x3 &input_transformation)
{
  transformation = input_transformation * transformation;
}

Domain Domain::transposed() const
{
  Domain domain = *this;
  domain.data_size = int2(this->data_size.y, this->data_size.x);
  domain.display_size = int2(this->display_size.y, this->display_size.x);
  domain.data_offset = int2(this->data_offset.y, this->data_offset.x);
  return domain;
}

Domain Domain::identity()
{
  return Domain(int2(1), float3x3::identity());
}

bool Domain::is_equal(const Domain &a, const Domain &b, const float epsilon)
{
  return a.data_size == b.data_size && math::is_equal(a.transformation, b.transformation, epsilon);
}

bool operator==(const Domain &a, const Domain &b)
{
  return a.data_size == b.data_size && a.transformation == b.transformation;
}

bool operator!=(const Domain &a, const Domain &b)
{
  return !(a == b);
}

math::InterpWrapMode map_extension_mode_to_wrap_mode(const Extension &mode)
{
  switch (mode) {
    case Extension::Clip:
      return math::InterpWrapMode::Border;
    case Extension::Repeat:
      return math::InterpWrapMode::Repeat;
    case Extension::Extend:
      return math::InterpWrapMode::Extend;
  }
  BLI_assert_unreachable();
  return math::InterpWrapMode::Border;
}

GPUSamplerExtendMode map_extension_mode_to_extend_mode(const Extension &mode)
{
  switch (mode) {
    case blender::compositor::Extension::Clip:
      return GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;

    case blender::compositor::Extension::Extend:
      return GPU_SAMPLER_EXTEND_MODE_EXTEND;

    case blender::compositor::Extension::Repeat:
      return GPU_SAMPLER_EXTEND_MODE_REPEAT;
  }

  BLI_assert_unreachable();
  return GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
}

}  // namespace blender::compositor
