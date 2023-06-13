/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"

#include "BKE_node_runtime.hh"

#include "BLI_multi_value_map.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_timeit.hh"

namespace blender::bke::anonymous_attribute_inferencing {
namespace aal = nodes::aal;
using nodes::NodeDeclaration;

static bool socket_is_field(const bNodeSocket &socket)
{
  return socket.display_shape == SOCK_DISPLAY_SHAPE_DIAMOND;
}

static const aal::RelationsInNode &get_relations_in_node(const bNode &node, ResourceScope &scope)
{
  if (node.is_group()) {
    if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id)) {
      /* Undefined tree types have no relations. */
      if (!ntreeIsRegistered(group)) {
        return scope.construct<aal::RelationsInNode>();
      }

      BLI_assert(group->runtime->anonymous_attribute_relations);
      return *group->runtime->anonymous_attribute_relations;
    }
  }
  if (node.is_reroute()) {
    const bNodeSocket &socket = node.input_socket(0);
    if (socket_is_field(socket)) {
      static const aal::RelationsInNode field_relations = []() {
        aal::RelationsInNode relations;
        relations.reference_relations.append({0, 0});
        return relations;
      }();
      return field_relations;
    }
    if (socket.type == SOCK_GEOMETRY) {
      static const aal::RelationsInNode geometry_relations = []() {
        aal::RelationsInNode relations;
        relations.propagate_relations.append({0, 0});
        return relations;
      }();
      return geometry_relations;
    }
  }
  if (ELEM(node.type, GEO_NODE_SIMULATION_INPUT, GEO_NODE_SIMULATION_OUTPUT)) {
    aal::RelationsInNode &relations = scope.construct<aal::RelationsInNode>();
    {
      /* Add eval relations. */
      int last_geometry_index = -1;
      for (const int i : node.input_sockets().index_range()) {
        const bNodeSocket &socket = node.input_socket(i);
        if (socket.type == SOCK_GEOMETRY) {
          last_geometry_index = i;
        }
        else if (socket_is_field(socket)) {
          if (last_geometry_index != -1) {
            relations.eval_relations.append({i, last_geometry_index});
          }
        }
      }
    }

    {
      /* Add available relations. */
      int last_geometry_index = -1;
      for (const int i : node.output_sockets().index_range()) {
        const bNodeSocket &socket = node.output_socket(i);
        if (socket.type == SOCK_GEOMETRY) {
          last_geometry_index = i;
        }
        else if (socket_is_field(socket)) {
          if (last_geometry_index == -1) {
            relations.available_on_none.append(i);
          }
          else {
            relations.available_relations.append({i, last_geometry_index});
          }
        }
      }
    }
    return relations;
  }
  if (const NodeDeclaration *node_decl = node.declaration()) {
    if (const aal::RelationsInNode *relations = node_decl->anonymous_attribute_relations()) {
      return *relations;
    }
  }
  return scope.construct<aal::RelationsInNode>();
}

Array<const aal::RelationsInNode *> get_relations_by_node(const bNodeTree &tree,
                                                          ResourceScope &scope)
{
  const Span<const bNode *> nodes = tree.all_nodes();
  Array<const aal::RelationsInNode *> relations_by_node(nodes.size());
  for (const int i : nodes.index_range()) {
    relations_by_node[i] = &get_relations_in_node(*nodes[i], scope);
  }
  return relations_by_node;
}

/**
 * Start at a group output socket and find all linked group inputs.
 */
static Vector<int> find_linked_group_inputs(
    const bNodeTree &tree,
    const bNodeSocket &group_output_socket,
    const FunctionRef<Vector<int>(const bNodeSocket &)> get_linked_node_inputs)
{
  Set<const bNodeSocket *> found_sockets;
  Stack<const bNodeSocket *> sockets_to_check;

  Vector<int> input_indices;

  found_sockets.add_new(&group_output_socket);
  sockets_to_check.push(&group_output_socket);

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_used()) {
          const bNodeSocket &from_socket = *link->fromsock;
          if (found_sockets.add(&from_socket)) {
            sockets_to_check.push(&from_socket);
          }
        }
      }
    }
    else {
      const bNode &node = socket.owner_node();
      for (const int input_index : get_linked_node_inputs(socket)) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          if (found_sockets.add(&input_socket)) {
            sockets_to_check.push(&input_socket);
          }
        }
      }
    }
  }

  for (const bNode *node : tree.group_input_nodes()) {
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (found_sockets.contains(socket)) {
        input_indices.append_non_duplicates(socket->index());
      }
    }
  }

  return input_indices;
}

static void infer_propagate_relations(const bNodeTree &tree,
                                      const Span<const aal::RelationsInNode *> relations_by_node,
                                      const bNode &group_output_node,
                                      aal::RelationsInNode &r_relations)
{
  for (const bNodeSocket *group_output_socket : group_output_node.input_sockets().drop_back(1)) {
    if (group_output_socket->type != SOCK_GEOMETRY) {
      continue;
    }
    const Vector<int> input_indices = find_linked_group_inputs(
        tree, *group_output_socket, [&](const bNodeSocket &output_socket) {
          Vector<int> indices;
          for (const aal::PropagateRelation &relation :
               relations_by_node[output_socket.owner_node().index()]->propagate_relations)
          {
            if (relation.to_geometry_output == output_socket.index()) {
              indices.append(relation.from_geometry_input);
            }
          }
          return indices;
        });
    for (const int input_index : input_indices) {
      aal::PropagateRelation relation;
      relation.from_geometry_input = input_index;
      relation.to_geometry_output = group_output_socket->index();
      r_relations.propagate_relations.append(relation);
    }
  }
}

static void infer_reference_relations(const bNodeTree &tree,
                                      const Span<const aal::RelationsInNode *> relations_by_node,
                                      const bNode &group_output_node,
                                      aal::RelationsInNode &r_relations)
{
  for (const bNodeSocket *group_output_socket : group_output_node.input_sockets().drop_back(1)) {
    if (!socket_is_field(*group_output_socket)) {
      continue;
    }
    const Vector<int> input_indices = find_linked_group_inputs(
        tree, *group_output_socket, [&](const bNodeSocket &output_socket) {
          Vector<int> indices;
          for (const aal::ReferenceRelation &relation :
               relations_by_node[output_socket.owner_node().index()]->reference_relations)
          {
            if (relation.to_field_output == output_socket.index()) {
              indices.append(relation.from_field_input);
            }
          }
          return indices;
        });
    for (const int input_index : input_indices) {
      if (tree.runtime->field_inferencing_interface->inputs[input_index] !=
          nodes::InputSocketFieldType::None)
      {
        aal::ReferenceRelation relation;
        relation.from_field_input = input_index;
        relation.to_field_output = group_output_socket->index();
        r_relations.reference_relations.append(relation);
      }
    }
  }
}

/**
 * Find group output geometries that contain anonymous attributes referenced by the field.
 * If `nullopt` is returned, the field does not depend on any anonymous attribute created in this
 * node tree.
 */
static std::optional<Vector<int>> find_available_on_outputs(
    const bNodeSocket &initial_group_output_socket,
    const bNode &group_output_node,
    const Span<const aal::RelationsInNode *> relations_by_node)
{
  Set<const bNodeSocket *> geometry_sockets;

  {
    /* Find the nodes that added anonymous attributes to the field. */
    Set<const bNodeSocket *> found_sockets;
    Stack<const bNodeSocket *> sockets_to_check;

    found_sockets.add_new(&initial_group_output_socket);
    sockets_to_check.push(&initial_group_output_socket);

    while (!sockets_to_check.is_empty()) {
      const bNodeSocket &socket = *sockets_to_check.pop();
      if (socket.is_input()) {
        for (const bNodeLink *link : socket.directly_linked_links()) {
          if (link->is_used()) {
            const bNodeSocket &from_socket = *link->fromsock;
            if (found_sockets.add(&from_socket)) {
              sockets_to_check.push(&from_socket);
            }
          }
        }
      }
      else {
        const bNode &node = socket.owner_node();
        const aal::RelationsInNode &relations = *relations_by_node[node.index()];
        for (const aal::AvailableRelation &relation : relations.available_relations) {
          if (socket.index() == relation.field_output) {
            const bNodeSocket &geometry_output = node.output_socket(relation.geometry_output);
            if (geometry_output.is_available()) {
              geometry_sockets.add(&geometry_output);
            }
          }
        }
        for (const aal::ReferenceRelation &relation : relations.reference_relations) {
          if (socket.index() == relation.to_field_output) {
            const bNodeSocket &field_input = node.input_socket(relation.from_field_input);
            if (field_input.is_available()) {
              if (found_sockets.add(&field_input)) {
                sockets_to_check.push(&field_input);
              }
            }
          }
        }
      }
    }
  }

  if (geometry_sockets.is_empty()) {
    /* The field does not depend on any anonymous attribute created within this node tree. */
    return std::nullopt;
  }

  /* Find the group output geometries that contain the anonymous attribute referenced by the field
   * output. */
  Set<const bNodeSocket *> found_sockets;
  Stack<const bNodeSocket *> sockets_to_check;

  for (const bNodeSocket *socket : geometry_sockets) {
    found_sockets.add_new(socket);
    sockets_to_check.push(socket);
  }

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    if (socket.is_input()) {
      const bNode &node = socket.owner_node();
      const aal::RelationsInNode &relations = *relations_by_node[node.index()];
      for (const aal::PropagateRelation &relation : relations.propagate_relations) {
        if (socket.index() == relation.from_geometry_input) {
          const bNodeSocket &output_socket = node.output_socket(relation.to_geometry_output);
          if (output_socket.is_available()) {
            if (found_sockets.add(&output_socket)) {
              sockets_to_check.push(&output_socket);
            }
          }
        }
      }
    }
    else {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_used()) {
          const bNodeSocket &to_socket = *link->tosock;
          if (found_sockets.add(&to_socket)) {
            sockets_to_check.push(&to_socket);
          }
        }
      }
    }
  }

  Vector<int> output_indices;
  for (const bNodeSocket *socket : group_output_node.input_sockets().drop_back(1)) {
    if (found_sockets.contains(socket)) {
      output_indices.append(socket->index());
    }
  }
  return output_indices;
}

static void infer_available_relations(const Span<const aal::RelationsInNode *> relations_by_node,
                                      const bNode &group_output_node,
                                      aal::RelationsInNode &r_relations)
{
  for (const bNodeSocket *group_output_socket : group_output_node.input_sockets().drop_back(1)) {
    if (!socket_is_field(*group_output_socket)) {
      continue;
    }
    const std::optional<Vector<int>> output_indices = find_available_on_outputs(
        *group_output_socket, group_output_node, relations_by_node);
    if (output_indices.has_value()) {
      if (output_indices->is_empty()) {
        r_relations.available_on_none.append(group_output_socket->index());
      }
      else {
        for (const int output_index : *output_indices) {
          aal::AvailableRelation relation;
          relation.field_output = group_output_socket->index();
          relation.geometry_output = output_index;
          r_relations.available_relations.append(relation);
        }
      }
    }
  }
}

/**
 * Returns a list of all the geometry inputs that the field input may be evaluated on.
 */
static Vector<int> find_eval_on_inputs(const bNodeTree &tree,
                                       const int field_input_index,
                                       const Span<const aal::RelationsInNode *> relations_by_node)
{
  const Span<const bNode *> group_input_nodes = tree.group_input_nodes();
  Set<const bNodeSocket *> geometry_sockets;

  {
    /* Find all the nodes that evaluate the input field. */
    Set<const bNodeSocket *> found_sockets;
    Stack<const bNodeSocket *> sockets_to_check;

    for (const bNode *node : group_input_nodes) {
      const bNodeSocket &socket = node->output_socket(field_input_index);
      found_sockets.add_new(&socket);
      sockets_to_check.push(&socket);
    }

    while (!sockets_to_check.is_empty()) {
      const bNodeSocket &socket = *sockets_to_check.pop();
      if (socket.is_input()) {
        const bNode &node = socket.owner_node();
        const aal::RelationsInNode &relations = *relations_by_node[node.index()];
        for (const aal::EvalRelation &relation : relations.eval_relations) {
          if (socket.index() == relation.field_input) {
            const bNodeSocket &geometry_input = node.input_socket(relation.geometry_input);
            if (geometry_input.is_available()) {
              geometry_sockets.add(&geometry_input);
            }
          }
        }
        for (const aal::ReferenceRelation &relation : relations.reference_relations) {
          if (socket.index() == relation.from_field_input) {
            const bNodeSocket &field_output = node.output_socket(relation.to_field_output);
            if (field_output.is_available()) {
              if (found_sockets.add(&field_output)) {
                sockets_to_check.push(&field_output);
              }
            }
          }
        }
      }
      else {
        for (const bNodeLink *link : socket.directly_linked_links()) {
          if (link->is_used()) {
            const bNodeSocket &to_socket = *link->tosock;
            if (found_sockets.add(&to_socket)) {
              sockets_to_check.push(&to_socket);
            }
          }
        }
      }
    }
  }

  if (geometry_sockets.is_empty()) {
    return {};
  }

  /* Find the group input geometries whose attributes are propagated to the nodes that evaluate the
   * field. */
  Set<const bNodeSocket *> found_sockets;
  Stack<const bNodeSocket *> sockets_to_check;

  Vector<int> geometry_input_indices;

  for (const bNodeSocket *socket : geometry_sockets) {
    found_sockets.add_new(socket);
    sockets_to_check.push(socket);
  }

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_used()) {
          const bNodeSocket &from_socket = *link->fromsock;
          if (found_sockets.add(&from_socket)) {
            sockets_to_check.push(&from_socket);
          }
        }
      }
    }
    else {
      const bNode &node = socket.owner_node();
      if (node.is_group_input()) {
        geometry_input_indices.append_non_duplicates(socket.index());
      }
      else {
        const aal::RelationsInNode &relations = *relations_by_node[node.index()];
        for (const aal::PropagateRelation &relation : relations.propagate_relations) {
          if (socket.index() == relation.to_geometry_output) {
            const bNodeSocket &input_socket = node.input_socket(relation.from_geometry_input);
            if (input_socket.is_available()) {
              if (found_sockets.add(&input_socket)) {
                sockets_to_check.push(&input_socket);
              }
            }
          }
        }
      }
    }
  }

  return geometry_input_indices;
}

static void infer_eval_relations(const bNodeTree &tree,
                                 const Span<const aal::RelationsInNode *> relations_by_node,
                                 aal::RelationsInNode &r_relations)
{
  for (const int input_index : tree.interface_inputs().index_range()) {
    if (tree.runtime->field_inferencing_interface->inputs[input_index] ==
        nodes::InputSocketFieldType::None)
    {
      continue;
    }
    const Vector<int> geometry_input_indices = find_eval_on_inputs(
        tree, input_index, relations_by_node);
    for (const int geometry_input : geometry_input_indices) {
      aal::EvalRelation relation;
      relation.field_input = input_index;
      relation.geometry_input = geometry_input;
      r_relations.eval_relations.append(std::move(relation));
    }
  }
}

bool update_anonymous_attribute_relations(bNodeTree &tree)
{
  tree.ensure_topology_cache();

  ResourceScope scope;
  Array<const aal::RelationsInNode *> relations_by_node = get_relations_by_node(tree, scope);

  std::unique_ptr<aal::RelationsInNode> new_relations = std::make_unique<aal::RelationsInNode>();
  if (!tree.has_available_link_cycle()) {
    if (const bNode *group_output_node = tree.group_output_node()) {
      infer_propagate_relations(tree, relations_by_node, *group_output_node, *new_relations);
      infer_reference_relations(tree, relations_by_node, *group_output_node, *new_relations);
      infer_available_relations(relations_by_node, *group_output_node, *new_relations);
    }
    infer_eval_relations(tree, relations_by_node, *new_relations);
  }

  const bool group_interface_changed = !tree.runtime->anonymous_attribute_relations ||
                                       *tree.runtime->anonymous_attribute_relations !=
                                           *new_relations;
  tree.runtime->anonymous_attribute_relations = std::move(new_relations);

  return group_interface_changed;
}

}  // namespace blender::bke::anonymous_attribute_inferencing
