/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader.h"

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

 private:
  /* Get the realization shader of the appropriate type. */
  GPUShader *get_realization_shader();
};

/* ------------------------------------------------------------------------------------------------
 * Realize Transformation Operation
 *
 * A simple operation that realizes its input on a domain such that its transformations become
 * identity and its size is increased/decreased to adapt to the new domain. The transformations
 * that become identity are the ones marked to be realized in the given InputRealizationOptions.
 * For instance, if InputRealizationOptions.realize_rotation is true and the input is rotated, the
 * output of the operation will be a result of zero rotation but expanded size to account for the
 * bounding box of the input after rotation. This is useful for operations that are not rotation
 * invariant and thus require an input of zero rotation for correct operation.
 *
 * Notice that this class is not an actual operation, but constructs a RealizeOnDomainOperation
 * with the appreciate domain in its construct_if_needed static constructor. */
class RealizeTransformationOperation {
 public:
  /* Determine if a realize transformation operation is needed for the input with the given result
   * and descriptor. If it is not needed, return a null pointer. If it is needed, return an
   * instance of RealizeOnDomainOperation with the appropriate domain. */
  static SimpleOperation *construct_if_needed(Context &context,
                                              const Result &input_result,
                                              const InputDescriptor &input_descriptor);

 private:
  /* Given the domain of an input and its realization options, compute a domain such that the
   * appropriate transformations specified in the realization options become identity and the size
   * of the domain is increased/reduced to adapt to the new domain. */
  static Domain compute_target_domain(const Domain &input_domain,
                                      const InputRealizationOptions &realization_options);
};

}  // namespace blender::realtime_compositor
