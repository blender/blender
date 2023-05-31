/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Simple Operation
 *
 * A simple operation is an operation that takes exactly one input and computes exactly one output.
 * Moreover, the output is guaranteed to only have a single user, that is, its reference count will
 * be one. Such operations can be attached to the inputs of operations to pre-process the inputs to
 * prepare them before the operation is executed. */
class SimpleOperation : public Operation {
 private:
  /* The identifier of the output. This is constant for all operations. */
  static const StringRef output_identifier_;
  /* The identifier of the input. This is constant for all operations. */
  static const StringRef input_identifier_;

 public:
  using Operation::Operation;

  /* Get a reference to the output result of the operation, this essentially calls the super
   * get_result method with the output identifier of the operation. */
  Result &get_result();

  /* Map the input of the operation to the given result, this essentially calls the super
   * map_input_to_result method with the input identifier of the operation. */
  void map_input_to_result(Result *result);

 protected:
  /* Simple operations don't need input processors, so override with an empty implementation. */
  void add_and_evaluate_input_processors() override;

  /* Get a reference to the input result of the operation, this essentially calls the super
   * get_result method with the input identifier of the operation. */
  Result &get_input();

  /* Switch the result mapped to the input with the given result, this essentially calls the super
   * switch_result_mapped_to_input method with the input identifier of the operation. */
  void switch_result_mapped_to_input(Result *result);

  /* Populate the result of the operation, this essentially calls the super populate_result method
   * with the output identifier of the operation and sets the initial reference count of the result
   * to 1, since the result of an operation is guaranteed to have a single user. */
  void populate_result(Result result);

  /* Declare the descriptor of the input of the operation to be the given descriptor, this
   * essentially calls the super declare_input_descriptor method with the input identifier of the
   * operation. */
  void declare_input_descriptor(InputDescriptor descriptor);

  /* Get a reference to the descriptor of the input, this essentially calls the super
   * get_input_descriptor method with the input identifier of the operation. */
  InputDescriptor &get_input_descriptor();
};

}  // namespace blender::realtime_compositor
