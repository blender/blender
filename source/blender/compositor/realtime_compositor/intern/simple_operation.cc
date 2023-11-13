/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_input_descriptor.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::realtime_compositor {

const StringRef SimpleOperation::input_identifier_ = StringRef("Input");
const StringRef SimpleOperation::output_identifier_ = StringRef("Output");

Result &SimpleOperation::get_result()
{
  return Operation::get_result(output_identifier_);
}

void SimpleOperation::map_input_to_result(Result *result)
{
  Operation::map_input_to_result(input_identifier_, result);
}

void SimpleOperation::add_and_evaluate_input_processors() {}

Result &SimpleOperation::get_input()
{
  return Operation::get_input(input_identifier_);
}

void SimpleOperation::switch_result_mapped_to_input(Result *result)
{
  Operation::switch_result_mapped_to_input(input_identifier_, result);
}

void SimpleOperation::populate_result(Result result)
{
  Operation::populate_result(output_identifier_, result);

  /* The result of a simple operation is guaranteed to have a single user. */
  get_result().set_initial_reference_count(1);
}

void SimpleOperation::declare_input_descriptor(InputDescriptor descriptor)
{
  Operation::declare_input_descriptor(input_identifier_, descriptor);
}

InputDescriptor &SimpleOperation::get_input_descriptor()
{
  return Operation::get_input_descriptor(input_identifier_);
}

}  // namespace blender::realtime_compositor
