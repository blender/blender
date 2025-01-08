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

namespace blender::compositor {

/* Given a potentially transformed domain, compute a domain such that its rotation and scale become
 * identity and the size of the domain is increased/reduced to adapt to the new transformation. For
 * instance, if the domain is rotated, the returned domain will have zero rotation but expanded
 * size to account for the bounding box of the domain after rotation. The size of the returned
 * domain is bound and clipped by the maximum possible size to avoid allocations that surpass
 * hardware limits. */
static Domain compute_realized_transformation_domain(Context &context, const Domain &domain)
{
  const int2 size = domain.size;

  /* Compute the 4 corners of the domain. */
  const float2 lower_left_corner = float2(0.0f);
  const float2 lower_right_corner = float2(size.x, 0.0f);
  const float2 upper_left_corner = float2(0.0f, size.y);
  const float2 upper_right_corner = float2(size);

  /* Eliminate the translation component of the transformation and create a centered
   * transformation with the image center as the origin. Translation is ignored since it has no
   * effect on the size of the domain and will be restored later. */
  const float2 center = float2(float2(size) / 2.0f);
  const float3x3 transformation = float3x3(float2x2(domain.transformation));
  const float3x3 centered_transformation = math::from_origin_transform(transformation, center);

  /* Transform each of the 4 corners of the image by the centered transformation. */
  const float2 transformed_lower_left_corner = math::transform_point(centered_transformation,
                                                                     lower_left_corner);
  const float2 transformed_lower_right_corner = math::transform_point(centered_transformation,
                                                                      lower_right_corner);
  const float2 transformed_upper_left_corner = math::transform_point(centered_transformation,
                                                                     upper_left_corner);
  const float2 transformed_upper_right_corner = math::transform_point(centered_transformation,
                                                                      upper_right_corner);

  /* Compute the lower and upper bounds of the bounding box of the transformed corners. */
  const float2 lower_bound = math::min(
      math::min(transformed_lower_left_corner, transformed_lower_right_corner),
      math::min(transformed_upper_left_corner, transformed_upper_right_corner));
  const float2 upper_bound = math::max(
      math::max(transformed_lower_left_corner, transformed_lower_right_corner),
      math::max(transformed_upper_left_corner, transformed_upper_right_corner));

  /* Round the bounds such that they cover the entire transformed domain, which means flooring for
   * the lower bound and ceiling for the upper bound. */
  const int2 integer_lower_bound = int2(math::floor(lower_bound));
  const int2 integer_upper_bound = int2(math::ceil(upper_bound));

  const int2 new_size = integer_upper_bound - integer_lower_bound;

  /* Make sure the new size is safe by clamping to the hardware limits and an upper bound. */
  const int max_size = context.use_gpu() ? GPU_max_texture_size() : 65536;
  const int2 safe_size = math::clamp(new_size, int2(1), int2(max_size));

  /* Create a domain from the new safe size and just the translation component of the
   * transformation, */
  return Domain(safe_size, math::from_location<float3x3>(domain.transformation.location()));
}

void transform(Context &context,
               Result &input,
               Result &output,
               const float3x3 &transformation,
               RealizationOptions realization_options)
{
  Domain transformed_domain = input.domain();
  transformed_domain.transform(transformation);

  /* Realize the input on the target domain using the full transformation. */
  const Domain target_domain = compute_realized_transformation_domain(context, transformed_domain);
  realize_on_domain(context,
                    input,
                    output,
                    target_domain,
                    transformation * input.domain().transformation,
                    realization_options);

  output.get_realization_options().interpolation = realization_options.interpolation;
}

}  // namespace blender::compositor
