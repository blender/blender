/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

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
  const Domain input_domain = this->get_input().domain();
  const Domain output_domain = target_domain_;

  /* Create a transformation matrix that transforms the pixels in the data window from the data
   * space to the virtual compositing space. This is done by first adding the data offset to go
   * from the data space to the display space, then subtracting the center of the display window to
   * go from the display space to the virtual compositing space. See the corrective translation
   * function for more information on its function. */
  const float2 input_center = float2(input_domain.display_size) / 2.0f;
  const float2 input_translation = float2(input_domain.data_offset) - input_center +
                                   this->compute_corrective_translation();
  const float3x3 input_data_to_virtual = math::translate(input_domain.transformation,
                                                         input_translation);

  /* Same as above but for the output domain. */
  const float2 output_center = float2(output_domain.display_size) / 2.0f;
  const float2 output_translation = float2(output_domain.data_offset) - output_center;
  const float3x3 output_data_to_virtual = math::translate(output_domain.transformation,
                                                          output_translation);

  /* Create a transformation matrix from the output data space to the input data space */
  const float3x3 virtual_to_input_data = math::invert(input_data_to_virtual);
  const float3x3 output_data_to_input_data = virtual_to_input_data * output_data_to_virtual;

  /* Create a transformation matrix from the output integer texel to the input normalized sampler
   * coordinates. This is done by adding 0.5 to evaluate the output at the center if pixels and
   * dividing by the input size to get normalized coordinates. */
  const float3x3 output_texel_to_output_data = math::from_location<float3x3>(float2(0.5f));
  const float3x3 input_data_to_input_sampler = math::from_scale<float3x3, 2>(
      1.0f / float2(input_domain.data_size));
  const float3x3 output_texel_to_input_sampler = input_data_to_input_sampler *
                                                 output_data_to_input_data *
                                                 output_texel_to_output_data;

  if (this->context().use_gpu()) {
    this->realize_on_domain_gpu(output_texel_to_input_sampler);
  }
  else {
    this->realize_on_domain_cpu(output_texel_to_input_sampler);
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
  const int2 output_size = this->compute_domain().data_size;
  const int2 input_size = this->get_input().domain().data_size;
  return float2(((input_size[0] ^ output_size[0]) & 1) ? -0.5f : 0.0f,
                ((input_size[1] ^ output_size[1]) & 1) ? -0.5f : 0.0f);
}

void RealizeOnDomainOperation::realize_on_domain_gpu(const float3x3 &transformation)
{
  gpu::Shader *shader = this->context().get_shader(this->get_realization_shader_name());
  GPU_shader_bind(shader);

  GPU_shader_uniform_mat3_as_mat4(shader, "transformation", transformation.ptr());

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

  compute_dispatch_threads_at_least(shader, output.domain().data_size);

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
        /* Float3 is internally stored in a float4 texture due to GPU module limitations. */
        return "compositor_realize_on_domain_bicubic_float4";
      case ResultType::Float4:
        return "compositor_realize_on_domain_bicubic_float4";
      case ResultType::Color:
        return "compositor_realize_on_domain_bicubic_float4";
      case ResultType::Int:
        return "compositor_realize_on_domain_int";
      case ResultType::Int2:
        return "compositor_realize_on_domain_int2";
      case ResultType::Int3:
        /* Int3 is internally stored in a int4 texture due to GPU module limitations. */
        return "compositor_realize_on_domain_int4";
      case ResultType::Bool:
        return "compositor_realize_on_domain_bool";
      case ResultType::Float4x4:
        return "compositor_realize_on_domain_float4x4";
      case ResultType::Menu:
        return "compositor_realize_on_domain_menu";
      case ResultType::String:
      case ResultType::Object:
      case ResultType::Image:
      case ResultType::Font:
      case ResultType::Scene:
      case ResultType::Text:
      case ResultType::Mask:
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
        /* Float3 is internally stored in a float4 texture due to GPU module limitations. */
        return "compositor_realize_on_domain_float4";
      case ResultType::Float4:
        return "compositor_realize_on_domain_float4";
      case ResultType::Color:
        return "compositor_realize_on_domain_float4";
      case ResultType::Int:
        return "compositor_realize_on_domain_int";
      case ResultType::Int2:
        return "compositor_realize_on_domain_int2";
      case ResultType::Int3:
        /* Int3 is internally stored in a int4 texture due to GPU module limitations. */
        return "compositor_realize_on_domain_int4";
      case ResultType::Bool:
        return "compositor_realize_on_domain_bool";
      case ResultType::Float4x4:
        return "compositor_realize_on_domain_float4x4";
      case ResultType::Menu:
        return "compositor_realize_on_domain_menu";
      case ResultType::String:
      case ResultType::Object:
      case ResultType::Image:
      case ResultType::Font:
      case ResultType::Scene:
      case ResultType::Text:
      case ResultType::Mask:
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
static void realize_on_domain(const Result &input, Result &output, const float3x3 &transformation)
{
  const RealizationOptions realization_options = input.get_realization_options();
  parallel_for(output.domain().data_size, [&](const int2 texel) {
    const float2 coordinates = math::transform_point(transformation, float2(texel));
    T sample = input.sample<T>(coordinates,
                               realization_options.interpolation,
                               realization_options.extension_x,
                               realization_options.extension_y);
    output.store_pixel(texel, sample);
  });
}

void RealizeOnDomainOperation::realize_on_domain_cpu(const float3x3 &transformation)
{
  Result &input = this->get_input();
  Result &output = this->get_result();

  const Domain domain = this->compute_domain();
  output.allocate_texture(domain);

  input.get_cpp_type()
      .to_static_type<float,
                      float2,
                      float3,
                      float4,
                      Color,
                      int32_t,
                      int2,
                      int3,
                      bool,
                      float4x4,
                      nodes::MenuValue>(
          [&]<typename T>() { realize_on_domain<T>(input, output, transformation); });
}

Domain RealizeOnDomainOperation::compute_domain()
{
  return target_domain_;
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
  const Domain realized_target_domain = target_domain.realize_transformation(
      should_realize_translation);

  /* The input have an almost identical domain to the realized target domain, so no need to realize
   * it and the operation is not needed. */
  if (Domain::is_equal(input_result.domain(), realized_target_domain)) {
    return nullptr;
  }

  return new RealizeOnDomainOperation(context, realized_target_domain, input_descriptor.type);
}

}  // namespace blender::compositor
