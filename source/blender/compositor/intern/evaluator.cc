/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_evaluator.hh"
#include "COM_input_single_value_operation.hh"
#include "COM_multi_function_procedure_operation.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

Evaluator::Evaluator(Context &context) : context_(context) {}

void Evaluator::evaluate()
{
  context_.reset();

  if (!is_compiled_) {
    compile_and_evaluate();
  }
  else {
    for (const std::unique_ptr<Operation> &operation : operations_stream_) {
      if (context_.is_canceled()) {
        this->cancel_evaluation();
        break;
      }
      operation->evaluate();
    }
  }

  if (context_.profiler()) {
    context_.profiler()->finalize(context_.get_node_tree());
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

  for (const bNodeTree *node_tree : derived_node_tree_->used_btrees()) {
    for (const bNode *node : node_tree->all_nodes()) {
      /* The poll method of those two nodes perform raw pointer comparisons of node trees, so they
       * can wrongly fail since the compositor localizes the node tree, changing its pointer value
       * than the one in the main database. So handle those two nodes. */
      if (STR_ELEM(node->idname, "CompositorNodeRLayers", "CompositorNodeCryptomatteV2")) {
        continue;
      }

      const char *disabled_hint = nullptr;
      if (!node->typeinfo->poll(node->typeinfo, node_tree, &disabled_hint)) {
        context_.set_info_message("Compositor node tree has unsupported nodes.");
        return false;
      }
    }
  }

  return true;
}

void Evaluator::compile_and_evaluate()
{
  derived_node_tree_ = std::make_unique<DerivedNodeTree>(context_.get_node_tree());

  if (!validate_node_tree()) {
    return;
  }

  if (context_.is_canceled()) {
    this->cancel_evaluation();
    reset();
    return;
  }

  const Schedule schedule = compute_schedule(context_, *derived_node_tree_);

  CompileState compile_state(schedule);

  for (const DNode &node : schedule) {
    if (context_.is_canceled()) {
      this->cancel_evaluation();
      reset();
      return;
    }

    if (compile_state.should_compile_pixel_compile_unit(node)) {
      compile_and_evaluate_pixel_compile_unit(compile_state);
    }

    if (is_pixel_node(node)) {
      compile_state.add_node_to_pixel_compile_unit(node);
    }
    else {
      compile_and_evaluate_node(node, compile_state);
    }
  }

  is_compiled_ = true;
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
    InputSingleValueOperation *input_operation = new InputSingleValueOperation(
        context_, DInputSocket(dorigin));
    operation->map_input_to_result(input->identifier, &input_operation->get_result());

    operations_stream_.append(std::unique_ptr<InputSingleValueOperation>(input_operation));

    input_operation->evaluate();
  }
}

/* Create one of the concrete subclasses of the PixelOperation based on the context and compile
 * state. Deleting the operation is the caller's responsibility. */
static PixelOperation *create_pixel_operation(Context &context, CompileState &compile_state)
{
  const Schedule &schedule = compile_state.get_schedule();
  PixelCompileUnit &compile_unit = compile_state.get_pixel_compile_unit();

  /* Use multi-function procedure to execute the pixel compile unit for CPU contexts or if the
   * compile unit is single value and would thus be more efficient to execute on the CPU. */
  if (!context.use_gpu() || compile_state.is_pixel_compile_unit_single_value()) {
    return new MultiFunctionProcedureOperation(context, compile_unit, schedule);
  }

  return new ShaderOperation(context, compile_unit, schedule);
}

void Evaluator::compile_and_evaluate_pixel_compile_unit(CompileState &compile_state)
{
  PixelCompileUnit &compile_unit = compile_state.get_pixel_compile_unit();

  /* Pixel operations might have limitations on the number of outputs they can have, so we might
   * have to split the compile unit into smaller units to workaround this limitation. In practice,
   * splitting will almost always never happen due to the scheduling strategy we use, so the base
   * case remains fast. */
  int number_of_outputs = 0;
  for (int i : compile_unit.index_range()) {
    const DNode node = compile_unit[i];
    number_of_outputs += compile_state.compute_pixel_node_operation_outputs_count(node);

    if (number_of_outputs <= PixelOperation::maximum_number_of_outputs(context_)) {
      continue;
    }

    /* The number of outputs surpassed the limit, so we split the compile unit into two equal parts
     * and recursively call this method on each of them. It might seem unexpected that we split in
     * half as opposed to split at the node that surpassed the limit, but that is because the act
     * of splitting might actually introduce new outputs, since links that were previously internal
     * to the compile unit might now be external. So we can't precisely split and guarantee correct
     * units, and we just rely or recursive splitting until units are small enough. Further, half
     * splitting helps balancing the shaders, where we don't want to have one gigantic shader and
     * a tiny one. */
    const int split_index = compile_unit.size() / 2;
    const PixelCompileUnit start_compile_unit(compile_unit.as_span().take_front(split_index));
    const PixelCompileUnit end_compile_unit(compile_unit.as_span().drop_front(split_index));

    compile_state.get_pixel_compile_unit() = start_compile_unit;
    this->compile_and_evaluate_pixel_compile_unit(compile_state);

    compile_state.get_pixel_compile_unit() = end_compile_unit;
    this->compile_and_evaluate_pixel_compile_unit(compile_state);

    /* No need to continue, the above recursive calls will eventually exist the loop and do the
     * actual compilation. */
    return;
  }

  PixelOperation *operation = create_pixel_operation(context_, compile_state);

  for (DNode node : compile_unit) {
    compile_state.map_node_to_pixel_operation(node, operation);
  }

  map_pixel_operation_inputs_to_their_results(operation, compile_state);

  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(compile_state.get_schedule());

  operation->evaluate();

  compile_state.reset_pixel_compile_unit();
}

void Evaluator::map_pixel_operation_inputs_to_their_results(PixelOperation *operation,
                                                            CompileState &compile_state)
{
  for (const auto item : operation->get_inputs_to_linked_outputs_map().items()) {
    Result &result = compile_state.get_result_from_output_socket(item.value);
    operation->map_input_to_result(item.key, &result);

    /* Correct the reference count of the result in case multiple of the result's outgoing links
     * corresponds to a single input in the pixel operation. See the description of the member
     * inputs_to_reference_counts_map_ variable for more information. */
    const int internal_reference_count = operation->get_internal_input_reference_count(item.key);
    result.decrement_reference_count(internal_reference_count - 1);
  }
}

void Evaluator::cancel_evaluation()
{
  context_.cache_manager().skip_next_reset();
  for (const std::unique_ptr<Operation> &operation : operations_stream_) {
    operation->free_results();
  }
}

}  // namespace blender::compositor
