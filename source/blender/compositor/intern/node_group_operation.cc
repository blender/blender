/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_compute_context.hh"
#include "BLI_string_ref.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_eval_log.hh"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_group_operation.hh"
#include "COM_node_tree_evaluator.hh"
#include "COM_operation.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

NodeGroupOperation::NodeGroupOperation(Context &context,
                                       const bNodeTree &node_group,
                                       const ComputeContext &compute_context)
    : Operation(context), node_group_(node_group), compute_context_(compute_context)
{
  node_group.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *input : node_group.interface_inputs()) {
    const InputDescriptor input_descriptor = input_descriptor_from_interface_input(node_group,
                                                                                   *input);
    this->declare_input_descriptor(input->identifier, input_descriptor);
  }

  for (const bNodeTreeInterfaceSocket *output : node_group.interface_outputs()) {
    this->populate_result(output->identifier, get_node_interface_socket_result_type(*output));
  }
}

class ScopedNodeGroupTimer {
 private:
  const ComputeContext &compute_context_;
  nodes::eval_log::NodesEvalLog *log_;

  nodes::eval_log::TimePoint start_;

 public:
  ScopedNodeGroupTimer(const ComputeContext &compute_context, nodes::eval_log::NodesEvalLog *log)
      : compute_context_(compute_context), log_(log)
  {
    start_ = nodes::eval_log::Clock::now();
  }

  ~ScopedNodeGroupTimer()
  {
    if (!log_) {
      return;
    }
    const nodes::eval_log::TimePoint end = nodes::eval_log::Clock::now();
    nodes::eval_log::NodeTreeLogger &tree_logger = log_->get_local_tree_logger(compute_context_);
    tree_logger.execution_time = end - start_;
  }
};

void NodeGroupOperation::execute()
{
  const ScopedNodeGroupTimer node_group_timer{compute_context_,
                                              this->context().nodes_evaluation_log()};
  const Schedule schedule = compute_schedule(
      this->context(), this->node_group(), compute_context_, *this);
  NodeTreeEvaluator node_tree_evaluator(this->context(), schedule, *this, compute_context_);
  node_tree_evaluator.evaluate();

  /* Some of the needed outputs might not be allocated even after execution. This could happen for
   * instance when no Group Output node exist or when the evaluation gets canceled before the
   * output is written. */
  this->allocate_default_remaining_outputs();
}

const bNodeTree &NodeGroupOperation::node_group() const
{
  return node_group_;
}

}  // namespace blender::compositor
