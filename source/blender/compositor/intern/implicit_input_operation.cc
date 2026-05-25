/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "BLI_assert.h"

#include "COM_domain.hh"
#include "COM_implicit_input_operation.hh"
#include "COM_input_descriptor.hh"
#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::compositor {

const StringRef ImplicitInputOperation::output_identifier_ = StringRef("Output");

static ResultType get_implicit_input_result_type(const ImplicitInputType implicit_input)
{
  switch (implicit_input) {
    case ImplicitInputType::UniformImageCoordinates:
      return ResultType::Float2;
    case ImplicitInputType::SceneFrame:
      return ResultType::Int;
  }

  BLI_assert_unreachable();
  return ResultType::Float2;
}

ImplicitInputOperation::ImplicitInputOperation(Context &context,
                                               const ImplicitInputType implicit_input)
    : Operation(context), implicit_input_(implicit_input)
{
  this->populate_result(output_identifier_, get_implicit_input_result_type(implicit_input));
}

void ImplicitInputOperation::execute()
{
  Result &result = this->get_result();

  switch (implicit_input_) {
    case ImplicitInputType::UniformImageCoordinates: {
      const int2 size = this->context().get_compositing_domain().data_size;
      result.share_data(this->context().cache_manager().image_coordinates.get(
          this->context(), size, CoordinatesType::Uniform));
      break;
    }
    case ImplicitInputType::SceneFrame:
      result.allocate_single_value();
      result.set_single_value(this->context().get_frame_number());
      break;
  }
}

Result &ImplicitInputOperation::get_result()
{
  return Operation::get_result(output_identifier_);
}

std::optional<Domain> ImplicitInputOperation::get_domain(const Context &context,
                                                         const ImplicitInputType implicit_input)
{
  switch (implicit_input) {
    case ImplicitInputType::UniformImageCoordinates:
      return context.get_compositing_domain();
    case ImplicitInputType::SceneFrame:
      return std::nullopt;
  }

  BLI_assert_unreachable();
  return std::nullopt;
}

}  // namespace blender::compositor
