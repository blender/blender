/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_algorithm_realize_on_domain.hh"
#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"

#include "COM_realize_on_domain_operation.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Realize On Domain Operation
 */

RealizeOnDomainOperation::RealizeOnDomainOperation(Context &context,
                                                   Domain domain,
                                                   ResultType type)
    : SimpleOperation(context), domain_(domain)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = type;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(type));
}

void RealizeOnDomainOperation::execute()
{
  realize_on_domain(context(),
                    get_input(),
                    get_result(),
                    domain_,
                    get_input().domain().transformation,
                    get_input().get_realization_options());
}

Domain RealizeOnDomainOperation::compute_domain()
{
  return domain_;
}

SimpleOperation *RealizeOnDomainOperation::construct_if_needed(
    Context &context,
    const Result &input_result,
    const InputDescriptor &input_descriptor,
    const Domain &operation_domain)
{
  /* This input doesn't need realization, the operation is not needed. */
  if (!input_descriptor.realization_options.realize_on_operation_domain) {
    return nullptr;
  }

  /* The input expects a single value and if no single value is provided, it will be ignored and a
   * default value will be used, so no need to realize it and the operation is not needed. */
  if (input_descriptor.expects_single_value) {
    return nullptr;
  }

  /* Input result is a single value and does not need realization, the operation is not needed. */
  if (input_result.is_single_value()) {
    return nullptr;
  }

  /* The input have an identical domain to the operation domain, so no need to realize it and the
   * operation is not needed. */
  if (input_result.domain() == operation_domain) {
    return nullptr;
  }

  /* Otherwise, realization is needed. */
  return new RealizeOnDomainOperation(context, operation_domain, input_descriptor.type);
}

}  // namespace blender::realtime_compositor
