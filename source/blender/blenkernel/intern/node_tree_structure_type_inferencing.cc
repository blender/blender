/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_bit_vector.hh"
#include "BLI_enum_flags.hh"
#include "BLI_stack.hh"

#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

namespace blender::bke::node_structure_type_inferencing {

using nodes::StructureType;
namespace aal = nodes::anonymous_attribute_lifetime;

static nodes::StructureTypeInterface calc_node_interface(const bNode &node)
{
  const Span<const bNodeSocket *> input_sockets = node.input_sockets();
  const Span<const bNodeSocket *> output_sockets = node.output_sockets();

  nodes::StructureTypeInterface node_interface;
  node_interface.inputs.reinitialize(input_sockets.size());
  node_interface.outputs.reinitialize(output_sockets.size());

  if (node.is_undefined() || !node.declaration() || node.declaration()->skip_updating_sockets) {
    node_interface.inputs.fill(StructureType::Dynamic);
    node_interface.outputs.fill(
        nodes::StructureTypeInterface::OutputDependency{StructureType::Dynamic});
    return node_interface;
  }
  if (node.is_reroute()) {
    node_interface.inputs.first() = StructureType::Dynamic;
    node_interface.outputs.first() = {StructureType::Dynamic, {0}};
    return node_interface;
  }

  for (const int i : input_sockets.index_range()) {
    const nodes::SocketDeclaration &decl = *input_sockets[i]->runtime->declaration;
    node_interface.inputs[i] = decl.structure_type;
  }

  for (const int output : output_sockets.index_range()) {
    const nodes::SocketDeclaration &decl = *output_sockets[output]->runtime->declaration;
    nodes::StructureTypeInterface::OutputDependency &dependency = node_interface.outputs[output];
    dependency.type = decl.structure_type;
    if (dependency.type != StructureType::Dynamic) {
      continue;
    }

    /* Currently the input sockets that influence the field status of an output are the same as the
     * sockets that influence its structure type. Reuse that for the propagation of structure type
     * until there is a more generic format of intra-node dependencies. */
    switch (decl.output_field_dependency.field_type()) {
      case nodes::OutputSocketFieldType::None:
        break;
      case nodes::OutputSocketFieldType::FieldSource:
        break;
      case nodes::OutputSocketFieldType::DependentField:
        dependency.linked_inputs.reinitialize(input_sockets.size());
        array_utils::fill_index_range(dependency.linked_inputs.as_mutable_span());
        break;
      case nodes::OutputSocketFieldType::PartiallyDependent:
        dependency.linked_inputs = decl.output_field_dependency.linked_input_indices();
        break;
    }
  }

  return node_interface;
}

static Array<nodes::StructureTypeInterface> calc_node_interfaces(const bNodeTree &tree)
{
  const Span<const bNode *> nodes = tree.all_nodes();
  Array<nodes::StructureTypeInterface> interfaces(nodes.size());
  for (const int i : nodes.index_range()) {
    interfaces[i] = calc_node_interface(*nodes[i]);
  }
  return interfaces;
}

enum class DataRequirement : int8_t { None, Field, Single, Grid, List, Invalid };

static DataRequirement merge(const DataRequirement a, const DataRequirement b)
{
  if (a == b) {
    return a;
  }
  if (a == DataRequirement::None) {
    return b;
  }
  if (b == DataRequirement::None) {
    return a;
  }
  if ((a == DataRequirement::Field && b == DataRequirement::Single) ||
      (a == DataRequirement::Single && b == DataRequirement::Field))
  {
    /* Single beats field, because fields can accept single values too. */
    return DataRequirement::Single;
  }
  return DataRequirement::Invalid;
}

static StructureType data_requirement_to_auto_structure_type(const DataRequirement requirement)
{
  switch (requirement) {
    case DataRequirement::None:
      return StructureType::Dynamic;
    case DataRequirement::Field:
      return StructureType::Field;
    case DataRequirement::Single:
      return StructureType::Single;
    case DataRequirement::Grid:
      return StructureType::Grid;
    case DataRequirement::List:
      return StructureType::List;
    case DataRequirement::Invalid:
      return StructureType::Dynamic;
  }
  BLI_assert_unreachable();
  return StructureType::Dynamic;
}

static void find_auto_structure_type_sockets(const bNodeTree &tree,
                                             bits::MutableBoundedBitSpan is_auto_structure_type)
{
  /* Handle group inputs. */
  for (const int i : tree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &io_socket = *tree.interface_inputs()[i];
    if (io_socket.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
      continue;
    }
    for (const bNode *node : tree.group_input_nodes()) {
      const bNodeSocket &socket = node->output_socket(i);
      is_auto_structure_type[socket.index_in_tree()].set();
    }
  }

  /* Handle group outputs. */
  if (const bNode *group_output_node = tree.group_output_node()) {
    is_auto_structure_type.slice(group_output_node->input_socket_indices_in_tree().drop_back(1))
        .set_all();
  }

  /* Handle closure inputs and outputs. */
  const bke::bNodeZoneType *closure_zone_type = bke::zone_type_by_node_type(NODE_CLOSURE_OUTPUT);
  for (const bNode *closure_input_node : tree.nodes_by_type("NodeClosureInput")) {
    const auto *closure_output_node = closure_zone_type->get_corresponding_output(
        tree, *closure_input_node);
    if (!closure_output_node) {
      continue;
    }
    const auto &storage = *static_cast<const NodeClosureOutput *>(closure_output_node->storage);
    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeClosureInputItem &item = storage.input_items.items[i];
      if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        const bNodeSocket &socket = closure_input_node->output_socket(i);
        is_auto_structure_type[socket.index_in_tree()].set();
      }
    }
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeClosureOutputItem &item = storage.output_items.items[i];
      if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        const bNodeSocket &socket = closure_output_node->input_socket(i);
        is_auto_structure_type[socket.index_in_tree()].set();
      }
    }
  }

  /* Handle Evaluate Closure nodes. */
  for (const bNode *evaluate_closure_node : tree.nodes_by_type("NodeEvaluateClosure")) {
    auto &storage = *static_cast<NodeEvaluateClosure *>(evaluate_closure_node->storage);
    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeEvaluateClosureInputItem &item = storage.input_items.items[i];
      if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        const bNodeSocket &socket = evaluate_closure_node->input_socket(i + 1);
        is_auto_structure_type[socket.index_in_tree()].set();
      }
    }
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
      if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        const bNodeSocket &socket = evaluate_closure_node->output_socket(i);
        is_auto_structure_type[socket.index_in_tree()].set();
      }
    }
  }

  /* Handle Combine Bundle nodes. */
  for (const bNode *node : tree.nodes_by_type("NodeCombineBundle")) {
    auto &storage = *static_cast<NodeCombineBundle *>(node->storage);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeCombineBundleItem &item = storage.items[i];
      if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        const bNodeSocket &socket = node->input_socket(i);
        is_auto_structure_type[socket.index_in_tree()].set();
      }
    }
  }

  /* Handle Separate Bundle nodes. */
  for (const bNode *node : tree.nodes_by_type("NodeSeparateBundle")) {
    auto &storage = *static_cast<NodeSeparateBundle *>(node->storage);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeSeparateBundleItem &item = storage.items[i];
      if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        const bNodeSocket &socket = node->output_socket(i);
        is_auto_structure_type[socket.index_in_tree()].set();
      }
    }
  }
}

static void init_input_requirements(const bNodeTree &tree,
                                    const bits::BoundedBitSpan is_auto_structure_type,
                                    MutableSpan<DataRequirement> input_requirements)
{
  for (const bNode *node : tree.all_nodes()) {
    for (const bNodeSocket *socket : node->input_sockets()) {
      DataRequirement &requirement = input_requirements[socket->index_in_all_inputs()];
      if (is_auto_structure_type[socket->index_in_tree()]) {
        requirement = DataRequirement::None;
        continue;
      }
      const nodes::SocketDeclaration *declaration = socket->runtime->declaration;
      if (!declaration) {
        requirement = DataRequirement::None;
        continue;
      }
      if (nodes::socket_type_always_single(eNodeSocketDatatype(socket->type))) {
        requirement = DataRequirement::Single;
        continue;
      }
      switch (declaration->structure_type) {
        case StructureType::Dynamic: {
          requirement = DataRequirement::None;
          break;
        }
        case StructureType::Single: {
          requirement = DataRequirement::Single;
          break;
        }
        case StructureType::Grid: {
          requirement = DataRequirement::Grid;
          break;
        }
        case StructureType::Field: {
          requirement = DataRequirement::Field;
          break;
        }
        case StructureType::List: {
          requirement = DataRequirement::List;
          break;
        }
      }
    }
  }
}

static DataRequirement calc_output_socket_requirement(
    const bNodeSocket &output_socket, const Span<DataRequirement> input_requirements)
{
  DataRequirement requirement = DataRequirement::None;
  if (!output_socket.is_available()) {
    return requirement;
  }
  for (const bNodeSocket *socket : output_socket.directly_linked_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    requirement = merge(requirement, input_requirements[socket->index_in_all_inputs()]);
  }
  return requirement;
}

static void store_group_input_structure_types(const bNodeTree &tree,
                                              const Span<DataRequirement> input_requirements,
                                              nodes::StructureTypeInterface &derived_interface)
{
  /* Merge usages from all group input nodes. */
  Array<DataRequirement> interface_requirements(tree.interface_inputs().size(),
                                                DataRequirement::None);
  for (const bNode *node : tree.group_input_nodes()) {
    const Span<const bNodeSocket *> output_sockets = node->output_sockets();
    for (const int i : output_sockets.index_range().drop_back(1)) {
      const bNodeSocket &output = *output_sockets[i];
      interface_requirements[i] = merge(
          interface_requirements[i], calc_output_socket_requirement(output, input_requirements));
    }
  }

  /* Build derived interface structure types from group input nodes. */
  for (const int i : tree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &io_socket = *tree.interface_inputs()[i];
    if (io_socket.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
      derived_interface.inputs[i] = StructureType(io_socket.structure_type);
      continue;
    }

    const DataRequirement requirement = interface_requirements[i];
    derived_interface.inputs[i] = data_requirement_to_auto_structure_type(requirement);
  }
}

static void store_auto_output_structure_types(const bNodeTree &tree,
                                              const Span<DataRequirement> input_requirements,
                                              const bits::BoundedBitSpan is_auto_structure_type,
                                              MutableSpan<StructureType> structure_types)
{
  const Span<const bNodeSocket *> all_sockets = tree.all_sockets();
  bits::foreach_1_index(is_auto_structure_type, [&](const int i) {
    const bNodeSocket &socket = *all_sockets[i];
    if (socket.is_input()) {
      return;
    }
    const bNode &node = socket.owner_node();
    if (node.is_group_input()) {
      /* Group input nodes have special handling in #store_group_input_structure_types because
       * corresponding sockets on all group input nodes should have the same structure type. */
      return;
    }

    const DataRequirement requirement = calc_output_socket_requirement(socket, input_requirements);
    structure_types[socket.index_in_tree()] = data_requirement_to_auto_structure_type(requirement);
  });
}

enum class ZoneInOutChange {
  None = 0,
  In = (1 << 1),
  Out = (1 << 2),
};
ENUM_OPERATORS(ZoneInOutChange);

static ZoneInOutChange simulation_zone_requirements_propagate(
    const bNode &input_node,
    const bNode &output_node,
    MutableSpan<DataRequirement> input_requirements)
{
  ZoneInOutChange change = ZoneInOutChange::None;
  for (const int i : output_node.output_sockets().index_range()) {
    /* First input node output is Delta Time which does not appear in the output node outputs. */
    const bNodeSocket &input_of_input_node = input_node.input_socket(i);
    const bNodeSocket &output_of_output_node = output_node.output_socket(i);
    const bNodeSocket &input_of_output_node = output_node.input_socket(i + 1);
    const DataRequirement new_value = merge(
        input_requirements[input_of_input_node.index_in_all_inputs()],
        calc_output_socket_requirement(output_of_output_node, input_requirements));
    if (input_requirements[input_of_input_node.index_in_all_inputs()] != new_value) {
      input_requirements[input_of_input_node.index_in_all_inputs()] = new_value;
      change |= ZoneInOutChange::In;
    }
    if (input_requirements[input_of_output_node.index_in_all_inputs()] != new_value) {
      input_requirements[input_of_output_node.index_in_all_inputs()] = new_value;
      change |= ZoneInOutChange::Out;
    }
  }
  return change;
}

static ZoneInOutChange repeat_zone_requirements_propagate(
    const bNode &input_node,
    const bNode &output_node,
    MutableSpan<DataRequirement> input_requirements)
{
  ZoneInOutChange change = ZoneInOutChange::None;
  for (const int i : output_node.output_sockets().index_range()) {
    const bNodeSocket &input_of_input_node = input_node.input_socket(i + 1);
    const bNodeSocket &output_of_output_node = output_node.output_socket(i);
    const bNodeSocket &input_of_output_node = output_node.input_socket(i);
    const DataRequirement new_value = merge(
        input_requirements[input_of_input_node.index_in_all_inputs()],
        calc_output_socket_requirement(output_of_output_node, input_requirements));
    if (input_requirements[input_of_input_node.index_in_all_inputs()] != new_value) {
      input_requirements[input_of_input_node.index_in_all_inputs()] = new_value;
      change |= ZoneInOutChange::In;
    }
    if (input_requirements[input_of_output_node.index_in_all_inputs()] != new_value) {
      input_requirements[input_of_output_node.index_in_all_inputs()] = new_value;
      change |= ZoneInOutChange::Out;
    }
  }
  return change;
}

static bool propagate_zone_data_requirements(const bNodeTree &tree,
                                             const bNode &node,
                                             MutableSpan<DataRequirement> input_requirements)
{
  /* Sync field state between zone nodes and schedule another pass if necessary. */
  switch (node.type_legacy) {
    case GEO_NODE_SIMULATION_INPUT: {
      const auto &data = *static_cast<const NodeGeometrySimulationInput *>(node.storage);
      if (const bNode *output_node = tree.node_by_id(data.output_node_id)) {
        const ZoneInOutChange change = simulation_zone_requirements_propagate(
            node, *output_node, input_requirements);
        if (flag_is_set(change, ZoneInOutChange::Out)) {
          return true;
        }
      }
      return false;
    }
    case GEO_NODE_SIMULATION_OUTPUT: {
      for (const bNode *input_node : tree.nodes_by_type("GeometryNodeSimulationInput")) {
        const auto &data = *static_cast<const NodeGeometrySimulationInput *>(input_node->storage);
        if (node.identifier == data.output_node_id) {
          const ZoneInOutChange change = simulation_zone_requirements_propagate(
              *input_node, node, input_requirements);
          if (flag_is_set(change, ZoneInOutChange::In)) {
            return true;
          }
        }
      }
      return false;
    }
    case GEO_NODE_REPEAT_INPUT: {
      const auto &data = *static_cast<const NodeGeometryRepeatInput *>(node.storage);
      if (const bNode *output_node = tree.node_by_id(data.output_node_id)) {
        const ZoneInOutChange change = repeat_zone_requirements_propagate(
            node, *output_node, input_requirements);
        if (flag_is_set(change, ZoneInOutChange::Out)) {
          return true;
        }
      }
      return false;
    }
    case GEO_NODE_REPEAT_OUTPUT: {
      for (const bNode *input_node : tree.nodes_by_type("GeometryNodeRepeatInput")) {
        const auto &data = *static_cast<const NodeGeometryRepeatInput *>(input_node->storage);
        if (node.identifier == data.output_node_id) {
          const ZoneInOutChange change = repeat_zone_requirements_propagate(
              *input_node, node, input_requirements);
          if (flag_is_set(change, ZoneInOutChange::In)) {
            return true;
          }
        }
      }
      return false;
    }
  }

  return false;
}

static void propagate_right_to_left(const bNodeTree &tree,
                                    const Span<nodes::StructureTypeInterface> node_interfaces,
                                    MutableSpan<DataRequirement> input_requirements)
{
  while (true) {
    bool need_update = false;

    for (const bNode *node : tree.toposort_right_to_left()) {
      const Span<const bNodeSocket *> input_sockets = node->input_sockets();
      const Span<const bNodeSocket *> output_sockets = node->output_sockets();
      const nodes::StructureTypeInterface &node_interface = node_interfaces[node->index()];

      for (const int output : node_interface.outputs.index_range()) {
        const bNodeSocket &output_socket = *output_sockets[output];
        DataRequirement output_requirement = DataRequirement::None;
        for (const bNodeSocket *socket : output_socket.directly_linked_sockets()) {
          if (!socket->is_available()) {
            continue;
          }
          output_requirement = merge(output_requirement,
                                     input_requirements[socket->index_in_all_inputs()]);
        }

        switch (output_requirement) {
          case DataRequirement::Invalid:
          case DataRequirement::None: {
            break;
          }
          case DataRequirement::Single: {
            /* If the output is a single, all inputs must be singles. */
            for (const int input : node_interface.outputs[output].linked_inputs) {
              const bNodeSocket &input_socket = *input_sockets[input];
              input_requirements[input_socket.index_in_all_inputs()] = DataRequirement::Single;
            }
            break;
          }
          case DataRequirement::Field:
          case DataRequirement::Grid:
          case DataRequirement::List: {
            /* When a data requirement could be provided by multiple node inputs (i.e. only a
             * single node input involved in a math operation has to be a volume grid for the
             * output to be a grid), it's better to not propagate the data requirement than
             * incorrectly saying that all of the inputs have it. */
            Vector<int, 8> inputs_with_links;
            for (const int input : node_interface.outputs[output].linked_inputs) {
              const bNodeSocket &input_socket = *input_sockets[input];
              if (input_requirements[input_socket.index_in_all_inputs()] ==
                  DataRequirement::Single)
              {
                /* Inputs which require a single value can't get a different requirement. */
                continue;
              }
              if (input_socket.is_directly_linked()) {
                inputs_with_links.append(input_socket.index_in_all_inputs());
              }
            }
            if (inputs_with_links.size() == 1) {
              input_requirements[inputs_with_links.first()] = output_requirement;
            }
            else {
              for (const int input : inputs_with_links) {
                input_requirements[input] = DataRequirement::None;
              }
            }
            break;
          }
        }
      }

      /* Find reverse dependencies and resolve conflicts, which may require another pass. */
      if (propagate_zone_data_requirements(tree, *node, input_requirements)) {
        need_update = true;
      }
    }

    if (!need_update) {
      break;
    }
  }
}

static StructureType left_to_right_merge(const StructureType a, const StructureType b)
{
  if (a == b) {
    return a;
  }
  if (a == StructureType::Dynamic || b == StructureType::Dynamic) {
    return StructureType::Dynamic;
  }
  if ((a == StructureType::Field && b == StructureType::Grid) ||
      (a == StructureType::Grid && b == StructureType::Field))
  {
    return StructureType::Grid;
  }
  if ((a == StructureType::Single && b == StructureType::Field) ||
      (a == StructureType::Field && b == StructureType::Single))
  {
    return StructureType::Field;
  }
  if ((a == StructureType::Single && b == StructureType::Grid) ||
      (a == StructureType::Grid && b == StructureType::Single))
  {
    return StructureType::Grid;
  }
  if ((a == StructureType::Single && b == StructureType::List) ||
      (a == StructureType::List && b == StructureType::Single))
  {
    return StructureType::List;
  }
  if ((a == StructureType::Field && b == StructureType::List) ||
      (a == StructureType::List && b == StructureType::Field))
  {
    return StructureType::List;
  }
  /* Invalid combination. */
  return a;
}

static ZoneInOutChange simulation_zone_status_propagate(const bNode &input_node,
                                                        const bNode &output_node,
                                                        MutableSpan<StructureType> structure_types)
{
  ZoneInOutChange change = ZoneInOutChange::None;
  for (const int i : output_node.output_sockets().index_range()) {
    /* First input node output is Delta Time which does not appear in the output node outputs. */
    const bNodeSocket &input = input_node.output_socket(i + 1);
    const bNodeSocket &output = output_node.output_socket(i);
    const StructureType new_value = left_to_right_merge(structure_types[input.index_in_tree()],
                                                        structure_types[output.index_in_tree()]);
    if (structure_types[input.index_in_tree()] != new_value) {
      structure_types[input.index_in_tree()] = new_value;
      change |= ZoneInOutChange::In;
    }
    if (structure_types[output.index_in_tree()] != new_value) {
      structure_types[output.index_in_tree()] = new_value;
      change |= ZoneInOutChange::Out;
    }
  }
  return change;
}

static ZoneInOutChange repeat_zone_status_propagate(const bNode &input_node,
                                                    const bNode &output_node,
                                                    MutableSpan<StructureType> structure_types)
{
  ZoneInOutChange change = ZoneInOutChange::None;
  for (const int i : output_node.output_sockets().index_range()) {
    const bNodeSocket &input_of_input_node = input_node.output_socket(i + 1);
    const bNodeSocket &output_of_output_node = output_node.output_socket(i);
    const StructureType new_value = left_to_right_merge(
        structure_types[input_of_input_node.index_in_tree()],
        structure_types[output_of_output_node.index_in_tree()]);
    if (structure_types[input_of_input_node.index_in_tree()] != new_value) {
      structure_types[input_of_input_node.index_in_tree()] = new_value;
      change |= ZoneInOutChange::In;
    }
    if (structure_types[output_of_output_node.index_in_tree()] != new_value) {
      structure_types[output_of_output_node.index_in_tree()] = new_value;
      change |= ZoneInOutChange::Out;
    }
  }
  return change;
}

static bool propagate_zone_status(const bNodeTree &tree,
                                  const bNode &node,
                                  MutableSpan<StructureType> structure_types)
{
  /* Sync field state between zone nodes and schedule another pass if necessary. */
  switch (node.type_legacy) {
    case GEO_NODE_SIMULATION_INPUT: {
      const auto &data = *static_cast<const NodeGeometrySimulationInput *>(node.storage);
      if (const bNode *output_node = tree.node_by_id(data.output_node_id)) {
        const ZoneInOutChange change = simulation_zone_status_propagate(
            node, *output_node, structure_types);
        if (flag_is_set(change, ZoneInOutChange::Out)) {
          return true;
        }
      }
      return false;
    }
    case GEO_NODE_SIMULATION_OUTPUT: {
      for (const bNode *input_node : tree.nodes_by_type("GeometryNodeSimulationInput")) {
        const auto &data = *static_cast<const NodeGeometrySimulationInput *>(input_node->storage);
        if (node.identifier == data.output_node_id) {
          const ZoneInOutChange change = simulation_zone_status_propagate(
              *input_node, node, structure_types);
          if (flag_is_set(change, ZoneInOutChange::In)) {
            return true;
          }
        }
      }
      return false;
    }
    case GEO_NODE_REPEAT_INPUT: {
      const auto &data = *static_cast<const NodeGeometryRepeatInput *>(node.storage);
      if (const bNode *output_node = tree.node_by_id(data.output_node_id)) {
        const ZoneInOutChange change = repeat_zone_status_propagate(
            node, *output_node, structure_types);
        if (flag_is_set(change, ZoneInOutChange::Out)) {
          return true;
        }
      }
      return false;
    }
    case GEO_NODE_REPEAT_OUTPUT: {
      for (const bNode *input_node : tree.nodes_by_type("GeometryNodeRepeatInput")) {
        const auto &data = *static_cast<const NodeGeometryRepeatInput *>(input_node->storage);
        if (node.identifier == data.output_node_id) {
          const ZoneInOutChange change = repeat_zone_status_propagate(
              *input_node, node, structure_types);
          if (flag_is_set(change, ZoneInOutChange::In)) {
            return true;
          }
        }
      }
      return false;
    }
  }

  return false;
}

static StructureType get_unconnected_input_structure_type(
    const nodes::SocketDeclaration &declaration)
{
  if (declaration.input_field_type == nodes::InputSocketFieldType::Implicit) {
    return StructureType::Field;
  }
  return StructureType::Single;
}

static void propagate_left_to_right(const bNodeTree &tree,
                                    const Span<nodes::StructureTypeInterface> node_interfaces,
                                    const Span<StructureType> group_input_structure_types,
                                    const bits::BoundedBitSpan is_auto_structure_type,
                                    MutableSpan<StructureType> structure_types)
{
  for (const bNodeSocket *input : tree.all_input_sockets()) {
    if (input->owner_node().is_undefined()) {
      continue;
    }
    if (!input->is_directly_linked()) {
      if (const nodes::SocketDeclaration *declaration = input->runtime->declaration) {
        structure_types[input->index_in_tree()] = get_unconnected_input_structure_type(
            *declaration);
      }
    }
  }

  /* Outputs of these nodes have dynamic structure type but should start out as single values. */
  for (const StringRefNull idname : {"GeometryNodeRepeatInput",
                                     "GeometryNodeRepeatOutput",
                                     "GeometryNodeSimulationInput",
                                     "GeometryNodeSimulationOutput"})
  {
    for (const bNode *node : tree.nodes_by_type(idname)) {
      for (const bNodeSocket *socket : node->output_sockets()) {
        structure_types[socket->index_in_tree()] = StructureType::Single;
      }
    }
  }

  while (true) {
    bool need_update = false;
    for (const bNode *node : tree.toposort_left_to_right()) {
      if (node->is_undefined()) {
        continue;
      }
      const Span<const bNodeSocket *> input_sockets = node->input_sockets();
      const Span<const bNodeSocket *> output_sockets = node->output_sockets();
      if (node->is_group_input()) {
        for (const int i : output_sockets.index_range().drop_back(1)) {
          structure_types[output_sockets[i]->index_in_tree()] = group_input_structure_types[i];
        }
        continue;
      }

      for (const bNodeSocket *input : input_sockets) {
        if (!input->is_available()) {
          continue;
        }

        std::optional<StructureType> input_type;
        for (const bNodeLink *link : input->directly_linked_links()) {
          if (!link->is_used()) {
            continue;
          }
          const StructureType new_type = structure_types[link->fromsock->index_in_tree()];
          if (input_type) {
            input_type = left_to_right_merge(*input_type, new_type);
          }
          else {
            input_type = new_type;
          }
        }
        if (input_type) {
          structure_types[input->index_in_tree()] = *input_type;
        }
      }

      const nodes::StructureTypeInterface &node_interface = node_interfaces[node->index()];

      for (const int output_index : node_interface.outputs.index_range()) {
        const bNodeSocket &output = *output_sockets[output_index];
        if (!output.is_available() || !output.runtime->declaration) {
          continue;
        }
        if (is_auto_structure_type[output.index_in_tree()]) {
          /* Has been initialized in #store_auto_output_structure_types. */
          continue;
        }
        const nodes::SocketDeclaration &declaration = *output.runtime->declaration;

        std::optional<StructureType> output_type;
        for (const int input_index : node_interface.outputs[output_index].linked_inputs) {
          const bNodeSocket &input = node->input_socket(input_index);
          if (!input.is_available()) {
            continue;
          }
          const StructureType new_type = structure_types[input.index_in_tree()];
          if (output_type) {
            output_type = left_to_right_merge(*output_type, new_type);
          }
          else {
            output_type = new_type;
          }
        }
        structure_types[output.index_in_tree()] = output_type.value_or(declaration.structure_type);
      }

      if (propagate_zone_status(tree, *node, structure_types)) {
        need_update = true;
      }
    }

    if (!need_update) {
      break;
    }
  }
}

static Vector<int> find_dynamic_output_linked_inputs(
    const bNodeSocket &group_output, const Span<nodes::StructureTypeInterface> interface_by_node)
{
  /* Use a Set instead of an array indexed by socket because we may only look at a few sockets. */
  Set<const bNodeSocket *> handled_sockets;
  Stack<const bNodeSocket *> sockets_to_check;

  handled_sockets.add(&group_output);
  sockets_to_check.push(&group_output);

  Vector<int> group_inputs;

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket *input_socket = sockets_to_check.pop();
    if (!input_socket->is_directly_linked()) {
      continue;
    }

    for (const bNodeSocket *origin_socket : input_socket->directly_linked_sockets()) {
      const bNode &origin_node = origin_socket->owner_node();
      if (origin_node.is_group_input()) {
        group_inputs.append_non_duplicates(origin_socket->index());
        continue;
      }

      const nodes::StructureTypeInterface &node_interface = interface_by_node[origin_node.index()];
      for (const int input_index : node_interface.outputs[origin_socket->index()].linked_inputs) {
        const bNodeSocket &input = origin_node.input_socket(input_index);
        if (!input.is_available()) {
          continue;
        }
        if (handled_sockets.add(&input)) {
          sockets_to_check.push(&input);
        }
      }
    }
  }

  return group_inputs;
}

static void store_group_output_structure_types(
    const bNodeTree &tree,
    const Span<nodes::StructureTypeInterface> interface_by_node,
    const Span<StructureType> structure_types,
    nodes::StructureTypeInterface &interface)
{
  const bNode *group_output_node = tree.group_output_node();
  if (!group_output_node) {
    for (nodes::StructureTypeInterface::OutputDependency &output : interface.outputs) {
      output.type = StructureType::Dynamic;
    }
    return;
  }

  const Span<const bNodeTreeInterfaceSocket *> interface_outputs = tree.interface_outputs();
  const Span<const bNodeSocket *> sockets = group_output_node->input_sockets().drop_back(1);
  for (const int i : sockets.index_range()) {
    if (interface_outputs[i]->structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
      interface.outputs[i] = {StructureType(interface_outputs[i]->structure_type), {}};
      continue;
    }
    /* Update derived interface output structure types from output node socket usages. */
    interface.outputs[i].type = structure_types[sockets[i]->index_in_tree()];
    if (interface.outputs[i].type == StructureType::Dynamic) {
      const Vector<int> linked_inputs = find_dynamic_output_linked_inputs(*sockets[i],
                                                                          interface_by_node);
      interface.outputs[i] = {StructureType::Dynamic, linked_inputs.as_span()};
    }
  }
}

struct StructureTypeInferenceResult {
  nodes::StructureTypeInterface group_interface;
  Array<StructureType> socket_structure_types;
};

static StructureTypeInferenceResult calc_structure_type_interface(const bNodeTree &tree)
{
  tree.ensure_topology_cache();
  tree.ensure_interface_cache();

  StructureTypeInferenceResult result;
  result.socket_structure_types = Array<StructureType>(tree.all_sockets().size(),
                                                       StructureType::Dynamic);

  result.group_interface.inputs.reinitialize(tree.interface_inputs().size());
  result.group_interface.outputs.reinitialize(tree.interface_outputs().size());
  if (tree.has_available_link_cycle()) {
    result.group_interface.inputs.fill(StructureType::Dynamic);
    result.group_interface.outputs.fill({StructureType::Dynamic, {}});
    return result;
  }

  Array<nodes::StructureTypeInterface> node_interfaces = calc_node_interfaces(tree);
  bits::BitVector<> is_auto_structure_type(tree.all_sockets().size(), false);

  Array<DataRequirement> data_requirements(tree.all_input_sockets().size());

  find_auto_structure_type_sockets(tree, is_auto_structure_type);
  init_input_requirements(tree, is_auto_structure_type, data_requirements);
  propagate_right_to_left(tree, node_interfaces, data_requirements);
  store_group_input_structure_types(tree, data_requirements, result.group_interface);
  store_auto_output_structure_types(
      tree, data_requirements, is_auto_structure_type, result.socket_structure_types);
  propagate_left_to_right(tree,
                          node_interfaces,
                          result.group_interface.inputs,
                          is_auto_structure_type,
                          result.socket_structure_types);
  store_group_output_structure_types(
      tree, node_interfaces, result.socket_structure_types, result.group_interface);

  /* Ensure that the structure type is never invalid. */
  for (const int i : tree.all_sockets().index_range()) {
    const bNodeSocket &socket = *tree.all_sockets()[i];
    if (nodes::socket_type_always_single(eNodeSocketDatatype(socket.type))) {
      result.socket_structure_types[i] = StructureType::Single;
    }
  }

  return result;
}

bool update_structure_type_interface(bNodeTree &tree)
{
  StructureTypeInferenceResult result = calc_structure_type_interface(tree);
  for (const int i : tree.all_sockets().index_range()) {
    const bNodeSocket &socket = *tree.all_sockets()[i];
    socket.runtime->inferred_structure_type = result.socket_structure_types[i];
  }
  if (tree.runtime->structure_type_interface &&
      *tree.runtime->structure_type_interface == result.group_interface)
  {
    return false;
  }
  tree.runtime->structure_type_interface = std::make_unique<nodes::StructureTypeInterface>(
      std::move(result.group_interface));
  return true;
}

}  // namespace blender::bke::node_structure_type_inferencing
