/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string>

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "COM_input_single_value_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

const StringRef InputSingleValueOperation::output_identifier_ = StringRef("Output");

InputSingleValueOperation::InputSingleValueOperation(Context &context, DInputSocket input_socket)
    : Operation(context), input_socket_(input_socket)
{
  const ResultType result_type = get_node_socket_result_type(input_socket_.bsocket());
  this->populate_result(context.create_result(result_type));
}

void InputSingleValueOperation::execute()
{
  Result &result = this->get_result();
  result.allocate_single_value();

  switch (input_socket_->type) {
    case SOCK_FLOAT: {
      const float value = input_socket_->default_value_typed<bNodeSocketValueFloat>()->value;
      result.set_single_value(value);
      break;
    }
    case SOCK_INT: {
      const int value = input_socket_->default_value_typed<bNodeSocketValueInt>()->value;
      result.set_single_value(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = input_socket_->default_value_typed<bNodeSocketValueBoolean>()->value;
      result.set_single_value(value);
      break;
    }
    case SOCK_VECTOR: {
      switch (input_socket_->default_value_typed<bNodeSocketValueVector>()->dimensions) {
        case 2: {
          const float2 value = input_socket_->default_value_typed<bNodeSocketValueVector>()->value;
          result.set_single_value(value);
          break;
        }
        case 3: {
          const float3 value = input_socket_->default_value_typed<bNodeSocketValueVector>()->value;
          result.set_single_value(value);
          break;
        }
        case 4: {
          const float4 value = input_socket_->default_value_typed<bNodeSocketValueVector>()->value;
          result.set_single_value(value);
          break;
        }
        default:
          BLI_assert_unreachable();
          break;
      }
      break;
    }
    case SOCK_RGBA: {
      const Color value = input_socket_->default_value_typed<bNodeSocketValueRGBA>()->value;
      result.set_single_value(value);
      break;
    }
    case SOCK_MENU: {
      const int32_t value = input_socket_->default_value_typed<bNodeSocketValueMenu>()->value;
      result.set_single_value(nodes::MenuValue(value));
      break;
    }
    case SOCK_STRING: {
      const std::string value =
          input_socket_->default_value_typed<bNodeSocketValueString>()->value;
      result.set_single_value(value);
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }
}

Result &InputSingleValueOperation::get_result()
{
  return Operation::get_result(output_identifier_);
}

void InputSingleValueOperation::populate_result(Result result)
{
  Operation::populate_result(output_identifier_, result);
}

}  // namespace blender::compositor
