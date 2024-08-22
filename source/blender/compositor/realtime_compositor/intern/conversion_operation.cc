/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_conversion_operation.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Conversion Operation
 */

void ConversionOperation::execute()
{
  Result &result = get_result();
  const Result &input = get_input();

  if (input.is_single_value()) {
    result.allocate_single_value();
    this->execute_single(input, result);
    return;
  }

  result.allocate_texture(input.domain());
  if (context().use_gpu()) {
    GPUShader *shader = get_conversion_shader();
    GPU_shader_bind(shader);

    input.bind_as_texture(shader, "input_tx");
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

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
  ResultType result_type = input_result.type();
  ResultType expected_type = input_descriptor.type;

  /* If the result type differs from the expected type, return an instance of an appropriate
   * conversion operation. Otherwise, return a null pointer. */

  if (result_type == ResultType::Float && expected_type == ResultType::Vector) {
    return new ConvertFloatToVectorOperation(context);
  }

  if (result_type == ResultType::Float && expected_type == ResultType::Color) {
    return new ConvertFloatToColorOperation(context);
  }

  if (result_type == ResultType::Color && expected_type == ResultType::Float) {
    return new ConvertColorToFloatOperation(context);
  }

  if (result_type == ResultType::Color && expected_type == ResultType::Vector) {
    return new ConvertColorToVectorOperation(context);
  }

  if (result_type == ResultType::Vector && expected_type == ResultType::Float) {
    return new ConvertVectorToFloatOperation(context);
  }

  if (result_type == ResultType::Vector && expected_type == ResultType::Color) {
    return new ConvertVectorToColorOperation(context);
  }

  return nullptr;
}

/* --------------------------------------------------------------------
 * Convert Float to Vector Operation
 */

ConvertFloatToVectorOperation::ConvertFloatToVectorOperation(Context &context)
    : ConversionOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = ResultType::Float;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(ResultType::Vector));
}

void ConvertFloatToVectorOperation::execute_single(const Result &input, Result &output)
{
  output.set_vector_value(float4(float3(input.get_float_value()), 1.0f));
}

void ConvertFloatToVectorOperation::execute_cpu(const Result &input, Result &output)
{
  parallel_for(input.domain().size, [&](const int2 texel) {
    output.store_pixel(texel, float4(float3(input.load_pixel(texel).x), 1.0f));
  });
}

GPUShader *ConvertFloatToVectorOperation::get_conversion_shader() const
{
  return context().get_shader("compositor_convert_float_to_vector");
}

/* --------------------------------------------------------------------
 * Convert Float to Color Operation
 */

ConvertFloatToColorOperation::ConvertFloatToColorOperation(Context &context)
    : ConversionOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = ResultType::Float;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(ResultType::Color));
}

void ConvertFloatToColorOperation::execute_single(const Result &input, Result &output)
{
  output.set_color_value(float4(float3(input.get_float_value()), 1.0f));
}

void ConvertFloatToColorOperation::execute_cpu(const Result &input, Result &output)
{
  parallel_for(input.domain().size, [&](const int2 texel) {
    output.store_pixel(texel, float4(float3(input.load_pixel(texel).x), 1.0f));
  });
}

GPUShader *ConvertFloatToColorOperation::get_conversion_shader() const
{
  return context().get_shader("compositor_convert_float_to_color");
}

/* --------------------------------------------------------------------
 * Convert Color to Float Operation
 */

ConvertColorToFloatOperation::ConvertColorToFloatOperation(Context &context)
    : ConversionOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = ResultType::Color;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(ResultType::Float));
}

void ConvertColorToFloatOperation::execute_single(const Result &input, Result &output)
{
  float4 color = input.get_color_value();
  output.set_float_value((color.x + color.y + color.z) / 3.0f);
}

void ConvertColorToFloatOperation::execute_cpu(const Result &input, Result &output)
{
  parallel_for(input.domain().size, [&](const int2 texel) {
    const float4 color = input.load_pixel(texel);
    output.store_pixel(texel, float4((color.x + color.y + color.z) / 3.0f));
  });
}

GPUShader *ConvertColorToFloatOperation::get_conversion_shader() const
{
  return context().get_shader("compositor_convert_color_to_float");
}

/* --------------------------------------------------------------------
 * Convert Color to Vector Operation
 */

ConvertColorToVectorOperation::ConvertColorToVectorOperation(Context &context)
    : ConversionOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = ResultType::Color;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(ResultType::Vector));
}

void ConvertColorToVectorOperation::execute_single(const Result &input, Result &output)
{
  float4 color = input.get_color_value();
  output.set_vector_value(color);
}

void ConvertColorToVectorOperation::execute_cpu(const Result &input, Result &output)
{
  parallel_for(input.domain().size,
               [&](const int2 texel) { output.store_pixel(texel, input.load_pixel(texel)); });
}

GPUShader *ConvertColorToVectorOperation::get_conversion_shader() const
{
  return context().get_shader("compositor_convert_color_to_vector");
}

/* --------------------------------------------------------------------
 * Convert Vector to Float Operation
 */

ConvertVectorToFloatOperation::ConvertVectorToFloatOperation(Context &context)
    : ConversionOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = ResultType::Vector;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(ResultType::Float));
}

void ConvertVectorToFloatOperation::execute_single(const Result &input, Result &output)
{
  float4 vector = input.get_vector_value();
  output.set_float_value((vector[0] + vector[1] + vector[2]) / 3.0f);
}

void ConvertVectorToFloatOperation::execute_cpu(const Result &input, Result &output)
{
  parallel_for(input.domain().size, [&](const int2 texel) {
    const float4 vector = input.load_pixel(texel);
    output.store_pixel(texel, float4((vector.x + vector.y + vector.z) / 3.0f));
  });
}

GPUShader *ConvertVectorToFloatOperation::get_conversion_shader() const
{
  return context().get_shader("compositor_convert_vector_to_float");
}

/* --------------------------------------------------------------------
 * Convert Vector to Color Operation
 */

ConvertVectorToColorOperation::ConvertVectorToColorOperation(Context &context)
    : ConversionOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = ResultType::Vector;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(ResultType::Color));
}

void ConvertVectorToColorOperation::execute_single(const Result &input, Result &output)
{
  output.set_color_value(float4(float3(input.get_vector_value()), 1.0f));
}

void ConvertVectorToColorOperation::execute_cpu(const Result &input, Result &output)
{
  parallel_for(input.domain().size, [&](const int2 texel) {
    output.store_pixel(texel, float4(input.load_pixel(texel).xyz(), 1.0f));
  });
}

GPUShader *ConvertVectorToColorOperation::get_conversion_shader() const
{
  return context().get_shader("compositor_convert_vector_to_color");
}

}  // namespace blender::realtime_compositor
