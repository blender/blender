/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Input Single Value Operation
 *
 * An input single value operation is an operation that outputs a single value result whose value
 * is the value of an unlinked input socket. This is typically used to initialize the values of
 * unlinked node input sockets. */
class InputSingleValueOperation : public Operation {
 private:
  /* The identifier of the output. */
  static const StringRef output_identifier_;
  /* The input socket whose value will be computed as the operation's result. */
  DInputSocket input_socket_;

 public:
  InputSingleValueOperation(Context &context, DInputSocket input_socket);

  /* Allocate a single value result and set its value to the default value of the input socket. */
  void execute() override;

  /* Get a reference to the output result of the operation, this essentially calls the super
   * get_result with the output identifier of the operation. */
  Result &get_result();

 private:
  /* Populate the result of the operation, this essentially calls the super populate_result method
   * with the output identifier of the operation. */
  void populate_result(Result result);
};

}  // namespace blender::realtime_compositor
