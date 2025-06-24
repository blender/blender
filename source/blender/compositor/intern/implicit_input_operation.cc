/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"

#include "COM_implicit_input_operation.hh"
#include "COM_input_descriptor.hh"
#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::compositor {

const StringRef ImplicitInputOperation::output_identifier_ = StringRef("Output");

static ResultType get_implicit_input_result_type(const ImplicitInput implicit_input)
{
  switch (implicit_input) {
    case ImplicitInput::None:
      break;
    case ImplicitInput::TextureCoordinates:
      return ResultType::Float2;
  }

  BLI_assert_unreachable();
  return ResultType::Float2;
}

ImplicitInputOperation::ImplicitInputOperation(Context &context,
                                               const ImplicitInput implicit_input)
    : Operation(context), implicit_input_(implicit_input)
{
  this->populate_result(output_identifier_,
                        context.create_result(get_implicit_input_result_type(implicit_input)));
}

void ImplicitInputOperation::execute()
{
  Result &result = this->get_result();

  switch (implicit_input_) {
    case ImplicitInput::None:
      BLI_assert_unreachable();
      break;
    case ImplicitInput::TextureCoordinates:
      const int2 size = this->context().get_compositing_region_size();
      result.wrap_external(this->context().cache_manager().image_coordinates.get(
          this->context(), size, CoordinatesType::Uniform));
  }
}

Result &ImplicitInputOperation::get_result()
{
  return Operation::get_result(output_identifier_);
}

}  // namespace blender::compositor
