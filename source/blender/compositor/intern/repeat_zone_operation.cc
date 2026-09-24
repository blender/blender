/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_compute_contexts.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "COM_node_tree_evaluator.hh"
#include "COM_repeat_zone_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"
#include "COM_zone_tree_operation.hh"

namespace blender::compositor {

void RepeatZoneOperation::execute()
{
  const int iterations_count = this->get_input("Iterations").get_single_value_default<int>();
  if (iterations_count <= 0) {
    this->allocate_default_remaining_outputs();
    return;
  }

  std::unique_ptr<ZoneTreeOperation> last_operation;
  const IndexRange iterations_range = IndexRange(iterations_count);
  for (const int64_t i : iterations_range) {
    const bke::RepeatZoneComputeContext compute_context(
        &compute_context_, *this->zone().output_node(), i);
    ZoneTreeOperation *zone_tree_operation = new ZoneTreeOperation(
        this->context(), this->zone(), compute_context);

    Result iteration_input = this->context().create_result(ResultType::Int);
    iteration_input.allocate_single_value();
    iteration_input.set_single_value(int(i));
    zone_tree_operation->map_input_to_result("Iteration", &iteration_input);

    if (i == iterations_range.last()) {
      this->set_reference_counts(*zone_tree_operation);
    }
    Vector<std::unique_ptr<Result>> inputs = this->map_inputs(*zone_tree_operation,
                                                              last_operation.get());
    Vector<std::unique_ptr<Result>> border_inputs = this->map_external_inputs(
        *zone_tree_operation);
    zone_tree_operation->evaluate();

    last_operation.reset(zone_tree_operation);

    if (this->context().is_canceled()) {
      this->cancel_evaluation(*last_operation);
      return;
    }
  }

  this->write_outputs(*last_operation);
}

void RepeatZoneOperation::set_reference_counts(ZoneTreeOperation &zone_tree_operation)
{
  for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    Result &zone_result = zone_tree_operation.get_result(output->identifier);
    Result &repeat_zone_result = this->get_result(output->identifier);
    zone_result.set_reference_count(repeat_zone_result.should_compute() ? 1 : 0);
  }
}

Vector<std::unique_ptr<Result>> RepeatZoneOperation::map_inputs(
    ZoneTreeOperation &zone_tree_operation, ZoneTreeOperation *last_zone_tree_operation)
{
  Vector<std::unique_ptr<Result>> temporary_inputs;
  /* If a last operation exist, map inputs to the outputs of the last iteration instead. */
  if (last_zone_tree_operation) {
    for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      Result &last_result = last_zone_tree_operation->get_result(output->identifier);
      std::unique_ptr<Result> temporary_input = std::make_unique<Result>(
          this->context().create_result(last_result.type(), last_result.precision()));
      temporary_input->share_data(last_result);
      temporary_inputs.append(std::move(temporary_input));
      zone_tree_operation.map_input_to_result(output->identifier, temporary_inputs.last().get());
    }
    return temporary_inputs;
  }

  /* Otherwise, map the initial inputs of the operation. */
  for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const Result &input_result = this->get_input(output->identifier);
    std::unique_ptr<Result> temporary_input = std::make_unique<Result>(
        this->context().create_result(input_result.type(), input_result.precision()));
    temporary_input->share_data(input_result);
    temporary_inputs.append(std::move(temporary_input));
    zone_tree_operation.map_input_to_result(output->identifier, temporary_inputs.last().get());
  }
  return temporary_inputs;
}

Vector<std::unique_ptr<Result>> RepeatZoneOperation::map_external_inputs(
    ZoneTreeOperation &zone_tree_operation)
{
  Vector<std::unique_ptr<Result>> temporary_inputs;
  Set<const bNodeSocket *> outputs_already_considered;
  for (const bNodeLink *link : this->zone().border_links) {
    const bNodeSocket *output = get_output_linked_to_input(*link->tosock);
    if (!output) {
      continue;
    }

    if (outputs_already_considered.contains(output)) {
      continue;
    }
    outputs_already_considered.add_new(output);

    const std::string input_identifier = ZoneTreeOperation::get_external_input_identifier(*output);
    const Result &input_result = this->get_input(input_identifier);
    std::unique_ptr<Result> temporary_input = std::make_unique<Result>(
        this->context().create_result(input_result.type(), input_result.precision()));
    temporary_input->share_data(input_result);
    temporary_inputs.append(std::move(temporary_input));
    zone_tree_operation.map_input_to_result(input_identifier, temporary_inputs.last().get());
  }
  return temporary_inputs;
}

void RepeatZoneOperation::write_outputs(ZoneTreeOperation &zone_tree_operation)
{
  for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    Result &zone_tree_result = zone_tree_operation.get_result(output->identifier);
    Result &repeat_zone_result = this->get_result(output->identifier);
    if (repeat_zone_result.should_compute()) {
      repeat_zone_result.share_data(zone_tree_result);
      zone_tree_result.release();
    }
  }
}

void RepeatZoneOperation::cancel_evaluation(ZoneTreeOperation &last_zone_tree_operation)
{
  this->allocate_default_remaining_outputs();

  for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    Result &zone_tree_result = last_zone_tree_operation.get_result(output->identifier);
    if (zone_tree_result.should_compute()) {
      zone_tree_result.release();
    }
  }
}

}  // namespace blender::compositor
