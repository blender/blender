/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

#include "NOD_geometry.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "BLI_enum_flags.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"

namespace blender::bke::node_field_inferencing {

using nodes::FieldInferencingInterface;
using nodes::InputSocketFieldType;
using nodes::NodeDeclaration;
using nodes::OutputFieldDependency;
using nodes::OutputSocketFieldType;
using nodes::SocketDeclaration;

static bool is_field_socket_type(const bNodeSocket &socket)
{
  return nodes::socket_type_supports_fields(socket.typeinfo->type);
}

static bool all_dangling_reroutes(const Span<const bNodeSocket *> sockets)
{
  for (const bNodeSocket *socket : sockets) {
    if (!socket->owner_node().is_dangling_reroute()) {
      return false;
    }
  }
  return true;
}

static InputSocketFieldType get_interface_input_field_type(const bNode &node,
                                                           const bNodeSocket &socket)
{
  if (!is_field_socket_type(socket)) {
    return InputSocketFieldType::None;
  }
  if (node.is_reroute()) {
    return InputSocketFieldType::IsSupported;
  }
  if (node.is_group_output()) {
    /* Outputs always support fields when the data type is correct. */
    return InputSocketFieldType::IsSupported;
  }
  if (node.is_undefined()) {
    return InputSocketFieldType::None;
  }
  if (node.type_legacy == NODE_CUSTOM) {
    return InputSocketFieldType::None;
  }

  /* TODO: Ensure declaration exists. */
  const NodeDeclaration *node_decl = node.declaration();

  /* Node declarations should be implemented for nodes involved here. */
  BLI_assert(node_decl != nullptr);

  /* Get the field type from the declaration. */
  const SocketDeclaration &socket_decl = *node_decl->inputs[socket.index()];
  const InputSocketFieldType field_type = socket_decl.input_field_type;
  return field_type;
}

static OutputFieldDependency get_interface_output_field_dependency(const bNode &node,
                                                                   const bNodeSocket &socket)
{
  if (!is_field_socket_type(socket)) {
    /* Non-field sockets always output data. */
    return OutputFieldDependency::ForDataSource();
  }
  if (node.is_reroute()) {
    /* The reroute just forwards what is passed in. */
    return OutputFieldDependency::ForDependentField();
  }
  if (node.is_group_input()) {
    /* Input nodes get special treatment in #determine_group_input_states. */
    return OutputFieldDependency::ForDependentField();
  }
  if (node.is_undefined()) {
    return OutputFieldDependency::ForDataSource();
  }
  if (node.type_legacy == NODE_CUSTOM) {
    return OutputFieldDependency::ForDataSource();
  }

  const NodeDeclaration *node_decl = node.declaration();

  /* Node declarations should be implemented for nodes involved here. */
  BLI_assert(node_decl != nullptr);

  /* Use the socket declaration. */
  const SocketDeclaration &socket_decl = *node_decl->outputs[socket.index()];
  return socket_decl.output_field_dependency;
}

static const FieldInferencingInterface &get_dummy_field_inferencing_interface(const bNode &node,
                                                                              ResourceScope &scope)
{
  auto &inferencing_interface = scope.construct<FieldInferencingInterface>();
  inferencing_interface.inputs = Array<InputSocketFieldType>(node.input_sockets().size(),
                                                             InputSocketFieldType::None);
  inferencing_interface.outputs = Array<OutputFieldDependency>(
      node.output_sockets().size(), OutputFieldDependency::ForDataSource());
  return inferencing_interface;
}

/**
 * Retrieves information about how the node interacts with fields.
 * In the future, this information can be stored in the node declaration. This would allow this
 * function to return a reference, making it more efficient.
 */
static const FieldInferencingInterface &get_node_field_inferencing_interface(const bNode &node,
                                                                             ResourceScope &scope)
{
  /* Node groups already reference all required information, so just return that. */
  if (node.is_group()) {
    bNodeTree *group = (bNodeTree *)node.id;
    if (group == nullptr) {
      static const FieldInferencingInterface empty_interface;
      return empty_interface;
    }
    if (!bke::node_tree_is_registered(*group)) {
      /* This can happen when there is a linked node group that was not found (see #92799). */
      return get_dummy_field_inferencing_interface(node, scope);
    }
    if (!group->runtime->field_inferencing_interface) {
      /* This shouldn't happen because referenced node groups should always be updated first. */
      BLI_assert_unreachable();
    }
    return *group->runtime->field_inferencing_interface;
  }

  auto &inferencing_interface = scope.construct<FieldInferencingInterface>();

  const Span<const bNodeSocket *> input_sockets = node.input_sockets();
  inferencing_interface.inputs.reinitialize(input_sockets.size());
  for (const int i : input_sockets.index_range()) {
    inferencing_interface.inputs[i] = get_interface_input_field_type(node, *input_sockets[i]);
  }

  const Span<const bNodeSocket *> output_sockets = node.output_sockets();
  inferencing_interface.outputs.reinitialize(output_sockets.size());
  for (const int i : output_sockets.index_range()) {
    inferencing_interface.outputs[i] = get_interface_output_field_dependency(node,
                                                                             *output_sockets[i]);
  }
  return inferencing_interface;
}

/**
 * This struct contains information for every socket. The values are propagated through the
 * network.
 */
struct SocketFieldState {
  /* This socket starts a new field. */
  bool is_field_source = false;
  /* This socket can never become a field, because the node itself does not support it. */
  bool is_always_single = false;
  /* This socket is currently a single value. It could become a field though. */
  bool is_single = true;
  /* This socket is required to be a single value. This can be because the node itself only
   * supports this socket to be a single value, or because a node afterwards requires this to be a
   * single value. */
  bool requires_single = false;
};

static Vector<const bNodeSocket *> gather_input_socket_dependencies(
    const OutputFieldDependency &field_dependency, const bNode &node)
{
  const OutputSocketFieldType type = field_dependency.field_type();
  Vector<const bNodeSocket *> input_sockets;
  switch (type) {
    case OutputSocketFieldType::FieldSource:
    case OutputSocketFieldType::None: {
      break;
    }
    case OutputSocketFieldType::DependentField: {
      /* This output depends on all inputs. */
      input_sockets.extend(node.input_sockets());
      break;
    }
    case OutputSocketFieldType::PartiallyDependent: {
      /* This output depends only on a few inputs. */
      for (const int i : field_dependency.linked_input_indices()) {
        input_sockets.append(&node.input_socket(i));
      }
      break;
    }
  }
  return input_sockets;
}

/**
 * Check what the group output socket depends on. Potentially traverses the node tree
 * to figure out if it is always a field or if it depends on any group inputs.
 */
static OutputFieldDependency find_group_output_dependencies(
    const bNodeSocket &group_output_socket,
    const Span<const FieldInferencingInterface *> interface_by_node,
    const Span<SocketFieldState> field_state_by_socket_id)
{
  if (!is_field_socket_type(group_output_socket)) {
    return OutputFieldDependency::ForDataSource();
  }

  /* Use a Set here instead of an array indexed by socket id, because we my only need to look at
   * very few sockets. */
  Set<const bNodeSocket *> handled_sockets;
  Stack<const bNodeSocket *> sockets_to_check;

  handled_sockets.add(&group_output_socket);
  sockets_to_check.push(&group_output_socket);

  /* Keeps track of group input indices that are (indirectly) connected to the output. */
  Vector<int> linked_input_indices;

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket *input_socket = sockets_to_check.pop();

    if (!input_socket->is_directly_linked() &&
        !field_state_by_socket_id[input_socket->index_in_tree()].is_single)
    {
      /* This socket uses a field as input by default. */
      return OutputFieldDependency::ForFieldSource();
    }

    for (const bNodeSocket *origin_socket : input_socket->directly_linked_sockets()) {
      const bNode &origin_node = origin_socket->owner_node();
      const SocketFieldState &origin_state =
          field_state_by_socket_id[origin_socket->index_in_tree()];

      if (origin_state.is_field_source) {
        if (origin_node.is_group_input()) {
          /* Found a group input that the group output depends on. */
          linked_input_indices.append_non_duplicates(origin_socket->index());
        }
        else {
          /* Found a field source that is not the group input. So the output is always a field. */
          return OutputFieldDependency::ForFieldSource();
        }
      }
      else if (!origin_state.is_single) {
        const FieldInferencingInterface &inferencing_interface =
            *interface_by_node[origin_node.index()];
        const OutputFieldDependency &field_dependency =
            inferencing_interface.outputs[origin_socket->index()];

        /* Propagate search further to the left. */
        for (const bNodeSocket *origin_input_socket :
             gather_input_socket_dependencies(field_dependency, origin_node))
        {
          if (!origin_input_socket->is_available()) {
            continue;
          }
          if (!field_state_by_socket_id[origin_input_socket->index_in_tree()].is_single) {
            if (handled_sockets.add(origin_input_socket)) {
              sockets_to_check.push(origin_input_socket);
            }
          }
        }
      }
    }
  }
  return OutputFieldDependency::ForPartiallyDependentField(std::move(linked_input_indices));
}

/** Result of syncing two field states. */
enum class FieldStateSyncResult : int8_t {
  /* Nothing changed. */
  NONE = 0,
  /* State A has been modified. */
  CHANGED_A = (1 << 0),
  /* State B has been modified. */
  CHANGED_B = (1 << 1),
};
ENUM_OPERATORS(FieldStateSyncResult)

/**
 * Compare both field states and select the most compatible.
 * Afterwards both field states will be the same.
 * \return FieldStateSyncResult flags indicating which field states have changed.
 */
static FieldStateSyncResult sync_field_states(SocketFieldState &a, SocketFieldState &b)
{
  const bool requires_single = a.requires_single || b.requires_single;
  const bool is_single = a.is_single && b.is_single;

  FieldStateSyncResult res = FieldStateSyncResult::NONE;
  if (a.requires_single != requires_single || a.is_single != is_single) {
    res |= FieldStateSyncResult::CHANGED_A;
  }
  if (b.requires_single != requires_single || b.is_single != is_single) {
    res |= FieldStateSyncResult::CHANGED_B;
  }

  a.requires_single = requires_single;
  b.requires_single = requires_single;
  a.is_single = is_single;
  b.is_single = is_single;

  return res;
}

/**
 * Compare field states of simulation nodes sockets and select the most compatible.
 * Afterwards all field states will be the same.
 * \return FieldStateSyncResult flags indicating which field states have changed.
 */
static FieldStateSyncResult simulation_nodes_field_state_sync(
    const bNode &input_node,
    const bNode &output_node,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  FieldStateSyncResult res = FieldStateSyncResult::NONE;
  for (const int i : output_node.output_sockets().index_range()) {
    /* First input node output is Delta Time which does not appear in the output node outputs. */
    const bNodeSocket &input_socket = input_node.output_socket(i + 1);
    const bNodeSocket &output_socket = output_node.output_socket(i);
    SocketFieldState &input_state = field_state_by_socket_id[input_socket.index_in_tree()];
    SocketFieldState &output_state = field_state_by_socket_id[output_socket.index_in_tree()];
    res |= sync_field_states(input_state, output_state);
  }
  return res;
}

static FieldStateSyncResult repeat_field_state_sync(
    const bNode &input_node,
    const bNode &output_node,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  FieldStateSyncResult res = FieldStateSyncResult::NONE;
  const auto &storage = *static_cast<const NodeGeometryRepeatOutput *>(output_node.storage);
  for (const int i : IndexRange(storage.items_num)) {
    const bNodeSocket &input_socket = input_node.output_socket(i + 1);
    const bNodeSocket &output_socket = output_node.output_socket(i);
    SocketFieldState &input_state = field_state_by_socket_id[input_socket.index_in_tree()];
    SocketFieldState &output_state = field_state_by_socket_id[output_socket.index_in_tree()];
    res |= sync_field_states(input_state, output_state);
  }
  return res;
}

static bool propagate_special_data_requirements(
    const bNodeTree &tree,
    const bNode &node,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  tree.ensure_topology_cache();

  bool need_update = false;

  /* Sync field state between zone nodes and schedule another pass if necessary. */
  switch (node.type_legacy) {
    case GEO_NODE_SIMULATION_INPUT: {
      const auto &data = *static_cast<const NodeGeometrySimulationInput *>(node.storage);
      if (const bNode *output_node = tree.node_by_id(data.output_node_id)) {
        const FieldStateSyncResult sync_result = simulation_nodes_field_state_sync(
            node, *output_node, field_state_by_socket_id);
        if (flag_is_set(sync_result, FieldStateSyncResult::CHANGED_B)) {
          need_update = true;
        }
      }
      break;
    }
    case GEO_NODE_SIMULATION_OUTPUT: {
      for (const bNode *input_node : tree.nodes_by_type("GeometryNodeSimulationInput")) {
        const auto &data = *static_cast<const NodeGeometrySimulationInput *>(input_node->storage);
        if (node.identifier == data.output_node_id) {
          const FieldStateSyncResult sync_result = simulation_nodes_field_state_sync(
              *input_node, node, field_state_by_socket_id);
          if (flag_is_set(sync_result, FieldStateSyncResult::CHANGED_A)) {
            need_update = true;
          }
        }
      }
      break;
    }
    case GEO_NODE_REPEAT_INPUT: {
      const auto &data = *static_cast<const NodeGeometryRepeatInput *>(node.storage);
      if (const bNode *output_node = tree.node_by_id(data.output_node_id)) {
        const FieldStateSyncResult sync_result = repeat_field_state_sync(
            node, *output_node, field_state_by_socket_id);
        if (flag_is_set(sync_result, FieldStateSyncResult::CHANGED_B)) {
          need_update = true;
        }
      }
      break;
    }
    case GEO_NODE_REPEAT_OUTPUT: {
      for (const bNode *input_node : tree.nodes_by_type("GeometryNodeRepeatInput")) {
        const auto &data = *static_cast<const NodeGeometryRepeatInput *>(input_node->storage);
        if (node.identifier == data.output_node_id) {
          const FieldStateSyncResult sync_result = repeat_field_state_sync(
              *input_node, node, field_state_by_socket_id);
          if (flag_is_set(sync_result, FieldStateSyncResult::CHANGED_A)) {
            need_update = true;
          }
        }
      }
      break;
    }
  }

  return need_update;
}

static void propagate_data_requirements_from_right_to_left(
    const bNodeTree &tree,
    const Span<const FieldInferencingInterface *> interface_by_node,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  const Span<const bNode *> toposort_result = tree.toposort_right_to_left();

  while (true) {
    /* Node updates may require several passes due to cyclic dependencies caused by simulation or
     * repeat input/output nodes. */
    bool need_update = false;

    for (const bNode *node : toposort_result) {
      const FieldInferencingInterface &inferencing_interface = *interface_by_node[node->index()];

      for (const bNodeSocket *output_socket : node->output_sockets()) {
        SocketFieldState &state = field_state_by_socket_id[output_socket->index_in_tree()];

        const OutputFieldDependency &field_dependency =
            inferencing_interface.outputs[output_socket->index()];

        if (field_dependency.field_type() == OutputSocketFieldType::FieldSource) {
          continue;
        }
        if (field_dependency.field_type() == OutputSocketFieldType::None) {
          state.requires_single = true;
          state.is_always_single = true;
          continue;
        }

        /* The output is required to be a single value when it is connected to any input that does
         * not support fields. */
        for (const bNodeSocket *target_socket : output_socket->directly_linked_sockets()) {
          if (target_socket->is_available()) {
            state.requires_single |=
                field_state_by_socket_id[target_socket->index_in_tree()].requires_single;
          }
        }

        if (state.requires_single) {
          bool any_input_is_field_implicitly = false;
          const Vector<const bNodeSocket *> connected_inputs = gather_input_socket_dependencies(
              field_dependency, *node);
          for (const bNodeSocket *input_socket : connected_inputs) {
            if (!input_socket->is_available()) {
              continue;
            }
            if (inferencing_interface.inputs[input_socket->index()] ==
                InputSocketFieldType::Implicit)
            {
              if (!input_socket->is_logically_linked()) {
                any_input_is_field_implicitly = true;
                break;
              }
            }
          }
          if (any_input_is_field_implicitly) {
            /* This output isn't a single value actually. */
            state.requires_single = false;
          }
          else {
            /* If the output is required to be a single value, the connected inputs in the same
             * node must not be fields as well. */
            for (const bNodeSocket *input_socket : connected_inputs) {
              field_state_by_socket_id[input_socket->index_in_tree()].requires_single = true;
            }
          }
        }
      }

      /* Some inputs do not require fields independent of what the outputs are connected to. */
      for (const bNodeSocket *input_socket : node->input_sockets()) {
        SocketFieldState &state = field_state_by_socket_id[input_socket->index_in_tree()];
        if (inferencing_interface.inputs[input_socket->index()] == InputSocketFieldType::None) {
          state.requires_single = true;
          state.is_always_single = true;
        }
      }

      /* Find reverse dependencies and resolve conflicts, which may require another pass. */
      if (propagate_special_data_requirements(tree, *node, field_state_by_socket_id)) {
        need_update = true;
      }
    }

    if (!need_update) {
      break;
    }
  }
}

static void determine_group_input_states(
    const bNodeTree &tree,
    FieldInferencingInterface &new_inferencing_interface,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  {
    /* Non-field inputs never support fields. */
    for (const int index : tree.interface_inputs().index_range()) {
      const bNodeTreeInterfaceSocket *group_input = tree.interface_inputs()[index];
      const bNodeSocketType *typeinfo = group_input->socket_typeinfo();
      const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
      if (!nodes::socket_type_supports_fields(type)) {
        new_inferencing_interface.inputs[index] = InputSocketFieldType::None;
      }
      else if (group_input->default_input != NODE_DEFAULT_INPUT_VALUE) {
        new_inferencing_interface.inputs[index] = InputSocketFieldType::Implicit;
      }
      else if (is_layer_selection_field(*group_input)) {
        new_inferencing_interface.inputs[index] = InputSocketFieldType::Implicit;
      }
      else if (group_input->structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE) {
        new_inferencing_interface.inputs[index] = InputSocketFieldType::None;
      }
    }
  }
  /* Check if group inputs are required to be single values, because they are (indirectly)
   * connected to some socket that does not support fields. */
  for (const bNode *node : tree.group_input_nodes()) {
    for (const bNodeSocket *output_socket : node->output_sockets().drop_back(1)) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->index_in_tree()];
      const int output_index = output_socket->index();
      if (state.requires_single) {
        if (new_inferencing_interface.inputs[output_index] == InputSocketFieldType::Implicit) {
          /* Don't override hard-coded implicit fields. */
          continue;
        }
        new_inferencing_interface.inputs[output_index] = InputSocketFieldType::None;
      }
    }
  }
  /* If an input does not support fields, this should be reflected in all Group Input nodes. */
  for (const bNode *node : tree.group_input_nodes()) {
    for (const bNodeSocket *output_socket : node->output_sockets().drop_back(1)) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->index_in_tree()];
      const bool supports_field = new_inferencing_interface.inputs[output_socket->index()] !=
                                  InputSocketFieldType::None;
      if (supports_field) {
        state.is_single = false;
        state.is_field_source = true;
      }
      else {
        state.requires_single = true;
      }
    }
    SocketFieldState &dummy_socket_state =
        field_state_by_socket_id[node->output_sockets().last()->index_in_tree()];
    dummy_socket_state.requires_single = true;
  }
}

static void propagate_field_status_from_left_to_right(
    const bNodeTree &tree,
    const Span<const FieldInferencingInterface *> interface_by_node,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  const Span<const bNode *> toposort_result = tree.toposort_left_to_right();

  while (true) {
    /* Node updates may require several passes due to cyclic dependencies. */
    bool need_update = false;

    for (const bNode *node : toposort_result) {
      if (node->is_group_input()) {
        continue;
      }

      const FieldInferencingInterface &inferencing_interface = *interface_by_node[node->index()];

      /* Update field state of input sockets, also taking into account linked origin sockets. */
      for (const bNodeSocket *input_socket : node->input_sockets()) {
        SocketFieldState &state = field_state_by_socket_id[input_socket->index_in_tree()];
        if (state.is_always_single) {
          state.is_single = true;
          continue;
        }
        state.is_single = true;
        if (!input_socket->is_directly_linked() ||
            all_dangling_reroutes(input_socket->directly_linked_sockets()))
        {
          if (inferencing_interface.inputs[input_socket->index()] ==
              InputSocketFieldType::Implicit)
          {
            state.is_single = false;
          }
        }
        else {
          for (const bNodeSocket *origin_socket : input_socket->directly_linked_sockets()) {
            if (!field_state_by_socket_id[origin_socket->index_in_tree()].is_single) {
              state.is_single = false;
              break;
            }
          }
        }
      }

      /* Update field state of output sockets, also taking into account input sockets. */
      for (const bNodeSocket *output_socket : node->output_sockets()) {
        SocketFieldState &state = field_state_by_socket_id[output_socket->index_in_tree()];
        const OutputFieldDependency &field_dependency =
            inferencing_interface.outputs[output_socket->index()];

        switch (field_dependency.field_type()) {
          case OutputSocketFieldType::None: {
            state.is_single = true;
            break;
          }
          case OutputSocketFieldType::FieldSource: {
            state.is_single = false;
            state.is_field_source = true;
            break;
          }
          case OutputSocketFieldType::PartiallyDependent:
          case OutputSocketFieldType::DependentField: {
            for (const bNodeSocket *input_socket :
                 gather_input_socket_dependencies(field_dependency, *node))
            {
              if (!input_socket->is_available()) {
                continue;
              }
              if (!field_state_by_socket_id[input_socket->index_in_tree()].is_single) {
                state.is_single = false;
                break;
              }
            }
            break;
          }
        }
      }

      /* Find reverse dependencies and resolve conflicts, which may require another pass. */
      if (propagate_special_data_requirements(tree, *node, field_state_by_socket_id)) {
        need_update = true;
      }
    }

    if (!need_update) {
      break;
    }
  }
}

static void determine_group_output_states(
    const bNodeTree &tree,
    FieldInferencingInterface &new_inferencing_interface,
    const Span<const FieldInferencingInterface *> interface_by_node,
    const Span<SocketFieldState> field_state_by_socket_id)
{
  const bNode *group_output_node = tree.group_output_node();
  if (!group_output_node) {
    return;
  }

  for (const bNodeSocket *group_output_socket : group_output_node->input_sockets().drop_back(1)) {
    OutputFieldDependency field_dependency = find_group_output_dependencies(
        *group_output_socket, interface_by_node, field_state_by_socket_id);
    new_inferencing_interface.outputs[group_output_socket->index()] = std::move(field_dependency);
  }
}

static Array<FieldSocketState> calc_socket_states(
    const Span<SocketFieldState> field_state_by_socket_id)
{
  auto get_state_to_store = [&](const SocketFieldState &state) {
    if (state.is_always_single) {
      return FieldSocketState::RequiresSingle;
    }
    if (!state.is_single) {
      return FieldSocketState::IsField;
    }
    if (state.requires_single) {
      return FieldSocketState::RequiresSingle;
    }
    return FieldSocketState::CanBeField;
  };

  Array<FieldSocketState> result(field_state_by_socket_id.size());
  for (const int i : field_state_by_socket_id.index_range()) {
    result[i] = get_state_to_store(field_state_by_socket_id[i]);
  }
  return result;
}

static void prepare_inferencing_interfaces(
    const Span<const bNode *> nodes,
    MutableSpan<const FieldInferencingInterface *> interface_by_node,
    ResourceScope &scope)
{
  for (const int i : nodes.index_range()) {
    interface_by_node[i] = &get_node_field_inferencing_interface(*nodes[i], scope);
  }
}

bool update_field_inferencing(const bNodeTree &tree)
{
  BLI_assert(tree.type == NTREE_GEOMETRY);
  tree.ensure_topology_cache();
  tree.ensure_interface_cache();

  const Span<const bNode *> nodes = tree.all_nodes();
  ResourceScope scope;
  Array<const FieldInferencingInterface *> interface_by_node(nodes.size());
  prepare_inferencing_interfaces(nodes, interface_by_node, scope);

  /* Create new inferencing interface for this node group. */
  std::unique_ptr<FieldInferencingInterface> new_inferencing_interface =
      std::make_unique<FieldInferencingInterface>();
  new_inferencing_interface->inputs = Array<InputSocketFieldType>(
      tree.interface_inputs().size(), InputSocketFieldType::IsSupported);
  new_inferencing_interface->outputs = Array<OutputFieldDependency>(
      tree.interface_outputs().size(), OutputFieldDependency::ForDataSource());

  /* Keep track of the state of all sockets. The index into this array is #SocketRef::id(). */
  Array<SocketFieldState> field_state_by_socket_id(tree.all_sockets().size());

  propagate_data_requirements_from_right_to_left(
      tree, interface_by_node, field_state_by_socket_id);
  determine_group_input_states(tree, *new_inferencing_interface, field_state_by_socket_id);
  propagate_field_status_from_left_to_right(tree, interface_by_node, field_state_by_socket_id);
  determine_group_output_states(
      tree, *new_inferencing_interface, interface_by_node, field_state_by_socket_id);

  /* Update the previous group interface. */
  const bool group_interface_changed = !tree.runtime->field_inferencing_interface ||
                                       *tree.runtime->field_inferencing_interface !=
                                           *new_inferencing_interface;
  tree.runtime->field_inferencing_interface = std::move(new_inferencing_interface);
  tree.runtime->field_states = calc_socket_states(field_state_by_socket_id);

  return group_interface_changed;
}

}  // namespace blender::bke::node_field_inferencing
