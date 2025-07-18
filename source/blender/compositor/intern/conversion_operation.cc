/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_color.hh"
#include "BLI_cpp_type.hh"
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

/* Returns true if conversion between the given from and to types is supported. This should be
 * consistent and up to date with the compositor node tree's validate_link fallback. */
static bool is_conversion_supported(const ResultType from_type, const ResultType to_type)
{
  switch (from_type) {
    case ResultType::Float:
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Float4:
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Color:
    case ResultType::Bool:
      switch (to_type) {
        case ResultType::Float:
        case ResultType::Float2:
        case ResultType::Float3:
        case ResultType::Float4:
        case ResultType::Int:
        case ResultType::Int2:
        case ResultType::Color:
        case ResultType::Bool:
          return true;
        case ResultType::Menu:
          return false;
      }
      break;
    case ResultType::Menu:
      return to_type == ResultType::Menu;
  }

  BLI_assert_unreachable();
  return false;
}

void ConversionOperation::execute()
{
  Result &result = this->get_result();
  const Result &input = this->get_input();

  if (!is_conversion_supported(input.type(), result.type())) {
    result.allocate_invalid();
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
    GPUShader *shader = this->context().get_shader(shader_name.c_str());
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

/* Gets the single value of the given result as a single element GSpan. This calls the underlying
 * single_value method to construct a GSpan from the GPointer, however, it has an exception for
 * color types, since colors are stored as float4 internally, while their semantic type is
 * ColorSceneLinear4f<eAlpha::Premultiplied> during conversion. */
static GSpan get_result_single_value(const Result &result)
{
  if (result.type() == ResultType::Color) {
    return GSpan(
        CPPType::get<ColorSceneLinear4f<eAlpha::Premultiplied>>(), result.single_value().get(), 1);
  }

  return GSpan(result.single_value().type(), result.single_value().get(), 1);
}

static GMutableSpan get_result_single_value(Result &result)
{
  if (result.type() == ResultType::Color) {
    return GMutableSpan(
        CPPType::get<ColorSceneLinear4f<eAlpha::Premultiplied>>(), result.single_value().get(), 1);
  }

  return GMutableSpan(result.single_value().type(), result.single_value().get(), 1);
}

void ConversionOperation::execute_single(const Result &input, Result &output)
{
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  conversions.convert_to_initialized_n(get_result_single_value(input),
                                       get_result_single_value(output));
  output.update_single_value_data();
}

/* Gets the CPU data of the given result as a GSpan. This calls the underlying cpu_data method,
 * however, it has an exception for color types, since colors are stored as float4 internally,
 * while their semantic type is ColorSceneLinear4f<eAlpha::Premultiplied> during conversion. */
static GSpan get_result_data(const Result &result)
{
  if (result.type() == ResultType::Color) {
    return GSpan(CPPType::get<ColorSceneLinear4f<eAlpha::Premultiplied>>(),
                 result.cpu_data().data(),
                 result.cpu_data().size());
  }

  return result.cpu_data();
}

/* Same as get_result_data but takes non-const result and returns a GMutableSpan. */
static GMutableSpan get_result_data(Result &result)
{
  if (result.type() == ResultType::Color) {
    return GMutableSpan(CPPType::get<ColorSceneLinear4f<eAlpha::Premultiplied>>(),
                        result.cpu_data().data(),
                        result.cpu_data().size());
  }

  return result.cpu_data();
}

void ConversionOperation::execute_cpu(const Result &input, Result &output)
{
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  conversions.convert_to_initialized_n(get_result_data(input), get_result_data(output));
}

}  // namespace blender::compositor
