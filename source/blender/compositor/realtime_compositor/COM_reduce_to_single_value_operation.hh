/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Reduce To Single Value Operation
 *
 * A simple operation that reduces its input result into a single value output result. The input is
 * assumed to be a texture result of size 1x1, that is, a texture composed of a single pixel, the
 * value of which shall serve as the single value of the output result. */
class ReduceToSingleValueOperation : public SimpleOperation {
 public:
  ReduceToSingleValueOperation(Context &context, ResultType type);

  /* Download the input pixel from the GPU texture and set its value to the value of the allocated
   * single value output result. */
  void execute() override;

  /* Determine if a reduce to single value operation is needed for the input with the
   * given result. If it is not needed, return a null pointer. If it is needed, return an instance
   * of the operation. */
  static SimpleOperation *construct_if_needed(Context &context, const Result &input_result);
};

}  // namespace blender::realtime_compositor
