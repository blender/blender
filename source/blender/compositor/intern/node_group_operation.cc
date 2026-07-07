/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_group_input_node_operation.hh"
#include "COM_group_node_operation.hh"
#include "COM_group_output_node_operation.hh"
#include "COM_implicit_input_operation.hh"
#include "COM_input_descriptor.hh"
#include "COM_multi_function_procedure_operation.hh"
#include "COM_node_group_operation.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"
#include "COM_single_value_node_input_operation.hh"
#include "COM_undefined_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

NodeGroupOperation::NodeGroupOperation(Context &context,
                                       const bNodeTree &node_group,
                                       const NodeGroupOutputTypes needed_outputs,
                                       Map<bNodeInstanceKey, bke::bNodePreview> *node_previews,
                                       const bNodeInstanceKey active_node_group_instance_key,
                                       const bNodeInstanceKey instance_key)
    : Operation(context),
      node_group_(node_group),
      needed_output_types_(needed_outputs),
      node_previews_(node_previews),
      active_node_group_instance_key_(active_node_group_instance_key),
      instance_key_(instance_key)
{
  node_group.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *input : node_group.interface_inputs()) {
    const InputDescriptor input_descriptor = input_descriptor_from_interface_input(node_group,
                                                                                   *input);
    this->declare_input_descriptor(input->identifier, input_descriptor);
  }

  for (const bNodeTreeInterfaceSocket *output : node_group.interface_outputs()) {
    const ResultType result_type = get_node_interface_socket_result_type(*output);
    this->populate_result(output->identifier, context.create_result(result_type));
  }
}

void NodeGroupOperation::execute()
{
  Set<StringRef> needed_outputs;
  for (const bNodeTreeInterfaceSocket *output : node_group_.interface_outputs()) {
    if (this->get_result(output->identifier).should_compute()) {
      needed_outputs.add_new(output->identifier);
    }
  }

  const VectorSet<const bNode *> schedule = compute_schedule(this->context(),
                                                             node_group_,
                                                             needed_output_types_,
                                                             needed_outputs,
                                                             instance_key_,
                                                             active_node_group_instance_key_);
  CompileState compile_state(this->context(), schedule);

  for (const bNode *node : schedule) {
    if (this->context().is_canceled()) {
      this->cancel_evaluation();
      break;
    }

    if (compile_state.should_compile_pixel_compile_unit(*node)) {
      this->evaluate_pixel_compile_unit(compile_state);
    }

    if (is_pixel_node(*node)) {
      compile_state.add_node_to_pixel_compile_unit(*node);
    }
    else {
      this->evaluate_node(*node, compile_state);
    }
  }

  /* Allocate outputs as invalid if they are not allocated already and are needed. This could
   * happen for instance when no Group Output node exist or when the evaluation gets canceled
   * before the output is written. */
  for (const bNodeTreeInterfaceSocket *output : node_group_.interface_outputs()) {
    Result &result = this->get_result(output->identifier);
    if (!result.is_allocated() && result.should_compute()) {
      result.allocate_invalid();
    }
  }
}

void NodeGroupOperation::evaluate_node(const bNode &node, CompileState &compile_state)
{
  NodeOperation *operation = this->get_node_operation(node);
  operation->set_instance_key(bke::node_instance_key(instance_key_, &node_group_, &node));

  /* Only set previews if the node group is currently being viewed. Except if the node is a group
   * node, because a child node group might be the active one. */
  if (node.is_group() || instance_key_ == active_node_group_instance_key_) {
    operation->set_node_previews(node_previews_);
  }

  compile_state.map_node_to_node_operation(node, operation);

  map_node_operation_inputs_to_their_results(node, operation, compile_state);

  /* This has to be done after input mapping because the method may add Input Single Value
   * Operations to the operations stream, which needs to be evaluated before the operation itself
   * is evaluated. */
  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(compile_state.get_schedule());

  operation->evaluate();
}

NodeOperation *NodeGroupOperation::get_node_operation(const bNode &node)
{
  const char *disabled_hint = nullptr;
  if (!node.typeinfo->poll(node.typeinfo, &node.owner_tree(), &disabled_hint)) {
    return get_undefined_node_operation(this->context(), node);
  }

  if (node.is_group()) {
    return get_group_node_operation(
        this->context(), node, needed_output_types_, active_node_group_instance_key_);
  }

  if (node.is_group_output()) {
    return get_group_output_node_operation(this->context(), node, *this);
  }

  if (node.is_group_input()) {
    return get_group_input_node_operation(this->context(), node, *this);
  }

  return node.typeinfo->get_compositor_operation(this->context(), node);
}

void NodeGroupOperation::map_node_operation_inputs_to_their_results(const bNode &node,
                                                                    NodeOperation *operation,
                                                                    CompileState &compile_state)
{
  for (const bNodeSocket *input : node.input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const bNodeSocket *output = get_output_linked_to_input(*input);
    if (output && compile_state.get_schedule().contains(&output->owner_node())) {
      /* The input is linked to a node that is part of the schedule. So map the input to the result
       * we get from the output. */
      Result &result = compile_state.get_result_from_output_socket(*output);
      operation->map_input_to_result(input->identifier, &result);
      continue;
    }

    /* Otherwise, the input is essentially unlinked. So map the input to the result of a newly
     * created Input Single Value Operation. */
    SingleValueNodeInputOperation *input_operation = new SingleValueNodeInputOperation(
        this->context(), *input);
    operations_stream_.append(std::unique_ptr<SingleValueNodeInputOperation>(input_operation));
    input_operation->evaluate();
    operation->map_input_to_result(input->identifier, &input_operation->get_result());
  }
}

/* Create one of the concrete subclasses of the PixelOperation based on the context and compile
 * state. Deleting the operation is the caller's responsibility. */
static PixelOperation *create_pixel_operation(Context &context, CompileState &compile_state)
{
  const VectorSet<const bNode *> &schedule = compile_state.get_schedule();
  PixelCompileUnit &compile_unit = compile_state.get_pixel_compile_unit();

  /* Use multi-function procedure to execute the pixel compile unit for CPU contexts or if the
   * compile unit is single value and would thus be more efficient to execute on the CPU. */
  const bool is_single_value = compile_state.is_pixel_compile_unit_single_value();
  if (!context.use_gpu() || is_single_value) {
    return new MultiFunctionProcedureOperation(context, compile_unit, schedule, is_single_value);
  }

  return new ShaderOperation(context, compile_unit, schedule);
}

void NodeGroupOperation::evaluate_pixel_compile_unit(CompileState &compile_state)
{
  PixelCompileUnit &compile_unit = compile_state.get_pixel_compile_unit();

  /* Pixel operations might have limitations on the number of outputs or inputs they can have, so
   * we might have to split the compile unit into smaller units to workaround this limitation. In
   * practice, splitting will almost always never happen due to the scheduling strategy we use, so
   * the base case remains fast. */
  const bool are_node_previews_needed = instance_key_ == active_node_group_instance_key_;
  if (compile_state.pixel_compile_unit_has_too_many_outputs(are_node_previews_needed) ||
      compile_state.pixel_compile_unit_has_too_many_inputs())
  {
    const int split_index = compile_unit.size() / 2;
    const PixelCompileUnit start_compile_unit(compile_unit.as_span().take_front(split_index));
    const PixelCompileUnit end_compile_unit(compile_unit.as_span().drop_front(split_index));

    compile_state.get_pixel_compile_unit() = start_compile_unit;
    this->evaluate_pixel_compile_unit(compile_state);

    compile_state.get_pixel_compile_unit() = end_compile_unit;
    this->evaluate_pixel_compile_unit(compile_state);

    /* No need to continue, the above recursive calls will eventually exist the loop and do the
     * actual compilation. */
    return;
  }

  PixelOperation *operation = create_pixel_operation(this->context(), compile_state);
  operation->set_instance_key(instance_key_);

  /* Only compute previews if the node group is active. */
  if (instance_key_ == active_node_group_instance_key_) {
    operation->set_node_previews(node_previews_);
  }

  for (const bNode *node : compile_unit) {
    compile_state.map_node_to_pixel_operation(*node, operation);
  }

  map_pixel_operation_inputs_to_their_results(operation, compile_state);

  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(compile_state.get_schedule());

  operation->evaluate();

  compile_state.reset_pixel_compile_unit();
}

void NodeGroupOperation::map_pixel_operation_inputs_to_their_results(PixelOperation *operation,
                                                                     CompileState &compile_state)
{
  for (const auto item : operation->get_inputs_to_linked_outputs_map().items()) {
    const bNodeSocket &output = *item.value;
    const StringRef input_identifier = item.key;

    Result *input_result = &compile_state.get_result_from_output_socket(output);
    operation->map_input_to_result(input_identifier, input_result);

    /* Correct the reference count of the result in case multiple of the result's outgoing links
     * corresponds to a single input in the pixel operation. See the description of the member
     * inputs_to_reference_counts_map_ variable for more information. */
    const int internal_reference_count = operation->get_internal_input_reference_count(
        input_identifier);
    input_result->decrement_reference_count(internal_reference_count - 1);
  }

  for (const auto item : operation->get_implicit_inputs_to_input_identifiers_map().items()) {
    ImplicitInputOperation *input_operation = new ImplicitInputOperation(this->context(),
                                                                         item.key);
    operation->map_input_to_result(item.value, &input_operation->get_result());

    operations_stream_.append(std::unique_ptr<ImplicitInputOperation>(input_operation));

    input_operation->evaluate();
  }
}

void NodeGroupOperation::cancel_evaluation()
{
  for (const std::unique_ptr<Operation> &operation : operations_stream_) {
    operation->free_results();
  }
}

}  // namespace blender::compositor
