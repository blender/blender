/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string>

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_evaluator.hh"
#include "COM_input_single_value_operation.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

Evaluator::Evaluator(Context &context) : context_(context) {}

void Evaluator::evaluate()
{
  context_.cache_manager().reset();
  context_.texture_pool().reset();

  if (!is_compiled_) {
    compile_and_evaluate();
    is_compiled_ = true;
    return;
  }

  for (const std::unique_ptr<Operation> &operation : operations_stream_) {
    operation->evaluate();
  }
}

void Evaluator::reset()
{
  operations_stream_.clear();
  derived_node_tree_.reset();

  is_compiled_ = false;
}

bool Evaluator::validate_node_tree()
{
  if (derived_node_tree_->has_link_cycles()) {
    context_.set_info_message("Compositor node tree has cyclic links!");
    return false;
  }

  if (derived_node_tree_->has_undefined_nodes_or_sockets()) {
    context_.set_info_message("Compositor node tree has undefined nodes or sockets!");
    return false;
  }

  return true;
}

void Evaluator::compile_and_evaluate()
{
  derived_node_tree_ = std::make_unique<DerivedNodeTree>(*context_.get_scene()->nodetree);

  if (!validate_node_tree()) {
    return;
  }

  const Schedule schedule = compute_schedule(*derived_node_tree_);

  CompileState compile_state(schedule);

  for (const DNode &node : schedule) {
    if (compile_state.should_compile_shader_compile_unit(node)) {
      compile_and_evaluate_shader_compile_unit(compile_state);
    }

    if (is_shader_node(node)) {
      compile_state.add_node_to_shader_compile_unit(node);
    }
    else {
      compile_and_evaluate_node(node, compile_state);
    }
  }
}

void Evaluator::compile_and_evaluate_node(DNode node, CompileState &compile_state)
{
  NodeOperation *operation = node->typeinfo->get_compositor_operation(context_, node);

  compile_state.map_node_to_node_operation(node, operation);

  map_node_operation_inputs_to_their_results(node, operation, compile_state);

  /* This has to be done after input mapping because the method may add Input Single Value
   * Operations to the operations stream, which needs to be evaluated before the operation itself
   * is evaluated. */
  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(compile_state.get_schedule());

  operation->evaluate();
}

void Evaluator::map_node_operation_inputs_to_their_results(DNode node,
                                                           NodeOperation *operation,
                                                           CompileState &compile_state)
{
  for (const bNodeSocket *input : node->input_sockets()) {
    const DInputSocket dinput{node.context(), input};

    DSocket dorigin = get_input_origin_socket(dinput);

    /* The origin socket is an output, which means the input is linked. So map the input to the
     * result we get from the output. */
    if (dorigin->is_output()) {
      Result &result = compile_state.get_result_from_output_socket(DOutputSocket(dorigin));
      operation->map_input_to_result(input->identifier, &result);
      continue;
    }

    /* Otherwise, the origin socket is an input, which either means the input is unlinked and the
     * origin is the input socket itself or the input is connected to an unlinked input of a group
     * input node and the origin is the input of the group input node. So map the input to the
     * result of a newly created Input Single Value Operation. */
    auto *input_operation = new InputSingleValueOperation(context_, DInputSocket(dorigin));
    operation->map_input_to_result(input->identifier, &input_operation->get_result());

    operations_stream_.append(std::unique_ptr<InputSingleValueOperation>(input_operation));

    input_operation->evaluate();
  }
}

void Evaluator::compile_and_evaluate_shader_compile_unit(CompileState &compile_state)
{
  ShaderCompileUnit &compile_unit = compile_state.get_shader_compile_unit();
  ShaderOperation *operation = new ShaderOperation(context_, compile_unit);

  for (DNode node : compile_unit) {
    compile_state.map_node_to_shader_operation(node, operation);
  }

  map_shader_operation_inputs_to_their_results(operation, compile_state);

  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(compile_state.get_schedule());

  operation->evaluate();

  compile_state.reset_shader_compile_unit();
}

void Evaluator::map_shader_operation_inputs_to_their_results(ShaderOperation *operation,
                                                             CompileState &compile_state)
{
  for (const auto item : operation->get_inputs_to_linked_outputs_map().items()) {
    Result &result = compile_state.get_result_from_output_socket(item.value);
    operation->map_input_to_result(item.key, &result);
  }
}

}  // namespace blender::realtime_compositor
