/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_lazy_function_graph_executor.hh"

/* The entire executor is included here. Otherwise an additional indirection using forward
 * declarations of #GenericGraphExecutor would be needed. However, there isn't really a point in
 * having that because it's tightly coupled to #GraphExecutor anyway. It's only defined in a
 * separate file for code organization purposes. */
#include "lazy_function_graph_executor_generic.hh"

namespace blender::fn::lazy_function {

GraphExecutor::GraphExecutor(const Graph &graph,
                             const Logger *logger,
                             const SideEffectProvider *side_effect_provider,
                             const NodeExecuteWrapper *node_execute_wrapper)
    : GraphExecutor(graph,
                    Vector<const GraphInputSocket *>(graph.graph_inputs()),
                    Vector<const GraphOutputSocket *>(graph.graph_outputs()),
                    logger,
                    side_effect_provider,
                    node_execute_wrapper)
{
}

GraphExecutor::GraphExecutor(const Graph &graph,
                             Vector<const GraphInputSocket *> graph_inputs,
                             Vector<const GraphOutputSocket *> graph_outputs,
                             const Logger *logger,
                             const SideEffectProvider *side_effect_provider,
                             const NodeExecuteWrapper *node_execute_wrapper)
    : graph_(graph),
      graph_inputs_(std::move(graph_inputs)),
      graph_outputs_(std::move(graph_outputs)),
      graph_input_index_by_socket_index_(graph.graph_inputs().size(), -1),
      graph_output_index_by_socket_index_(graph.graph_outputs().size(), -1),
      logger_(logger),
      side_effect_provider_(side_effect_provider),
      node_execute_wrapper_(node_execute_wrapper)
{
  debug_name_ = graph.name().c_str();

  /* The graph executor can handle partial execution when there are still missing inputs. */
  allow_missing_requested_inputs_ = true;

  for (const int i : graph_inputs_.index_range()) {
    const OutputSocket &socket = *graph_inputs_[i];
    BLI_assert(socket.node().is_interface());
    inputs_.append({"In", socket.type(), ValueUsage::Maybe});
    graph_input_index_by_socket_index_[socket.index()] = i;
  }
  for (const int i : graph_outputs_.index_range()) {
    const InputSocket &socket = *graph_outputs_[i];
    BLI_assert(socket.node().is_interface());
    outputs_.append({"Out", socket.type()});
    graph_output_index_by_socket_index_[socket.index()] = i;
  }

  GenericExecutor::preprocess_graph(*this);
}

void GraphExecutor::execute_impl(Params &params, const Context &context) const
{
  GenericExecutor &executor = *static_cast<GenericExecutor *>(context.storage);
  executor.execute(params, context);
}

void *GraphExecutor::init_storage(LinearAllocator<> &allocator) const
{
  GenericExecutor &executor = *allocator.construct<GenericExecutor>(*this).release();
  return &executor;
}

void GraphExecutor::destruct_storage(void *storage) const
{
  std::destroy_at(static_cast<GenericExecutor *>(storage));
}

std::string GraphExecutor::input_name(const int index) const
{
  const lf::OutputSocket &socket = *graph_inputs_[index];
  return socket.name();
}

std::string GraphExecutor::output_name(const int index) const
{
  const lf::InputSocket &socket = *graph_outputs_[index];
  return socket.name();
}

GraphExecutorLogger::LoggingEnabledState GraphExecutorLogger::get_logging_enabled_state(
    const Context & /*context*/) const
{
  return LoggingEnabledState{true};
}

void GraphExecutorLogger::log_socket_value(const Socket &socket,
                                           const GPointer value,
                                           const Context &context) const
{
  UNUSED_VARS(socket, value, context);
}

void GraphExecutorLogger::log_before_node_execute(const FunctionNode &node,
                                                  const Params &params,
                                                  const Context &context) const
{
  UNUSED_VARS(node, params, context);
}

void GraphExecutorLogger::log_after_node_execute(const FunctionNode &node,
                                                 const Params &params,
                                                 const Context &context) const
{
  UNUSED_VARS(node, params, context);
}

Vector<const FunctionNode *> GraphExecutorSideEffectProvider::get_nodes_with_side_effects(
    const Context &context) const
{
  UNUSED_VARS(context);
  return {};
}

void GraphExecutorLogger::dump_when_outputs_are_missing(const FunctionNode &node,
                                                        Span<const OutputSocket *> missing_sockets,
                                                        const Context &context) const
{
  UNUSED_VARS(node, missing_sockets, context);
}

void GraphExecutorLogger::dump_when_input_is_set_twice(const InputSocket &target_socket,
                                                       const OutputSocket &from_socket,
                                                       const Context &context) const
{
  UNUSED_VARS(target_socket, from_socket, context);
}

}  // namespace blender::fn::lazy_function
