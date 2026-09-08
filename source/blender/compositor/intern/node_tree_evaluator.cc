/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_compute_context.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_group_input_node_operation.hh"
#include "COM_group_node_operation.hh"
#include "COM_group_output_node_operation.hh"
#include "COM_implicit_input_operation.hh"
#include "COM_input_descriptor.hh"
#include "COM_multi_function_procedure_operation.hh"
#include "COM_node_operation.hh"
#include "COM_node_tree_evaluator.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"
#include "COM_single_value_node_input_operation.hh"
#include "COM_undefined_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

NodeTreeEvaluator::NodeTreeEvaluator(Context &context,
                                     const Schedule &schedule,
                                     Operation &operation,
                                     const ComputeContext &compute_context)
    : context_(context),
      schedule_(schedule),
      operation_(operation),
      compute_context_(compute_context)
{
}

void NodeTreeEvaluator::evaluate()
{
  for (const bNode *node : this->schedule().nodes) {
    if (this->context().is_canceled()) {
      this->cancel_evaluation();
      break;
    }

    if (this->should_compile_pixel_compile_unit(*node)) {
      this->evaluate_pixel_compile_unit();
    }

    if (is_pixel_node(*node)) {
      this->add_node_to_pixel_compile_unit(*node);
    }
    else {
      this->evaluate_node(*node);
    }
  }
}

Result &NodeTreeEvaluator::get_result_from_output_socket(const bNodeSocket &output)
{
  /* The output belongs to a node that was compiled into a standard node operation, so return a
   * reference to the result from that operation using the output identifier. */
  if (node_operations_.contains(&output.owner_node())) {
    NodeOperation *operation = node_operations_.lookup(&output.owner_node());
    return operation->get_result(output.identifier);
  }

  /* Otherwise, the output belongs to a node that was compiled into a pixel operation, so retrieve
   * the internal identifier of that output and return a reference to the result from that
   * operation using the retrieved identifier. */
  PixelOperation *operation = pixel_operations_.lookup(&output.owner_node());
  return operation->get_result(operation->get_output_identifier_from_output_socket(output));
}

PixelCompileUnit &NodeTreeEvaluator::pixel_compile_unit()
{
  return pixel_compile_unit_;
}

const Schedule &NodeTreeEvaluator::schedule()
{
  return schedule_;
}

void NodeTreeEvaluator::evaluate_node(const bNode &node)
{
  NodeOperation *operation = this->create_node_operation(node);
  operation->set_compute_context(compute_context_);

  this->map_node_to_node_operation(node, operation);

  map_node_operation_inputs_to_their_results(node, operation);

  /* This has to be done after input mapping because the method may add Input Single Value
   * Operations to the operations stream, which needs to be evaluated before the operation itself
   * is evaluated. */
  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(this->schedule());

  operation->evaluate();
}

NodeOperation *NodeTreeEvaluator::create_node_operation(const bNode &node)
{
  const char *disabled_hint = nullptr;
  if (!node.typeinfo->poll(node.typeinfo, &node.owner_tree(), &disabled_hint)) {
    return get_undefined_node_operation(this->context(), node);
  }

  if (node.is_group()) {
    return get_group_node_operation(this->context(), node);
  }

  if (node.is_group_output()) {
    return get_group_output_node_operation(this->context(), node, this->operation());
  }

  if (node.is_group_input()) {
    return get_group_input_node_operation(this->context(), node, this->operation());
  }

  return node.typeinfo->get_compositor_operation(this->context(), node);
}

void NodeTreeEvaluator::map_node_operation_inputs_to_their_results(const bNode &node,
                                                                   NodeOperation *operation)
{
  for (const bNodeSocket *input : node.input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const bNodeSocket *output = get_output_linked_to_input(*input);
    if (output && this->schedule().nodes.contains(&output->owner_node()) &&
        !this->schedule().unneeded_inputs.contains(input))
    {
      /* The input is linked to a node that is part of the schedule. So map the input to the result
       * we get from the output. */
      Result &result = this->get_result_from_output_socket(*output);
      operation->map_input_to_result(input->identifier, &result);
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
    if (!input_descriptor.implicit_input.has_value()) {
      /* The input is unlinked with no implicit value. So map the input to the result of a newly
       * created Input Single Value Operation. */
      SingleValueNodeInputOperation *input_operation = new SingleValueNodeInputOperation(
          this->context(), *input);
      operations_stream_.append(std::unique_ptr<SingleValueNodeInputOperation>(input_operation));
      input_operation->evaluate();
      operation->map_input_to_result(input->identifier, &input_operation->get_result());
      continue;
    }

    ImplicitInputOperation *input_operation = new ImplicitInputOperation(
        this->context(), input_descriptor.implicit_input.value());
    operations_stream_.append(std::unique_ptr<ImplicitInputOperation>(input_operation));
    input_operation->evaluate();
    operation->map_input_to_result(input->identifier, &input_operation->get_result());
  }
}

PixelOperation *NodeTreeEvaluator::create_pixel_operation()
{
  /* Use multi-function procedure to execute the pixel compile unit for CPU contexts or if the
   * compile unit is single value and would thus be more efficient to execute on the CPU. */
  const bool is_single_value = this->is_pixel_compile_unit_single_value();
  if (!this->context().use_gpu() || is_single_value) {
    return new MultiFunctionProcedureOperation(
        this->context(), *this, is_single_value, compute_context_);
  }

  return new ShaderOperation(this->context(), *this, compute_context_);
}

void NodeTreeEvaluator::evaluate_pixel_compile_unit()
{
  PixelCompileUnit &compile_unit = this->pixel_compile_unit();

  /* Only compute previews if they are needed and the node group is currently active. */
  const bool needs_node_previews = flag_is_set(this->context().needed_side_effect_output_types(),
                                               SideEffectOutputTypes::NodePreviews);
  const bool is_active_context = compute_context_.hash() ==
                                 this->context().get_active_compute_context_hash();
  const bool are_node_previews_needed = needs_node_previews && is_active_context;

  /* Pixel operations might have limitations on the number of outputs or inputs they can have, so
   * we might have to split the compile unit into smaller units to workaround this limitation. In
   * practice, splitting will almost always never happen due to the scheduling strategy we use, so
   * the base case remains fast. */
  if (compile_unit.size() > 1 &&
      (this->pixel_compile_unit_has_too_many_outputs(are_node_previews_needed) ||
       this->pixel_compile_unit_has_too_many_inputs()))
  {
    const int split_index = compile_unit.size() / 2;
    const PixelCompileUnit start_compile_unit(compile_unit.as_span().take_front(split_index));
    const PixelCompileUnit end_compile_unit(compile_unit.as_span().drop_front(split_index));

    this->pixel_compile_unit() = start_compile_unit;
    this->evaluate_pixel_compile_unit();

    this->pixel_compile_unit() = end_compile_unit;
    this->evaluate_pixel_compile_unit();

    /* No need to continue, the above recursive calls will eventually exist the loop and do the
     * actual compilation. */
    return;
  }

  PixelOperation *operation = this->create_pixel_operation();

  for (const bNode *node : compile_unit) {
    this->map_node_to_pixel_operation(*node, operation);
  }

  map_pixel_operation_inputs_to_their_results(operation);

  operations_stream_.append(std::unique_ptr<Operation>(operation));

  operation->compute_results_reference_counts(this->schedule());

  operation->evaluate();

  this->reset_pixel_compile_unit();
}

void NodeTreeEvaluator::map_pixel_operation_inputs_to_their_results(PixelOperation *operation)
{
  for (const auto item : operation->get_inputs_to_linked_outputs_map().items()) {
    const bNodeSocket &output = *item.value;
    const StringRef input_identifier = item.key;

    Result *input_result = &this->get_result_from_output_socket(output);
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

void NodeTreeEvaluator::map_node_to_node_operation(const bNode &node, NodeOperation *operation)
{
  node_operations_.add_new(&node, operation);
}

void NodeTreeEvaluator::map_node_to_pixel_operation(const bNode &node, PixelOperation *operation)
{
  pixel_operations_.add_new(&node, operation);
}

void NodeTreeEvaluator::add_node_to_pixel_compile_unit(const bNode &node)
{
  pixel_compile_unit_.add_new(&node);

  /* If this is the first node in the compile unit, then we should initialize the single value
   * type, as well as the domain in case the node was not single value. */
  const bool is_first_node_in_operation = pixel_compile_unit_.size() == 1;
  if (is_first_node_in_operation) {
    is_pixel_compile_unit_single_value_ = this->is_pixel_node_single_value(node);

    /* If the node was not a single value, compute and initialize the domain. */
    if (!is_pixel_compile_unit_single_value_) {
      pixel_compile_unit_domain_ = this->compute_pixel_node_domain(node);
    }
  }
}

bool NodeTreeEvaluator::is_pixel_compile_unit_single_value()
{
  return is_pixel_compile_unit_single_value_;
}

void NodeTreeEvaluator::reset_pixel_compile_unit()
{
  pixel_compile_unit_.clear();
  pixel_compile_unit_domain_.reset();
}

bool NodeTreeEvaluator::should_compile_pixel_compile_unit(const bNode &node)
{
  /* If the pixel compile unit is empty, then it can't be compiled yet. */
  if (pixel_compile_unit_.is_empty()) {
    return false;
  }

  /* If the node is not a pixel node, then it can't be added to the pixel compile unit and the
   * pixel compile unit is considered complete and should be compiled. */
  if (!is_pixel_node(node)) {
    return true;
  }

  /* If the compile unit is single value and the given node is not or vice versa, then it can't be
   * added to the pixel compile unit and the pixel compile unit is considered complete and should
   * be compiled. */
  if (is_pixel_compile_unit_single_value_ != this->is_pixel_node_single_value(node)) {
    return true;
  }

  /* For non single value compile units, if the computed domain of the node doesn't matches the
   * domain of the pixel compile unit, then it can't be added to the pixel compile unit and the
   * pixel compile unit is considered complete and should be compiled. */
  if (!is_pixel_compile_unit_single_value_) {
    if (pixel_compile_unit_domain_.value() != this->compute_pixel_node_domain(node)) {
      return true;
    }
  }

  /* Otherwise, the node is compatible and can be added to the compile unit and it shouldn't be
   * compiled just yet. */
  return false;
}

bool NodeTreeEvaluator::is_pixel_node_single_value(const bNode &node)
{
  /* If any of the outputs are single-only outputs, then the node is operating on single values. */
  for (const bNodeSocket *output : node.output_sockets()) {
    if (!is_socket_available(output)) {
      continue;
    }

    if (Result::is_single_value_only_type(get_node_socket_result_type(output))) {
      return true;
    }
  }

  /* If any of the inputs are single-only outputs, then the node is operating on single values. */
  for (const bNodeSocket *input : node.input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    if (Result::is_single_value_only_type(get_node_socket_result_type(input))) {
      return true;
    }
  }

  /* The pixel node is single value when all of its inputs are single values. */
  for (const bNodeSocket *input : node.input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const bNodeSocket *output = get_output_linked_to_input(*input);
    if (!output) {
      /* The input does not have an implicit input, so it is a single value. */
      const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
      if (!input_descriptor.implicit_input.has_value()) {
        continue;
      }

      const std::optional<Domain> domain = ImplicitInputOperation::get_domain(
          context_, input_descriptor.implicit_input.value());
      if (!domain.has_value()) {
        /* The input has an implicit input, but it is a single value. */
        continue;
      }

      /* Otherwise, it has an non-single-value implicit input. */
      return false;
    }

    /* If the output belongs to a node that is part of the pixel compile unit and that compile unit
     * is not single value, then the node is not single value. */
    if (pixel_compile_unit_.contains(&output->owner_node())) {
      if (is_pixel_compile_unit_single_value_) {
        continue;
      }
      return false;
    }

    const Result &result = get_result_from_output_socket(*output);
    if (!result.is_single_value()) {
      return false;
    }
  }

  return true;
}

Domain NodeTreeEvaluator::compute_pixel_node_domain(const bNode &node)
{
  /* Default to an identity domain in case no domain input was found, most likely because all
   * inputs are single values. */
  Domain node_domain = Domain::identity();
  int current_domain_priority = std::numeric_limits<int>::max();

  /* Go over the inputs and find the domain of the non single value input with the highest domain
   * priority. */
  for (const bNodeSocket *input : node.input_sockets()) {
    if (!is_socket_available(input)) {
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);

    const bNodeSocket *output = get_output_linked_to_input(*input);
    if (!output) {
      /* The input does not have an implicit input, so it is a single that can't be a domain input
       * and we skip it. */
      if (!input_descriptor.implicit_input.has_value()) {
        continue;
      }

      const std::optional<Domain> domain = ImplicitInputOperation::get_domain(
          context_, input_descriptor.implicit_input.value());
      if (!domain.has_value()) {
        /* The input has an implicit input, but it is a single value that can't be a domain input
         * and we skip it. */
        continue;
      }

      /* Otherwise, the input has the domain of the implicit input, which is the domain of the
       * compositing region. Notice that the lower the domain priority value is, the higher the
       * priority is, hence the less than comparison. */
      if (input_descriptor.domain_priority < current_domain_priority) {
        node_domain = domain.value();
        current_domain_priority = input_descriptor.domain_priority;
      }
      continue;
    }

    /* If the output belongs to a node that is part of the pixel compile unit, then the domain of
     * the input is the domain of the compile unit itself. */
    if (pixel_compile_unit_.contains(&output->owner_node())) {
      /* Notice that the lower the domain priority value is, the higher the priority is, hence the
       * less than comparison. */
      if (input_descriptor.domain_priority < current_domain_priority) {
        node_domain = pixel_compile_unit_domain_.value();
        current_domain_priority = input_descriptor.domain_priority;
      }
      continue;
    }

    const Result &result = get_result_from_output_socket(*output);

    /* A single value input can't be a domain input. */
    if (result.is_single_value() || input_descriptor.expects_single_value) {
      continue;
    }

    /* An input that skips operation domain realization can't be a domain input. */
    if (input_descriptor.realization_mode != InputRealizationMode::OperationDomain) {
      continue;
    }

    /* Notice that the lower the domain priority value is, the higher the priority is, hence the
     * less than comparison. */
    if (input_descriptor.domain_priority < current_domain_priority) {
      node_domain = result.domain();
      current_domain_priority = input_descriptor.domain_priority;
    }
  }

  return node_domain;
}

bool NodeTreeEvaluator::pixel_compile_unit_has_too_many_outputs(
    const bool are_node_previews_needed)
{
  /* Only GPU and non-single units have output count limitations. */
  if (!context_.use_gpu() || is_pixel_compile_unit_single_value_) {
    return false;
  }

  int outputs_count = 0;
  for (const bNode *node : pixel_compile_unit_) {
    const bNodeSocket *preview_output = are_node_previews_needed ?
                                            find_preview_output_socket(*node) :
                                            nullptr;

    for (const bNodeSocket *output : node->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      /* If the output is used as the node preview, then an operation output will exist for it. */
      const bool is_preview_output = output == preview_output;

      /* If any of the nodes linked to the output are not part of the pixel compile unit but are
       * part of the execution schedule, then an operation output will exist for it. */
      const bool is_operation_output = is_output_linked_to_input_conditioned(
          *output, [&](const bNodeSocket &input) {
            return schedule_.nodes.contains(&input.owner_node()) &&
                   !schedule_.unneeded_inputs.contains(&input) &&
                   !pixel_compile_unit_.contains(&input.owner_node());
          });

      if (is_operation_output || is_preview_output) {
        outputs_count += 1;
      }

      if (outputs_count > ShaderOperation::maximum_outputs_count) {
        return true;
      }
    }
  }

  return false;
}

bool NodeTreeEvaluator::pixel_compile_unit_has_too_many_inputs()
{
  /* Only GPU and non-single units have input count limitations. */
  if (!context_.use_gpu() || is_pixel_compile_unit_single_value_) {
    return false;
  }

  Set<ImplicitInputType> referenced_implicit_inputs;
  Set<const bNodeSocket *> referenced_output_sockets;
  int inputs_count = 0;
  for (const bNode *node : pixel_compile_unit_) {
    for (const bNodeSocket *input : node->input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      if (this->schedule().unneeded_inputs.contains(input)) {
        continue;
      }

      const bNodeSocket *output = get_output_linked_to_input(*input);
      if (!output) {
        const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
        if (!input_descriptor.implicit_input.has_value()) {
          continue;
        }

        /* All implicit inputs of the same type share the same input, and this one was counted
         * before, so no need to count it again. */
        if (referenced_implicit_inputs.contains(input_descriptor.implicit_input.value())) {
          continue;
        }

        inputs_count++;
        if (inputs_count > ShaderOperation::maximum_inputs_count) {
          return true;
        }

        referenced_implicit_inputs.add_new(input_descriptor.implicit_input.value());
        continue;
      }

      /* This output is part of the pixel compile unit, so no input is declared for it. */
      if (pixel_compile_unit_.contains(&output->owner_node())) {
        continue;
      }

      /* All inputs linked to the same output share the same input, and this one was counted
       * before, so no need to count it again. */
      if (referenced_output_sockets.contains(output)) {
        continue;
      }

      /* Single values are folded into the shader so they do not count toward inputs count. */
      const Result &result = this->get_result_from_output_socket(*output);
      if (result.is_single_value()) {
        continue;
      }

      inputs_count++;
      if (inputs_count > ShaderOperation::maximum_inputs_count) {
        return true;
      }

      referenced_output_sockets.add_new(output);
    }
  }

  return false;
}

void NodeTreeEvaluator::cancel_evaluation()
{
  for (const std::unique_ptr<Operation> &operation : operations_stream_) {
    operation->free_results();
  }
}

Context &NodeTreeEvaluator::context()
{
  return context_;
}

Operation &NodeTreeEvaluator::operation()
{
  return operation_;
}

}  // namespace blender::compositor
