/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_generic_span.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "GPU_shader.hh"

#include "IMB_colormanagement.hh"

#include "BKE_type_conversions.hh"

#include "COM_context.hh"
#include "COM_conversion_operation.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

ConversionOperation::ConversionOperation(Context &context,
                                         const ResultType input_type,
                                         const ResultType expected_type)
    : SimpleOperation(context)
{
  this->declare_input_descriptor(InputDescriptor{input_type});
  this->populate_result(context.create_result(expected_type));
}

void ConversionOperation::execute()
{
  Result &result = this->get_result();
  const Result &input = this->get_input();

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  if (!conversions.is_convertible(input.get_cpp_type(), result.get_cpp_type())) {
    this->allocate_default_remaining_outputs();
    return;
  }

  if (input.is_single_value()) {
    result.allocate_single_value();
    this->execute_single(input, result);
    return;
  }

  result.allocate_texture(input.domain());
  if (this->context().use_gpu()) {
    const std::string shader_name = fmt::format("compositor_convert_{}_to_{}",
                                                Result::type_name(this->get_input().type()),
                                                Result::type_name(this->get_result().type()));
    gpu::Shader *shader = this->context().get_shader(shader_name.c_str());
    GPU_shader_bind(shader);

    if (this->get_input().type() == ResultType::Color &&
        ELEM(this->get_result().type(), ResultType::Float, ResultType::Int, ResultType::Bool))
    {
      float luminance_coefficients[3];
      IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
      GPU_shader_uniform_3fv(shader, "luminance_coefficients_u", luminance_coefficients);
    }

    input.bind_as_texture(shader, "input_tx");
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().data_size);

    input.unbind_as_texture();
    result.unbind_as_image();
    GPU_shader_unbind();
  }
  else {
    this->execute_cpu(input, result);
  }
}

SimpleOperation *ConversionOperation::construct_if_needed(Context &context,
                                                          const Result &input_result,
                                                          const InputDescriptor &input_descriptor)
{
  if (input_descriptor.skip_type_conversion) {
    return nullptr;
  }

  const ResultType result_type = input_result.type();
  const ResultType expected_type = input_descriptor.type;
  if (result_type != expected_type) {
    return new ConversionOperation(context, result_type, expected_type);
  }
  return nullptr;
}

void ConversionOperation::execute_single(const Result &input, Result &output)
{
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  conversions.convert_to_initialized_n(
      GSpan(input.single_value().type(), input.single_value().get(), 1),
      GMutableSpan(output.single_value().type(), output.single_value().get(), 1));
  output.update_single_value_data();
}

void ConversionOperation::execute_cpu(const Result &input, Result &output)
{
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  conversions.convert_to_initialized_n(input.cpu_data(), output.cpu_data_for_write());
}

}  // namespace blender::compositor
