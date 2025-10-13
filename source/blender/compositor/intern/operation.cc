/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>
#include <memory>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "COM_context.hh"
#include "COM_conversion_operation.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_operation.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::compositor {

Operation::Operation(Context &context) : context_(context) {}

Operation::~Operation() = default;

void Operation::evaluate()
{
  evaluate_input_processors();

  execute();

  compute_preview();

  release_inputs();

  context().evaluate_operation_post();
}

Result &Operation::get_result(StringRef identifier)
{
  return results_.lookup(identifier);
}

void Operation::map_input_to_result(StringRef identifier, Result *result)
{
  results_mapped_to_inputs_.add_new(identifier, result);
}

void Operation::free_results()
{
  for (Result &result : results_.values()) {
    result.free();
  }
}

Domain Operation::compute_domain()
{
  /* Default to an identity domain in case no domain input was found, most likely because all
   * inputs are single values. */
  Domain operation_domain = Domain::identity();
  int current_domain_priority = std::numeric_limits<int>::max();

  /* Go over the inputs and find the domain of the non single value input with the highest domain
   * priority. */
  for (StringRef identifier : input_descriptors_.keys()) {
    const Result &result = get_input(identifier);
    const InputDescriptor &descriptor = get_input_descriptor(identifier);

    /* A single value input can't be a domain input. */
    if (result.is_single_value() || descriptor.expects_single_value) {
      continue;
    }

    /* An input that skips operation domain realization can't be a domain input. */
    if (descriptor.realization_mode != InputRealizationMode::OperationDomain) {
      continue;
    }

    /* Notice that the lower the domain priority value is, the higher the priority is, hence the
     * less than comparison. */
    if (descriptor.domain_priority < current_domain_priority) {
      operation_domain = result.domain();
      current_domain_priority = descriptor.domain_priority;
    }
  }

  return operation_domain;
}

void Operation::add_and_evaluate_input_processors()
{
  /* Each input processor type is added to all inputs entirely before the next type. This is done
   * because the construction of the input processors may depend on the result of previous input
   * processors for all inputs. For instance, the realize on domain input processor considers the
   * value of all inputs, so previous input processors for all inputs needs to be added and
   * evaluated first. */

  for (const StringRef &identifier : results_mapped_to_inputs_.keys()) {
    SimpleOperation *conversion = ConversionOperation::construct_if_needed(
        context(), get_input(identifier), get_input_descriptor(identifier));
    add_and_evaluate_input_processor(identifier, conversion);
  }

  for (const StringRef &identifier : results_mapped_to_inputs_.keys()) {
    SimpleOperation *realize_on_domain = RealizeOnDomainOperation::construct_if_needed(
        context(), get_input(identifier), get_input_descriptor(identifier), compute_domain());
    add_and_evaluate_input_processor(identifier, realize_on_domain);
  }
}

void Operation::add_and_evaluate_input_processor(StringRef identifier, SimpleOperation *processor)
{
  /* Allow null inputs to facilitate construct_if_needed pattern of addition. For instance, see the
   * implementation of the add_and_evaluate_input_processors method. */
  if (!processor) {
    return;
  }

  ProcessorsVector &processors = input_processors_.lookup_or_add_default(identifier);

  /* Get the result that should serve as the input for the processor. This is either the result
   * mapped to the input or the result of the last processor depending on whether this is the first
   * processor or not. */
  Result &result = processors.is_empty() ? get_input(identifier) : processors.last()->get_result();

  /* Map the input result of the processor and add it to the processors vector. */
  processor->map_input_to_result(&result);
  processors.append(std::unique_ptr<SimpleOperation>(processor));

  /* Switch the result mapped to the input to be the output result of the processor. */
  switch_result_mapped_to_input(identifier, &processor->get_result());

  processor->evaluate();
}

void Operation::compute_preview() {};

Result &Operation::get_input(StringRef identifier) const
{
  return *results_mapped_to_inputs_.lookup(identifier);
}

void Operation::switch_result_mapped_to_input(StringRef identifier, Result *result)
{
  results_mapped_to_inputs_.lookup(identifier) = result;
}

void Operation::populate_result(StringRef identifier, Result result)
{
  results_.add_new(identifier, result);
}

void Operation::declare_input_descriptor(StringRef identifier, InputDescriptor descriptor)
{
  input_descriptors_.add_new(identifier, descriptor);
}

InputDescriptor &Operation::get_input_descriptor(StringRef identifier)
{
  return input_descriptors_.lookup(identifier);
}

Context &Operation::context() const
{
  return context_;
}

void Operation::evaluate_input_processors()
{
  if (!input_processors_added_) {
    add_and_evaluate_input_processors();
    input_processors_added_ = true;
    return;
  }

  for (const ProcessorsVector &processors : input_processors_.values()) {
    for (const std::unique_ptr<SimpleOperation> &processor : processors) {
      processor->evaluate();
    }
  }
}

void Operation::release_inputs()
{
  for (Result *result : results_mapped_to_inputs_.values()) {
    result->release();
  }
}

}  // namespace blender::compositor
