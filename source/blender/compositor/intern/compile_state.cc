/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

CompileState::CompileState(const Context &context, const VectorSet<const bNode *> &schedule)
    : context_(context), schedule_(schedule)
{
}

const VectorSet<const bNode *> &CompileState::get_schedule()
{
  return schedule_;
}

void CompileState::map_node_to_node_operation(const bNode &node, NodeOperation *operations)
{
  node_operations_.add_new(&node, operations);
}

void CompileState::map_node_to_pixel_operation(const bNode &node, PixelOperation *operations)
{
  pixel_operations_.add_new(&node, operations);
}

Result &CompileState::get_result_from_output_socket(const bNodeSocket &output)
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

void CompileState::add_node_to_pixel_compile_unit(const bNode &node)
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

PixelCompileUnit &CompileState::get_pixel_compile_unit()
{
  return pixel_compile_unit_;
}

bool CompileState::is_pixel_compile_unit_single_value()
{
  return is_pixel_compile_unit_single_value_;
}

void CompileState::reset_pixel_compile_unit()
{
  pixel_compile_unit_.clear();
  pixel_compile_unit_domain_.reset();
}

bool CompileState::should_compile_pixel_compile_unit(const bNode &node)
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

bool CompileState::is_pixel_node_single_value(const bNode &node)
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
      if (input_descriptor.implicit_input == ImplicitInput::None) {
        continue;
      }

      /* Otherwise, it has an implicit input, which is never a single value. */
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

Domain CompileState::compute_pixel_node_domain(const bNode &node)
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
      if (input_descriptor.implicit_input == ImplicitInput::None) {
        continue;
      }

      /* Otherwise, the input has the domain of the implicit input, which is the domain of the
       * compositing region. Notice that the lower the domain priority value is, the higher the
       * priority is, hence the less than comparison. */
      if (input_descriptor.domain_priority < current_domain_priority) {
        node_domain = context_.get_compositing_domain();
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

bool CompileState::pixel_compile_unit_has_too_many_outputs(const bool are_node_previews_needed)
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
      const bool is_operation_output = is_output_linked_to_node_conditioned(
          *output, [&](const bNode &node) {
            return schedule_.contains(&node) && !pixel_compile_unit_.contains(&node);
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

bool CompileState::pixel_compile_unit_has_too_many_inputs()
{
  /* Only GPU and non-single units have input count limitations. */
  if (!context_.use_gpu() || is_pixel_compile_unit_single_value_) {
    return false;
  }

  Set<ImplicitInput> referenced_implicit_inputs;
  Set<const bNodeSocket *> referenced_output_sockets;
  int inputs_count = 0;
  for (const bNode *node : pixel_compile_unit_) {
    for (const bNodeSocket *input : node->input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      const bNodeSocket *output = get_output_linked_to_input(*input);
      if (!output) {
        const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
        if (input_descriptor.implicit_input == ImplicitInput::None) {
          continue;
        }

        /* All implicit inputs of the same type share the same input, and this one was counted
         * before, so no need to count it again. */
        if (referenced_implicit_inputs.contains(input_descriptor.implicit_input)) {
          continue;
        }

        inputs_count++;
        if (inputs_count > ShaderOperation::maximum_inputs_count) {
          return true;
        }

        referenced_implicit_inputs.add_new(input_descriptor.implicit_input);
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

      inputs_count++;
      if (inputs_count > ShaderOperation::maximum_inputs_count) {
        return true;
      }

      referenced_output_sockets.add_new(output);
    }
  }

  return false;
}

}  // namespace blender::compositor
