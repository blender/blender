/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_compute_contexts.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "COM_node_tree_evaluator.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"
#include "COM_zone_tree_operation.hh"

namespace blender::compositor {

ZoneTreeOperation::ZoneTreeOperation(Context &context,
                                     const bke::bNodeTreeZone &zone,
                                     const ComputeContext &compute_context)
    : Operation(context), zone_(zone), compute_context_(compute_context)
{
  /* Declare inputs. */
  for (const bNodeSocket *output : zone_.input_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const InputDescriptor input_descriptor = InputDescriptor{get_node_socket_result_type(output),
                                                             InputRealizationMode::None};
    this->declare_input_descriptor(output->identifier, input_descriptor);
  }

  /* Declare inputs for border links. */
  Set<const bNodeSocket *> outputs_already_considered;
  for (const bNodeLink *link : zone_.border_links) {
    const bNodeSocket *output = get_output_linked_to_input(*link->tosock);
    if (!output) {
      continue;
    }

    if (outputs_already_considered.contains(output)) {
      continue;
    }
    outputs_already_considered.add_new(output);

    const InputDescriptor input_descriptor = InputDescriptor{get_node_socket_result_type(output),
                                                             InputRealizationMode::None};
    const std::string input_identifier = ZoneTreeOperation::get_external_input_identifier(*output);
    this->declare_input_descriptor(input_identifier, input_descriptor);
  }

  /* Declare outputs. */
  for (const bNodeSocket *input : zone_.output_node()->input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    this->populate_result(input->identifier, get_node_socket_result_type(input));
  }
}

/* Get the results associated with sockets in the zone prior to its evaluation. */
static Result *get_zone_result_for_socket(ZoneTreeOperation &operation,
                                          const bke::bNodeTreeZone &zone,
                                          const bNodeSocket &socket)
{
  if (zone.input_node() == &socket.owner_node()) {
    return &operation.get_input(socket.identifier);
  }
  if (zone.output_node() == &socket.owner_node()) {
    return &operation.get_result(socket.identifier);
  }
  return nullptr;
}

void ZoneTreeOperation::execute()
{
  auto socket_result_fn = [&](const bNodeSocket &socket) {
    return get_zone_result_for_socket(*this, zone_, socket);
  };
  const Schedule schedule = compute_schedule(
      this->context(), *zone_.owner->tree, compute_context_, socket_result_fn, &zone_);
  NodeTreeEvaluator node_tree_evaluator(this->context(), schedule, *this, compute_context_);
  node_tree_evaluator.evaluate();

  /* Some of the needed outputs might not be allocated even after execution. This could happen for
   * instance when the evaluation gets canceled before the output is written. */
  this->allocate_default_remaining_outputs();
}

std::string ZoneTreeOperation::get_external_input_identifier(const bNodeSocket &output)
{
  return std::string("External Input: ") + output.owner_node().name + ":" + output.identifier;
}

}  // namespace blender::compositor
