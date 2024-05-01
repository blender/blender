/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Realize On Domain Operation
 *
 * A simple operation that projects the input on a certain target domain, copies the area of the
 * input that intersects the target domain, and fill the rest with zeros or repetitions of the
 * input depending on the realization options of the target domain. See the discussion in
 * COM_domain.hh for more information. */
class RealizeOnDomainOperation : public SimpleOperation {
 private:
  /* The target domain to realize the input on. */
  Domain domain_;

 public:
  RealizeOnDomainOperation(Context &context, Domain domain, ResultType type);

  void execute() override;

  /* Determine if a realize on domain operation is needed for the input with the given result and
   * descriptor in an operation with the given operation domain. If it is not needed, return a null
   * pointer. If it is needed, return an instance of the operation. */
  static SimpleOperation *construct_if_needed(Context &context,
                                              const Result &input_result,
                                              const InputDescriptor &input_descriptor,
                                              const Domain &operation_domain);

 protected:
  /* The operation domain is just the target domain. */
  Domain compute_domain() override;
};

}  // namespace blender::realtime_compositor
