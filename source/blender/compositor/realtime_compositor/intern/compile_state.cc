/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "COM_compile_state.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

CompileState::CompileState(const Schedule &schedule) : schedule_(schedule) {}

const Schedule &CompileState::get_schedule()
{
  return schedule_;
}

void CompileState::map_node_to_node_operation(DNode node, NodeOperation *operations)
{
  return node_operations_.add_new(node, operations);
}

void CompileState::map_node_to_shader_operation(DNode node, ShaderOperation *operations)
{
  return shader_operations_.add_new(node, operations);
}

Result &CompileState::get_result_from_output_socket(DOutputSocket output)
{
  /* The output belongs to a node that was compiled into a standard node operation, so return a
   * reference to the result from that operation using the output identifier. */
  if (node_operations_.contains(output.node())) {
    NodeOperation *operation = node_operations_.lookup(output.node());
    return operation->get_result(output->identifier);
  }

  /* Otherwise, the output belongs to a node that was compiled into a shader operation, so
   * retrieve the internal identifier of that output and return a reference to the result from
   * that operation using the retrieved identifier. */
  ShaderOperation *operation = shader_operations_.lookup(output.node());
  return operation->get_result(operation->get_output_identifier_from_output_socket(output));
}

void CompileState::add_node_to_shader_compile_unit(DNode node)
{
  shader_compile_unit_.add_new(node);

  /* If the domain of the shader compile unit is not yet determined or was determined to be
   * an identity domain, update it to be the computed domain of the node. */
  if (shader_compile_unit_domain_ == Domain::identity()) {
    shader_compile_unit_domain_ = compute_shader_node_domain(node);
  }
}

ShaderCompileUnit &CompileState::get_shader_compile_unit()
{
  return shader_compile_unit_;
}

void CompileState::reset_shader_compile_unit()
{
  shader_compile_unit_.clear();
}

bool CompileState::should_compile_shader_compile_unit(DNode node)
{
  /* If the shader compile unit is empty, then it can't be compiled yet. */
  if (shader_compile_unit_.is_empty()) {
    return false;
  }

  /* If the node is not a shader node, then it can't be added to the shader compile unit and the
   * shader compile unit is considered complete and should be compiled. */
  if (!is_shader_node(node)) {
    return true;
  }

  /* If the computed domain of the node doesn't matches the domain of the shader compile unit, then
   * it can't be added to the shader compile unit and the shader compile unit is considered
   * complete and should be compiled. Identity domains are an exception as they are always
   * compatible because they represents single values. */
  if (shader_compile_unit_domain_ != Domain::identity() &&
      shader_compile_unit_domain_ != compute_shader_node_domain(node))
  {
    return true;
  }

  /* Otherwise, the node is compatible and can be added to the compile unit and it shouldn't be
   * compiled just yet. */
  return false;
}

Domain CompileState::compute_shader_node_domain(DNode node)
{
  /* Default to an identity domain in case no domain input was found, most likely because all
   * inputs are single values. */
  Domain node_domain = Domain::identity();
  int current_domain_priority = std::numeric_limits<int>::max();

  /* Go over the inputs and find the domain of the non single value input with the highest domain
   * priority. */
  for (const bNodeSocket *input : node->input_sockets()) {
    const DInputSocket dinput{node.context(), input};

    /* Get the output linked to the input. If it is null, that means the input is unlinked, so skip
     * it. */
    const DOutputSocket output = get_output_linked_to_input(dinput);
    if (!output) {
      continue;
    }

    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);

    /* If the output belongs to a node that is part of the shader compile unit, then the domain of
     * the input is the domain of the compile unit itself. */
    if (shader_compile_unit_.contains(output.node())) {
      /* Single value inputs can't be domain inputs. */
      if (shader_compile_unit_domain_.size == int2(1)) {
        continue;
      }

      /* Notice that the lower the domain priority value is, the higher the priority is, hence the
       * less than comparison. */
      if (input_descriptor.domain_priority < current_domain_priority) {
        node_domain = shader_compile_unit_domain_;
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
    if (!input_descriptor.realization_options.realize_on_operation_domain) {
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

}  // namespace blender::realtime_compositor
