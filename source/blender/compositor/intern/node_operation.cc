/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_string_ref.hh"
#include "BLI_timeit.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "GPU_debug.hh"

#include "COM_algorithm_compute_preview.hh"
#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

NodeOperation::NodeOperation(Context &context, const bNode &node) : Operation(context), node_(node)
{
  for (const bNodeSocket *output : this->node().output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const ResultType result_type = get_node_socket_result_type(output);
    populate_result(output->identifier, context.create_result(result_type));
  }

  for (const bNodeSocket *input : this->node().input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
    declare_input_descriptor(input->identifier, input_descriptor);
  }
}

void NodeOperation::evaluate()
{
  if (this->context().use_gpu()) {
    GPU_debug_group_begin(this->node().typeinfo->idname.c_str());
  }
  const timeit::TimePoint before_time = timeit::Clock::now();
  Operation::evaluate();
  const timeit::TimePoint after_time = timeit::Clock::now();
  if (this->context().profiler()) {
    this->context().profiler()->set_node_evaluation_time(instance_key_, after_time - before_time);
  }
  if (this->context().use_gpu()) {
    GPU_debug_group_end();
  }
}

void NodeOperation::compute_results_reference_counts(const VectorSet<const bNode *> &schedule)
{
  for (const bNodeSocket *output : this->node().output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const int reference_count = number_of_inputs_linked_to_output_conditioned(
        *output, [&](const bNodeSocket &input) { return schedule.contains(&input.owner_node()); });

    this->get_result(output->identifier).set_reference_count(reference_count);
  }
}

void NodeOperation::set_instance_key(const bNodeInstanceKey &instance_key)
{
  instance_key_ = instance_key;
}

const bNodeInstanceKey &NodeOperation::get_instance_key() const
{
  return instance_key_;
}

void NodeOperation::set_node_previews(Map<bNodeInstanceKey, bke::bNodePreview> *node_previews)
{
  node_previews_ = node_previews;
}

Map<bNodeInstanceKey, bke::bNodePreview> *NodeOperation::get_node_previews()
{
  return node_previews_;
}

void NodeOperation::compute_preview()
{
  if (node_previews_ && is_node_preview_needed(this->node())) {
    const Result *result = get_preview_result();
    if (result) {
      compositor::compute_preview(context(), node_previews_, this->get_instance_key(), *result);
    }
  }
}

const bNode &NodeOperation::node() const
{
  return node_;
}

Result *NodeOperation::get_preview_result()
{
  /* Find the first linked output. */
  for (const bNodeSocket *output : this->node().output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    Result &output_result = this->get_result(output->identifier);
    if (output_result.should_compute()) {
      return &output_result;
    }
  }

  /* No linked outputs, but no inputs either, so nothing to preview. */
  if (this->node().input_sockets().is_empty()) {
    return nullptr;
  }

  /* Find the first allocated input. */
  for (const bNodeSocket *input : this->node().input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    Result &input_result = this->get_input(input->identifier);
    if (input_result.is_allocated()) {
      return &input_result;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

}  // namespace blender::compositor
