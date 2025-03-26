/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Implicit Input Operation
 *
 * An operation that outputs a result representing a specific implicit input. */
class ImplicitInputOperation : public Operation {
 private:
  /* The identifier of the output. */
  static const StringRef output_identifier_;
  /* The type of implicit input needed. */
  ImplicitInput implicit_input_;

 public:
  ImplicitInputOperation(Context &context, const ImplicitInput implicit_input);

  void execute() override;

  /* Get a reference to the output result of the operation, this essentially calls the super
   * get_result with the output identifier of the operation. */
  Result &get_result();
};

}  // namespace blender::compositor
