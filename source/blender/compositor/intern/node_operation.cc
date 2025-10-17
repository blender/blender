/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_string_ref.hh"
#include "BLI_timeit.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "BKE_node.hh"

#include "GPU_debug.hh"

#include "COM_algorithm_compute_preview.hh"
#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

NodeOperation::NodeOperation(Context &context, DNode node) : Operation(context), node_(node)
{
  for (const bNodeSocket *output : node->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const ResultType result_type = get_node_socket_result_type(output);
    populate_result(output->identifier, context.create_result(result_type));
  }

  for (const bNodeSocket *input : node->input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
    declare_input_descriptor(input->identifier, input_descriptor);
  }
}

void NodeOperation::evaluate()
{
  if (context().use_gpu()) {
    GPU_debug_group_begin(node().bnode()->typeinfo->idname.c_str());
  }
  const timeit::TimePoint before_time = timeit::Clock::now();
  Operation::evaluate();
  const timeit::TimePoint after_time = timeit::Clock::now();
  if (context().profiler()) {
    context().profiler()->set_node_evaluation_time(node_.instance_key(), after_time - before_time);
  }
  if (context().use_gpu()) {
    GPU_debug_group_end();
  }
}

void NodeOperation::compute_preview()
{
  if (flag_is_set(context().needed_outputs(), OutputTypes::Previews) &&
      is_node_preview_needed(node()))
  {
    const Result *result = get_preview_result();
    if (result) {
      compositor::compute_preview(context(), node(), *result);
    }
  }
}

Result *NodeOperation::get_preview_result()
{
  /* Find the first linked output. */
  for (const bNodeSocket *output : node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    Result &output_result = get_result(output->identifier);
    if (output_result.should_compute()) {
      return &output_result;
    }
  }

  /* No linked outputs, but no inputs either, so nothing to preview. */
  if (node()->input_sockets().is_empty()) {
    return nullptr;
  }

  /* Find the first allocated input. */
  for (const bNodeSocket *input : node()->input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    Result &input_result = get_input(input->identifier);
    if (input_result.is_allocated()) {
      return &input_result;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

void NodeOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const bNodeSocket *output : this->node()->output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const DOutputSocket doutput{node().context(), output};

    const int reference_count = number_of_inputs_linked_to_output_conditioned(
        doutput, [&](DInputSocket input) { return schedule.contains(input.node()); });

    get_result(doutput->identifier).set_reference_count(reference_count);
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

}  // namespace blender::compositor
