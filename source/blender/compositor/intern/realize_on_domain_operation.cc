/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "GPU_capabilities.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_realize_on_domain_operation.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Realize On Domain Operation
 */

RealizeOnDomainOperation::RealizeOnDomainOperation(Context &context,
                                                   Domain target_domain,
                                                   ResultType type)
    : SimpleOperation(context), target_domain_(target_domain)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = type;
  this->declare_input_descriptor(input_descriptor);
  this->populate_result(context.create_result(type));
}

void RealizeOnDomainOperation::execute()
{
  /* Translate the input such that it is centered in the virtual compositing space. Adding any
   * corrective translation if necessary. */
  const float2 input_center_translation = float2(-float2(this->get_input().domain().size) / 2.0f);
  const float3x3 input_transformation = math::translate(
      this->get_input().domain().transformation,
      input_center_translation + this->compute_corrective_translation());

  /* Translate the output such that it is centered in the virtual compositing space. */
  const float2 output_center_translation = -float2(this->compute_domain().size) / 2.0f;
  const float3x3 output_transformation = math::translate(this->compute_domain().transformation,
                                                         output_center_translation);

  /* Get the transformation from the output space to the input space */
  const float3x3 inverse_transformation = math::invert(input_transformation) *
                                          output_transformation;

  if (this->context().use_gpu()) {
    this->realize_on_domain_gpu(inverse_transformation);
  }
  else {
    this->realize_on_domain_cpu(inverse_transformation);
  }
}

float2 RealizeOnDomainOperation::compute_corrective_translation()
{
  if (this->get_input().get_realization_options().interpolation == Interpolation::Nearest) {
    /* Bias translations in case of nearest interpolation to avoids the round-to-even behavior of
     * some GPUs at pixel boundaries. */
    return float2(std::numeric_limits<float>::epsilon() * 10e3f);
  }

  /* Assuming no transformations, if the input size is odd and output size is even or vice versa,
   * the centers of pixels of the input and output will be half a pixel away from each other due
   * to the centering translation. Which introduce fuzzy result due to interpolation. So if one
   * is odd and the other is even, detected by testing the low bit of the xor of the sizes, shift
   * the input by 1/2 pixel so the pixels align. */
  const int2 output_size = this->compute_domain().size;
  const int2 input_size = this->get_input().domain().size;
  return float2(((input_size[0] ^ output_size[0]) & 1) ? -0.5f : 0.0f,
                ((input_size[1] ^ output_size[1]) & 1) ? -0.5f : 0.0f);
}

void RealizeOnDomainOperation::realize_on_domain_gpu(const float3x3 &inverse_transformation)
{
  gpu::Shader *shader = this->context().get_shader(this->get_realization_shader_name());
  GPU_shader_bind(shader);

  GPU_shader_uniform_mat3_as_mat4(shader, "inverse_transformation", inverse_transformation.ptr());

  Result &input = this->get_input();
  const RealizationOptions realization_options = input.get_realization_options();

  if (!GPU_texture_has_integer_format(input)) {
    /* The texture sampler should use bilinear interpolation for both the bilinear and bicubic
     * cases, as the logic used by the bicubic realization shader expects textures to use bilinear
     * interpolation. */
    const bool use_bilinear = ELEM(
        realization_options.interpolation, Interpolation::Bilinear, Interpolation::Bicubic);
    GPU_texture_filter_mode(input, use_bilinear);
    GPU_texture_anisotropic_filter(input, false);
  }

  GPU_texture_extend_mode_x(input,
                            map_extension_mode_to_extend_mode(realization_options.extension_x));
  GPU_texture_extend_mode_y(input,
                            map_extension_mode_to_extend_mode(realization_options.extension_y));

  input.bind_as_texture(shader, "input_tx");

  const Domain domain = this->compute_domain();
  Result &output = this->get_result();
  output.allocate_texture(domain);
  output.bind_as_image(shader, "domain_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  input.unbind_as_texture();
  output.unbind_as_image();
  GPU_shader_unbind();
}

const char *RealizeOnDomainOperation::get_realization_shader_name()
{
  if (this->get_input().get_realization_options().interpolation == Interpolation::Bicubic) {
    switch (this->get_input().type()) {
      case ResultType::Float:
        return "compositor_realize_on_domain_bicubic_float";
      case ResultType::Float2:
        return "compositor_realize_on_domain_bicubic_float2";
      case ResultType::Float3:
        /* Float3 is internally stored in a float4 texture. */
        return "compositor_realize_on_domain_bicubic_float4";
      case ResultType::Float4:
        return "compositor_realize_on_domain_bicubic_float4";
      case ResultType::Color:
        return "compositor_realize_on_domain_bicubic_float4";
      case ResultType::Int:
        return "compositor_realize_on_domain_int";
      case ResultType::Int2:
        return "compositor_realize_on_domain_int2";
      case ResultType::Bool:
        return "compositor_realize_on_domain_bool";
      case ResultType::Menu:
        return "compositor_realize_on_domain_menu";
      case ResultType::String:
        /* Single only types do not support GPU code path. */
        BLI_assert(Result::is_single_value_only_type(this->get_input().type()));
        BLI_assert_unreachable();
        break;
    }
  }
  else {
    switch (this->get_input().type()) {
      case ResultType::Float:
        return "compositor_realize_on_domain_float";
      case ResultType::Float2:
        return "compositor_realize_on_domain_float2";
      case ResultType::Float3:
        /* Float3 is internally stored in a float4 texture. */
        return "compositor_realize_on_domain_float4";
      case ResultType::Float4:
        return "compositor_realize_on_domain_float4";
      case ResultType::Color:
        return "compositor_realize_on_domain_float4";
      case ResultType::Int:
        return "compositor_realize_on_domain_int";
      case ResultType::Int2:
        return "compositor_realize_on_domain_int2";
      case ResultType::Bool:
        return "compositor_realize_on_domain_bool";
      case ResultType::Menu:
        return "compositor_realize_on_domain_menu";
      case ResultType::String:
        /* Single only types do not support GPU code path. */
        BLI_assert(Result::is_single_value_only_type(this->get_input().type()));
        BLI_assert_unreachable();
        break;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

template<typename T>
static void realize_on_domain(const Result &input,
                              Result &output,
                              const float3x3 &inverse_transformation)
{
  const RealizationOptions realization_options = input.get_realization_options();
  const int2 input_size = input.domain().size;
  const int2 output_size = output.domain().size;
  parallel_for(output_size, [&](const int2 texel) {
    const float2 texel_coordinates = float2(texel) + float2(0.5f);

    /* Transform the input image by transforming the domain coordinates with the inverse of input
     * image's transformation. The inverse transformation is an affine matrix and thus the
     * coordinates should be in homogeneous coordinates. */
    const float2 transformed_coordinates =
        (inverse_transformation * float3(texel_coordinates, 1.0f)).xy();

    const float2 normalized_coordinates = transformed_coordinates / float2(input_size);
    T sample = input.sample<T>(normalized_coordinates,
                               realization_options.interpolation,
                               realization_options.extension_x,
                               realization_options.extension_y);
    output.store_pixel(texel, sample);
  });
}

void RealizeOnDomainOperation::realize_on_domain_cpu(const float3x3 &inverse_transformation)
{
  Result &input = this->get_input();
  Result &output = this->get_result();

  const Domain domain = this->compute_domain();
  output.allocate_texture(domain);

  input.get_cpp_type()
      .to_static_type_tag<float,
                          float2,
                          float3,
                          float4,
                          Color,
                          int32_t,
                          int2,
                          bool,
                          nodes::MenuValue>([&](auto type_tag) {
        using T = typename decltype(type_tag)::type;
        if constexpr (std::is_same_v<T, void>) {
          /* Unsupported type. */
          BLI_assert_unreachable();
        }
        else {
          realize_on_domain<T>(input, output, inverse_transformation);
        }
      });
}

Domain RealizeOnDomainOperation::compute_domain()
{
  return target_domain_;
}

/* If the transformations of the input and output domains are within this tolerance value, then
 * realization shouldn't be needed. */
static constexpr float transformation_tolerance = 10e-6f;

Domain RealizeOnDomainOperation::compute_realized_transformation_domain(
    Context &context, const Domain &domain, const bool realize_translation)
{
  const int2 size = domain.size;

  /* If the domain is only infinitesimally rotated or scaled, return a domain with just the
   * translation component if not realizing translation. */
  if (math::is_equal(
          float2x2(domain.transformation), float2x2::identity(), transformation_tolerance))
  {
    if (realize_translation) {
      return Domain(size);
    }
    return Domain(size, math::from_location<float3x3>(domain.transformation.location()));
  }

  /* Compute the 4 corners of the domain. */
  const float2 lower_left_corner = float2(0.0f);
  const float2 lower_right_corner = float2(size.x, 0.0f);
  const float2 upper_left_corner = float2(0.0f, size.y);
  const float2 upper_right_corner = float2(size);

  /* Eliminate the translation component of the transformation. Translation is ignored since it has
   * no effect on the size of the domain and will be restored later. */
  const float3x3 transformation = float3x3(float2x2(domain.transformation));

  /* Translate the input such that it is centered in the virtual compositing space. */
  const float2 center_translation = -float2(size) / 2.0f;
  const float3x3 centered_transformation = math::translate(transformation, center_translation);

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
   * transformation if not realizing translation. */
  if (realize_translation) {
    return Domain(safe_size);
  }
  return Domain(safe_size, math::from_location<float3x3>(domain.transformation.location()));
}

SimpleOperation *RealizeOnDomainOperation::construct_if_needed(
    Context &context,
    const Result &input_result,
    const InputDescriptor &input_descriptor,
    const Domain &operation_domain)
{
  /* This input doesn't need realization, the operation is not needed. */
  if (input_descriptor.realization_mode == InputRealizationMode::None) {
    return nullptr;
  }

  /* The input expects a single value and if no single value is provided, it will be ignored and a
   * default value will be used, so no need to realize it and the operation is not needed. */
  if (input_descriptor.expects_single_value) {
    return nullptr;
  }

  /* Input result is a single value and does not need realization, the operation is not needed. */
  if (input_result.is_single_value()) {
    return nullptr;
  }

  /* If we are realizing on the operation domain, then our target domain is the operation domain,
   * otherwise, we are only realizing the transforms, then our target domain is the input's one. */
  const bool use_operation_domain = input_descriptor.realization_mode ==
                                    InputRealizationMode::OperationDomain;
  const Domain target_domain = use_operation_domain ? operation_domain : input_result.domain();

  const bool should_realize_translation = input_descriptor.realization_mode ==
                                          InputRealizationMode::Transforms;
  const Domain realized_target_domain =
      RealizeOnDomainOperation::compute_realized_transformation_domain(
          context, target_domain, should_realize_translation);

  /* The input have an almost identical domain to the realized target domain, so no need to realize
   * it and the operation is not needed. */
  if (Domain::is_equal(input_result.domain(), realized_target_domain, transformation_tolerance)) {
    return nullptr;
  }

  /* Otherwise, realization is needed. */
  return new RealizeOnDomainOperation(context, realized_target_domain, input_descriptor.type);
}

}  // namespace blender::compositor
