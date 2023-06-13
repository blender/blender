/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_node_declaration.hh"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

NodeOperation::NodeOperation(Context &context, DNode node) : Operation(context), node_(node)
{
  for (const bNodeSocket *output : node->output_sockets()) {
    const ResultType result_type = get_node_socket_result_type(output);
    const Result result = Result(result_type, texture_pool());
    populate_result(output->identifier, result);
  }

  for (const bNodeSocket *input : node->input_sockets()) {
    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
    declare_input_descriptor(input->identifier, input_descriptor);
  }
}

void NodeOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const bNodeSocket *output : this->node()->output_sockets()) {
    const DOutputSocket doutput{node().context(), output};

    const int reference_count = number_of_inputs_linked_to_output_conditioned(
        doutput, [&](DInputSocket input) { return schedule.contains(input.node()); });

    get_result(doutput->identifier).set_initial_reference_count(reference_count);
  }
}

const DNode &NodeOperation::node() const
{
  return node_;
}

const bNode &NodeOperation::bnode() const
{
  return *node_;
}

bool NodeOperation::should_compute_output(StringRef identifier)
{
  return get_result(identifier).should_compute();
}

}  // namespace blender::realtime_compositor
