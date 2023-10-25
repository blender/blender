/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_capabilities.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_algorithm_realize_on_domain.hh"
#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_result.hh"

#include "COM_algorithm_transform.hh"

namespace blender::realtime_compositor {

/* Given a potentially transformed domain, compute a domain such that its rotation and scale become
 * identity and the size of the domain is increased/reduced to adapt to the new transformation. For
 * instance, if the domain is rotated, the returned domain will have zero rotation but expanded
 * size to account for the bounding box of the domain after rotation. The size of the returned
 * domain is bound and clipped by the maximum possible GPU texture size to avoid allocations that
 * surpass hardware limits, which is typically 16k. */
static Domain compute_realized_transformation_domain(const Domain &domain)
{
  math::AngleRadian rotation;
  float2 translation, scale;
  float2 size = float2(domain.size);
  math::to_loc_rot_scale(domain.transformation, translation, rotation, scale);

  /* Set the rotation to zero and expand the domain size to fit the bounding box of the rotated
   * result. */
  const float sine = math::abs(math::sin(rotation));
  const float cosine = math::abs(math::cos(rotation));
  size = float2(size.x * cosine + size.y * sine, size.x * sine + size.y * cosine);
  rotation = 0.0f;

  /* Set the scale to 1 and scale the domain size to adapt to the new domain. */
  size *= scale;
  scale = float2(1.0f);

  const float3x3 transformation = math::from_loc_rot_scale<float3x3>(translation, rotation, scale);

  const int2 domain_size = math::clamp(
      int2(math::round(size)), int2(1), int2(GPU_max_texture_size()));

  return Domain(domain_size, transformation);
}

void transform(Context &context,
               Result &input,
               Result &output,
               float3x3 transformation,
               Interpolation interpolation)
{
  math::AngleRadian rotation;
  float2 translation, scale;
  math::to_loc_rot_scale(transformation, translation, rotation, scale);

  /* Rotation and scale transformations are immediately realized. */
  if (rotation != 0.0f || scale != float2(1.0f)) {
    RealizationOptions realization_options = input.get_realization_options();
    realization_options.interpolation = interpolation;

    Domain input_domain = input.domain();
    input_domain.transform(transformation);

    const Domain target_domain = compute_realized_transformation_domain(input_domain);

    realize_on_domain(
        context, input, output, target_domain, input_domain.transformation, realization_options);
  }
  else {
    input.pass_through(output);
    const float3x3 translation_matrix = math::from_location<float3x3>(translation);
    output.transform(translation_matrix);
  }

  output.get_realization_options().interpolation = interpolation;
}

}  // namespace blender::realtime_compositor
