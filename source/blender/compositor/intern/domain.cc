/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.hh"
#include "BLI_bounds.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "BLT_translation.hh"

#include "GPU_texture.hh"

#include "COM_domain.hh"

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
  return a.data_size == b.data_size && a.display_size == b.display_size &&
         a.data_offset == b.data_offset &&
         math::is_equal(a.transformation, b.transformation, epsilon);
}

Domain Domain::realize_transformation(const bool realize_translation) const
{
  /* If the domain is only infinitesimally rotated or scaled, only realize the translation if
   * needed, otherwise, return as is. */
  const float3x3 translation = math::from_location<float3x3>(this->transformation.location());
  if (math::is_equal(float2x2(this->transformation), float2x2::identity(), 10e-6f)) {
    Domain realized_domain = *this;
    realized_domain.transformation = realize_translation ? float3x3::identity() : translation;
    return realized_domain;
  }

  /* Eliminate the translation component of the transformation. Translation is ignored since it has
   * no effect on the size of the domain and will be restored later if needed. */
  const float3x3 transformation = float3x3(float2x2(this->transformation));

  /* Translate the input such that it is centered in the virtual compositing space. */
  const float2 center_translation = -float2(this->display_size) / 2.0f;
  const float3x3 centered_transformation = math::translate(transformation, center_translation);

  /* Compute display window after transformation. */
  const Bounds<float2> display_window = {float2(0.0f), float2(this->display_size)};
  const Bounds<float2> new_display_window = bounds::transform_bounds(centered_transformation,
                                                                     display_window);
  const Bounds<int2> new_integer_display_window = {int2(math::floor(new_display_window.min)),
                                                   int2(math::ceil(new_display_window.max))};

  /* Compute data window after transformation. */
  const Bounds<float2> data_window = {float2(this->data_offset),
                                      float2(this->data_offset + this->data_size)};
  const Bounds<float2> new_data_window = bounds::transform_bounds(centered_transformation,
                                                                  data_window);
  const Bounds<int2> new_integer_data_window = {int2(math::floor(new_data_window.min)),
                                                int2(math::ceil(new_data_window.max))};

  Domain realized_domain = *this;
  realized_domain.display_size = math::max(int2(1), new_integer_display_window.size());
  realized_domain.data_size = math::max(int2(1), new_integer_data_window.size());
  realized_domain.data_offset = new_integer_data_window.min - new_integer_display_window.min;
  realized_domain.transformation = realize_translation ? float3x3::identity() : translation;
  return realized_domain;
}

bool operator==(const Domain &a, const Domain &b)
{
  return a.data_size == b.data_size && a.display_size == b.display_size &&
         a.data_offset == b.data_offset && a.transformation == b.transformation;
}

bool operator!=(const Domain &a, const Domain &b)
{
  return !(a == b);
}

StringRefNull to_string(const Interpolation &interpolation)
{
  switch (interpolation) {
    case Interpolation::Nearest:
      return N_("Nearest");
    case Interpolation::Bilinear:
      return N_("Bilinear");
    case Interpolation::Bicubic:
      return N_("Bicubic");
    case Interpolation::Anisotropic:
      return N_("Anisotropic");
  }

  BLI_assert_unreachable();
  return "None";
}

StringRefNull to_string(const Extension &extension)
{
  switch (extension) {
    case Extension::Extend:
      return N_("Extend");
    case Extension::Repeat:
      return N_("Repeat");
    case Extension::Clip:
      return N_("Clip");
  }

  BLI_assert_unreachable();
  return "None";
}

GPUSamplerExtendMode map_extension_mode_to_extend_mode(const Extension &mode)
{
  switch (mode) {
    case compositor::Extension::Clip:
      return GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;

    case compositor::Extension::Extend:
      return GPU_SAMPLER_EXTEND_MODE_EXTEND;

    case compositor::Extension::Repeat:
      return GPU_SAMPLER_EXTEND_MODE_REPEAT;
  }

  BLI_assert_unreachable();
  return GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
}

}  // namespace blender::compositor
