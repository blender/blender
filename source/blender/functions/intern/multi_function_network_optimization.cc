/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup fn
 */

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "BLI_stack.hh"

namespace blender::fn::mf_network_optimization {

/* -------------------------------------------------------------------- */
/** \name Utility functions to find nodes in a network.
 *
 * \{ */

static bool set_tag_and_check_if_modified(bool &tag, bool new_value)
{
  if (tag != new_value) {
    tag = new_value;
    return true;
  }
  else {
    return false;
  }
}

static Array<bool> mask_nodes_to_the_left(MFNetwork &network, Span<MFNode *> nodes)
{
  Array<bool> is_to_the_left(network.node_id_amount(), false);
  Stack<MFNode *> nodes_to_check;

  for (MFNode *node : nodes) {
    is_to_the_left[node->id()] = true;
    nodes_to_check.push(node);
  }

  while (!nodes_to_check.is_empty()) {
    MFNode &node = *nodes_to_check.pop();

    for (MFInputSocket *input_socket : node.inputs()) {
      MFOutputSocket *origin = input_socket->origin();
      if (origin != nullptr) {
        MFNode &origin_node = origin->node();
        if (set_tag_and_check_if_modified(is_to_the_left[origin_node.id()], true)) {
          nodes_to_check.push(&origin_node);
        }
      }
    }
  }

  return is_to_the_left;
}

static Array<bool> mask_nodes_to_the_right(MFNetwork &network, Span<MFNode *> nodes)
{
  Array<bool> is_to_the_right(network.node_id_amount(), false);
  Stack<MFNode *> nodes_to_check;

  for (MFNode *node : nodes) {
    is_to_the_right[node->id()] = true;
    nodes_to_check.push(node);
  }

  while (!nodes_to_check.is_empty()) {
    MFNode &node = *nodes_to_check.pop();

    for (MFOutputSocket *output_socket : node.outputs()) {
      for (MFInputSocket *target_socket : output_socket->targets()) {
        MFNode &target_node = target_socket->node();
        if (set_tag_and_check_if_modified(is_to_the_right[target_node.id()], true)) {
          nodes_to_check.push(&target_node);
        }
      }
    }
  }

  return is_to_the_right;
}

static Vector<MFNode *> find_nodes_based_on_mask(MFNetwork &network,
                                                 Span<bool> id_mask,
                                                 bool mask_value)
{
  Vector<MFNode *> nodes;
  for (uint id : id_mask.index_range()) {
    if (id_mask[id] == mask_value) {
      MFNode *node = network.node_or_null_by_id(id);
      if (node != nullptr) {
        nodes.append(node);
      }
    }
  }
  return nodes;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dead Node Removal
 *
 * \{ */

/**
 * Unused nodes are all those nodes that no dummy node depends upon.
 */
void dead_node_removal(MFNetwork &network)
{
  Array<bool> node_is_used_mask = mask_nodes_to_the_left(network, network.dummy_nodes());
  Vector<MFNode *> nodes_to_remove = find_nodes_based_on_mask(network, node_is_used_mask, false);
  network.remove(nodes_to_remove);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constant Folding
 *
 * \{ */

static bool output_has_non_constant_target_node(MFOutputSocket *output_socket,
                                                Span<bool> is_not_constant_mask)
{
  for (MFInputSocket *target_socket : output_socket->targets()) {
    MFNode &target_node = target_socket->node();
    bool target_is_not_constant = is_not_constant_mask[target_node.id()];
    if (target_is_not_constant) {
      return true;
    }
  }
  return false;
}

static MFInputSocket *try_find_dummy_target_socket(MFOutputSocket *output_socket)
{
  for (MFInputSocket *target_socket : output_socket->targets()) {
    if (target_socket->node().is_dummy()) {
      return target_socket;
    }
  }
  return nullptr;
}

static Vector<MFInputSocket *> find_constant_inputs_to_fold(
    MFNetwork &network, Vector<MFDummyNode *> &r_temporary_nodes)
{
  Span<MFNode *> non_constant_nodes = network.dummy_nodes();
  Array<bool> is_not_constant_mask = mask_nodes_to_the_right(network, non_constant_nodes);
  Vector<MFNode *> constant_nodes = find_nodes_based_on_mask(network, is_not_constant_mask, false);

  Vector<MFInputSocket *> sockets_to_compute;
  for (MFNode *node : constant_nodes) {
    if (node->inputs().size() == 0) {
      continue;
    }

    for (MFOutputSocket *output_socket : node->outputs()) {
      MFDataType data_type = output_socket->data_type();
      if (output_has_non_constant_target_node(output_socket, is_not_constant_mask)) {
        MFInputSocket *dummy_target = try_find_dummy_target_socket(output_socket);
        if (dummy_target == nullptr) {
          dummy_target = &network.add_output("Dummy", data_type);
          network.add_link(*output_socket, *dummy_target);
          r_temporary_nodes.append(&dummy_target->node().as_dummy());
        }

        sockets_to_compute.append(dummy_target);
      }
    }
  }
  return sockets_to_compute;
}

static void prepare_params_for_constant_folding(const MultiFunction &network_fn,
                                                MFParamsBuilder &params,
                                                ResourceCollector &resources)
{
  for (uint param_index : network_fn.param_indices()) {
    MFParamType param_type = network_fn.param_type(param_index);
    MFDataType data_type = param_type.data_type();

    switch (data_type.category()) {
      case MFDataType::Single: {
        /* Allocates memory for a single constant folded value. */
        const CPPType &cpp_type = data_type.single_type();
        void *buffer = resources.linear_allocator().allocate(cpp_type.size(),
                                                             cpp_type.alignment());
        GMutableSpan array{cpp_type, buffer, 1};
        params.add_uninitialized_single_output(array);
        break;
      }
      case MFDataType::Vector: {
        /* Allocates memory for a constant folded vector. */
        const CPPType &cpp_type = data_type.vector_base_type();
        GVectorArray &vector_array = resources.construct<GVectorArray>(AT, cpp_type, 1);
        params.add_vector_output(vector_array);
        break;
      }
    }
  }
}

static Array<MFOutputSocket *> add_constant_folded_sockets(const MultiFunction &network_fn,
                                                           MFParamsBuilder &params,
                                                           ResourceCollector &resources,
                                                           MFNetwork &network)
{
  Array<MFOutputSocket *> folded_sockets{network_fn.param_indices().size(), nullptr};

  for (uint param_index : network_fn.param_indices()) {
    MFParamType param_type = network_fn.param_type(param_index);
    MFDataType data_type = param_type.data_type();

    const MultiFunction *constant_fn = nullptr;

    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &cpp_type = data_type.single_type();
        GMutableSpan array = params.computed_array(param_index);
        void *buffer = array.buffer();
        resources.add(buffer, array.type().destruct_cb(), AT);

        constant_fn = &resources.construct<CustomMF_GenericConstant>(AT, cpp_type, buffer);
        break;
      }
      case MFDataType::Vector: {
        GVectorArray &vector_array = params.computed_vector_array(param_index);
        GSpan array = vector_array[0];
        constant_fn = &resources.construct<CustomMF_GenericConstantArray>(AT, array);
        break;
      }
    }

    MFFunctionNode &folded_node = network.add_function(*constant_fn);
    folded_sockets[param_index] = &folded_node.output(0);
  }
  return folded_sockets;
}

static Array<MFOutputSocket *> compute_constant_sockets_and_add_folded_nodes(
    MFNetwork &network,
    Span<const MFInputSocket *> sockets_to_compute,
    ResourceCollector &resources)
{
  MFNetworkEvaluator network_fn{{}, sockets_to_compute};

  MFContextBuilder context;
  MFParamsBuilder params{network_fn, 1};
  prepare_params_for_constant_folding(network_fn, params, resources);
  network_fn.call({0}, params, context);
  return add_constant_folded_sockets(network_fn, params, resources, network);
}

/**
 * Find function nodes that always output the same value and replace those with constant nodes.
 */
void constant_folding(MFNetwork &network, ResourceCollector &resources)
{
  Vector<MFDummyNode *> temporary_nodes;
  Vector<MFInputSocket *> inputs_to_fold = find_constant_inputs_to_fold(network, temporary_nodes);
  if (inputs_to_fold.size() == 0) {
    return;
  }

  Array<MFOutputSocket *> folded_sockets = compute_constant_sockets_and_add_folded_nodes(
      network, inputs_to_fold.as_span(), resources);

  for (uint i : inputs_to_fold.index_range()) {
    MFOutputSocket &original_socket = *inputs_to_fold[i]->origin();
    network.relink(original_socket, *folded_sockets[i]);
  }

  network.remove(temporary_nodes.as_span());
}

/** \} */

}  // namespace blender::fn::mf_network_optimization
