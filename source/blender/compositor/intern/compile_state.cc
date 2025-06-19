/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

CompileState::CompileState(const Context &context, const Schedule &schedule)
    : context_(context), schedule_(schedule)
{
}

const Schedule &CompileState::get_schedule()
{
  return schedule_;
}

void CompileState::map_node_to_node_operation(DNode node, NodeOperation *operations)
{
  node_operations_.add_new(node, operations);
}

void CompileState::map_node_to_pixel_operation(DNode node, PixelOperation *operations)
{
  pixel_operations_.add_new(node, operations);
}

Result &CompileState::get_result_from_output_socket(DOutputSocket output)
{
  /* The output belongs to a node that was compiled into a standard node operation, so return a
   * reference to the result from that operation using the output identifier. */
  if (node_operations_.contains(output.node())) {
    NodeOperation *operation = node_operations_.lookup(output.node());
    return operation->get_result(output->identifier);
  }

  /* Otherwise, the output belongs to a node that was compiled into a pixel operation, so retrieve
   * the internal identifier of that output and return a reference to the result from that
   * operation using the retrieved identifier. */
  PixelOperation *operation = pixel_operations_.lookup(output.node());
  return operation->get_result(operation->get_output_identifier_from_output_socket(output));
}

void CompileState::add_node_to_pixel_compile_unit(DNode node)
{
  pixel_compile_unit_.add_new(node);

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

bool CompileState::should_compile_pixel_compile_unit(DNode node)
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

int CompileState::compute_pixel_node_operation_outputs_count(DNode node)
{
  const DOutputSocket preview_output = find_preview_output_socket(node);

  int outputs_count = 0;
  for (const bNodeSocket *output : node->output_sockets()) {
    const DOutputSocket doutput{node.context(), output};

    if (!is_socket_available(output)) {
      continue;
    }

    /* If the output is used as the node preview, then an operation output will exist for it. */
    const bool is_preview_output = doutput == preview_output;

    /* If any of the nodes linked to the output are not part of the pixel compile unit but are
     * part of the execution schedule, then an operation output will exist for it. */
    const bool is_operation_output = is_output_linked_to_node_conditioned(
        doutput, [&](DNode node) {
          return schedule_.contains(node) && !pixel_compile_unit_.contains(node);
        });

    if (is_operation_output || is_preview_output) {
      outputs_count += 1;
    }
  }

  return outputs_count;
}

bool CompileState::is_pixel_node_single_value(DNode node)
{
  /* The pixel node is single value when all of its inputs are single values. */
  for (int i = 0; i < node->input_sockets().size(); i++) {
    const DInputSocket input{node.context(), node->input_sockets()[i]};

    if (!is_socket_available(input.bsocket())) {
      continue;
    }

    /* The origin socket is an input, that means the input is unlinked. */
    const DSocket origin = get_input_origin_socket(input);
    if (origin->is_input()) {
      const InputDescriptor origin_descriptor = input_descriptor_from_input_socket(
          origin.bsocket());

      /* The input does not have an implicit input, so it is a single value. */
      if (origin_descriptor.implicit_input == ImplicitInput::None) {
        continue;
      }

      /* Otherwise, it has an implicit input, which is never a single value. */
      return false;
    }

    /* Otherwise, the origin socket is an output, which means it is linked. */
    const DOutputSocket output = DOutputSocket(origin);

    /* If the output belongs to a node that is part of the pixel compile unit and that compile unit
     * is not single value, then the node is not single value. */
    if (pixel_compile_unit_.contains(output.node())) {
      if (is_pixel_compile_unit_single_value_) {
        continue;
      }
      return false;
    }

    const Result &result = get_result_from_output_socket(output);
    if (!result.is_single_value()) {
      return false;
    }
  }

  return true;
}

Domain CompileState::compute_pixel_node_domain(DNode node)
{
  /* Default to an identity domain in case no domain input was found, most likely because all
   * inputs are single values. */
  Domain node_domain = Domain::identity();
  int current_domain_priority = std::numeric_limits<int>::max();

  /* Go over the inputs and find the domain of the non single value input with the highest domain
   * priority. */
  for (int i = 0; i < node->input_sockets().size(); i++) {
    const DInputSocket input{node.context(), node->input_sockets()[i]};

    if (!is_socket_available(input.bsocket())) {
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input.bsocket());

    /* The origin socket is an input, that means the input is unlinked. */
    const DSocket origin = get_input_origin_socket(input);
    if (origin->is_input()) {
      const InputDescriptor origin_descriptor = input_descriptor_from_input_socket(
          origin.bsocket());

      /* The input does not have an implicit input, so it is a single that can't be a domain input
       * and we skip it. */
      if (origin_descriptor.implicit_input == ImplicitInput::None) {
        continue;
      }

      /* Otherwise, the input has the domain of the implicit input, which is the domain of the
       * compositing region. Notice that the lower the domain priority value is, the higher the
       * priority is, hence the less than comparison. */
      if (input_descriptor.domain_priority < current_domain_priority) {
        node_domain = Domain(context_.get_compositing_region_size());
        current_domain_priority = input_descriptor.domain_priority;
      }
      continue;
    }

    /* Otherwise, the origin socket is an output, which means it is linked. */
    const DOutputSocket output = DOutputSocket(origin);

    /* If the output belongs to a node that is part of the pixel compile unit, then the domain of
     * the input is the domain of the compile unit itself. */
    if (pixel_compile_unit_.contains(output.node())) {
      /* Notice that the lower the domain priority value is, the higher the priority is, hence the
       * less than comparison. */
      if (input_descriptor.domain_priority < current_domain_priority) {
        node_domain = pixel_compile_unit_domain_.value();
        current_domain_priority = input_descriptor.domain_priority;
      }
      continue;
    }

    const Result &result = get_result_from_output_socket(output);

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

}  // namespace blender::compositor
