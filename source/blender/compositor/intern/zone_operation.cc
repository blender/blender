/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_compute_contexts.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"
#include "COM_zone_operation.hh"
#include "COM_zone_tree_operation.hh"

namespace blender::compositor {

ZoneOperation::ZoneOperation(Context &context,
                             const bke::bNodeTreeZone &zone,
                             const ComputeContext &compute_context)
    : Operation(context), zone_(zone), compute_context_(compute_context)
{
  /* Declare inputs. */
  for (const bNodeSocket *input : this->zone().input_node()->input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const InputDescriptor input_descriptor = InputDescriptor{get_node_socket_result_type(input),
                                                             InputRealizationMode::None};
    this->declare_input_descriptor(input->identifier, input_descriptor);
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
  for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    this->populate_result(output->identifier, get_node_socket_result_type(output));
  }
}

void ZoneOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const bNodeSocket *output : this->zone().output_node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const int reference_count = compute_output_reference_count(*output, schedule);
    this->get_result(output->identifier).set_reference_count(reference_count);
  }
}

const bke::bNodeTreeZone &ZoneOperation::zone() const
{
  return zone_;
}

}  // namespace blender::compositor
