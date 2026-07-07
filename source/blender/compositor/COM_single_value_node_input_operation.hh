/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Single Value Node Input Operation
 *
 * An operation that outputs a single value result whose value is the value of an unlinked input
 * socket. This is typically used to initialize the values of unlinked node input sockets. */
class SingleValueNodeInputOperation : public Operation {
 private:
  /* The identifier of the output. */
  static const StringRef output_identifier_;
  /* The input socket whose value will be computed as the operation's result. */
  const bNodeSocket &input_socket_;

 public:
  SingleValueNodeInputOperation(Context &context, const bNodeSocket &input_socket);

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

}  // namespace blender::compositor
