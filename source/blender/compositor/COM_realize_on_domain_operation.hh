/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Realize On Domain Operation
 *
 * A simple operation that projects the input on a certain target domain, copies the area of the
 * input that intersects the target domain, and fill the rest with the extension options in the
 * realization options of the input. See the discussion in COM_domain.hh for more information. */
class RealizeOnDomainOperation : public SimpleOperation {
 private:
  /* The target domain to realize the input on. */
  Domain target_domain_;

 public:
  RealizeOnDomainOperation(Context &context, Domain target_domain, ResultType type);

  void execute() override;

  /* Determine if a realize on domain operation is needed for the input with the given result and
   * descriptor in an operation with the given operation domain. If it is not needed, return a null
   * pointer. If it is needed, return an instance of the operation.
   *
   * Since operations might not be transform-invariant, the rotation and scale components of the
   * operation domain are realized and the size of the domain is increased/reduced to adapt to the
   * new transformation. For instance, if the transformation is a rotation, the domain will be
   * rotated and expanded in size to account for the bounding box of the domain after rotation. */
  static SimpleOperation *construct_if_needed(Context &context,
                                              const Result &input_result,
                                              const InputDescriptor &input_descriptor,
                                              const Domain &operation_domain);

 protected:
  /* The operation domain is just the target domain. */
  Domain compute_domain() override;

 private:
  /* Get the name of the realization shader of the appropriate type. */
  const char *get_realization_shader_name();

  /* Computes the translation that the input should be translated by to fix the artifacts related
   * to interpolation. See the implementation for more information. */
  float2 compute_corrective_translation();

  void realize_on_domain_gpu(const float3x3 &inverse_transformation);
  void realize_on_domain_cpu(const float3x3 &inverse_transformation);
};

}  // namespace blender::compositor
