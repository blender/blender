/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_capabilities.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"

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
               RealizationOptions realization_options)
{
  /* If we are wrapping, the input is translated but the target domain remains fixed, which results
   * in the input clipping on one side and wrapping on the opposite side. This mask vector can be
   * multiplied to the translation component of the transformation to remove it. */
  const float2 wrap_mask = float2(realization_options.wrap_x ? 0.0f : 1.0f,
                                  realization_options.wrap_y ? 0.0f : 1.0f);

  /* Compute a transformed input domain, excluding translations of wrapped axes. */
  Domain input_domain = input.domain();
  float3x3 domain_transformation = transformation;
  domain_transformation.location() *= wrap_mask;
  input_domain.transform(domain_transformation);

  /* Realize the input on the target domain using the full transformation. */
  const Domain target_domain = compute_realized_transformation_domain(input_domain);
  realize_on_domain(context,
                    input,
                    output,
                    target_domain,
                    transformation * input.domain().transformation,
                    realization_options);

  output.get_realization_options().interpolation = realization_options.interpolation;
}

}  // namespace blender::realtime_compositor
