/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Conversion Operation
 *
 * A simple operation that converts a result from a certain type to another. See the derived
 * classes for more details. */
class ConversionOperation : public SimpleOperation {
 public:
  ConversionOperation(Context &context,
                      const ResultType input_type,
                      const ResultType expected_type);

  /* If the input result is a single value, execute_single is called. Otherwise, the shader
   * provided by get_conversion_shader is dispatched for GPU contexts or execute_cpu is called for
   * CPU contexts. */
  void execute() override;

  /* Determine if a conversion operation is needed for the input with the given result and
   * descriptor. If it is not needed, return a null pointer. If it is needed, return an instance of
   * the appropriate conversion operation. */
  static SimpleOperation *construct_if_needed(Context &context,
                                              const Result &input_result,
                                              const InputDescriptor &input_descriptor);

 private:
  /* Convert the input single value result to the output single value result. */
  void execute_single(const Result &input, Result &output);

  /* Convert the input to the appropriate type and write the result to the output on the CPU. */
  void execute_cpu(const Result &input, Result &output);

  /* Get the name of the shader the will be used for conversion. */
  const char *get_conversion_shader_name();
};

}  // namespace blender::compositor
