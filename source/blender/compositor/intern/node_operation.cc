/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "GPU_debug.hh"

#include "NOD_eval_log.hh"

#include "COM_algorithm_compute_preview.hh"
#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

NodeOperation::NodeOperation(Context &context, const bNode &node) : Operation(context), node_(node)
{
  for (const bNodeSocket *output : this->node().output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    populate_result(output->identifier, get_node_socket_result_type(output));
  }

  for (const bNodeSocket *input : this->node().input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
    declare_input_descriptor(input->identifier, input_descriptor);
  }
}

class ScopedNodeTimer {
 private:
  const bNode &node_;
  const ComputeContext &compute_context_;
  nodes::eval_log::NodesEvalLog *log_;

  nodes::eval_log::TimePoint start_;

 public:
  ScopedNodeTimer(const bNode &node,
                  const ComputeContext &compute_context,
                  nodes::eval_log::NodesEvalLog *log)
      : node_(node), compute_context_(compute_context), log_(log)
  {
    start_ = nodes::eval_log::Clock::now();
  }

  ~ScopedNodeTimer()
  {
    if (!log_) {
      return;
    }
    const nodes::eval_log::TimePoint end = nodes::eval_log::Clock::now();
    nodes::eval_log::NodeTreeLogger &tree_logger = log_->get_local_tree_logger(compute_context_);
    tree_logger.node_execution_times.append(*tree_logger.allocator,
                                            {node_.identifier, start_, end});
  }
};

void NodeOperation::evaluate()
{
  const ScopedNodeTimer node_timer{
      this->node(), this->get_compute_context(), this->context().nodes_evaluation_log()};
  if (this->context().use_gpu()) {
    GPU_debug_group_begin(this->node().typeinfo->idname.c_str());
  }
  Operation::evaluate();
  if (this->context().use_gpu()) {
    GPU_debug_group_end();
  }
}

void NodeOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const bNodeSocket *output : this->node().output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    const int reference_count = number_of_inputs_linked_to_output_conditioned(
        *output, [&](const bNodeSocket &input) {
          return schedule.nodes.contains(&input.owner_node()) &&
                 !schedule.unneeded_inputs.contains(&input);
        });

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

void NodeOperation::set_compute_context(const ComputeContext &compute_context)
{
  compute_context_ = &compute_context;
}

const ComputeContext &NodeOperation::get_compute_context() const
{
  return *compute_context_;
}

void NodeOperation::set_needs_node_previews(const bool needed)
{
  needs_node_previews_ = needed;
}

static destruct_ptr<nodes::eval_log::ImageInfoLog> get_image_info_log(LinearAllocator<> *allocator,
                                                                      const Result &result)
{
  const Domain &domain = result.domain();
  return allocator->construct<nodes::eval_log::ImageInfoLog>(
      domain.data_size,
      domain.display_size,
      domain.data_offset,
      domain.transformation,
      to_string(domain.realization_options.interpolation),
      to_string(domain.realization_options.extension_x),
      to_string(domain.realization_options.extension_y),
      to_string(result.precision()));
}

void NodeOperation::log_data()
{
  nodes::eval_log::NodesEvalLog *log = this->context().nodes_evaluation_log();
  if (!log) {
    return;
  }
  nodes::eval_log::NodeTreeLogger &tree_logger = log->get_local_tree_logger(*compute_context_);

  /* Log input values. */
  for (const bNodeSocket *input_socket : this->node().input_sockets()) {
    if (!is_socket_available(input_socket)) {
      continue;
    }

    const InputDescriptor &input_descriptor = this->get_input_descriptor(input_socket->identifier);
    if (!input_socket->is_logically_linked() && !input_descriptor.implicit_input.has_value()) {
      continue;
    }

    const Result &input = this->get_input(input_socket->identifier);
    if (input.is_single_value()) {
      tree_logger.log_value(this->node(), *input_socket, input.single_value());
      continue;
    }

    tree_logger.input_socket_values.append(*tree_logger.allocator,
                                           {node_.identifier,
                                            input_socket->index(),
                                            get_image_info_log(tree_logger.allocator, input)});
  }

  /* Log output values. */
  for (const bNodeSocket *output_socket : this->node().output_sockets()) {
    if (!is_socket_available(output_socket)) {
      continue;
    }

    const Result &result = this->get_result(output_socket->identifier);
    if (!result.is_allocated()) {
      continue;
    }

    if (result.is_single_value()) {
      tree_logger.log_value(this->node(), *output_socket, result.single_value());
      continue;
    }

    tree_logger.output_socket_values.append(*tree_logger.allocator,
                                            {node_.identifier,
                                             output_socket->index(),
                                             get_image_info_log(tree_logger.allocator, result)});
  }

  /* Log node preview. */
  if (needs_node_previews_ && is_node_preview_needed(this->node())) {
    const Result *result = this->get_preview_result();
    if (result && !result->is_single_value()) {
      ImBuf *preview = compositor::compute_preview(this->context(), *result);
      tree_logger.node_image_previews.append(*tree_logger.allocator, {node_.identifier, preview});
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
