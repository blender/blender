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

#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_noise.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_vector_set.hh"

#include "DNA_anim_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_anim_data.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"

#include "MOD_nodes.h"

#include "NOD_node_declaration.hh"
#include "NOD_node_tree_ref.hh"
#include "NOD_texture.h"

#include "DEG_depsgraph_query.h"

using namespace blender::nodes;

/**
 * These flags are used by the `changed_flag` field in #bNodeTree, #bNode and #bNodeSocket.
 * This enum is not part of the public api. It should be used through the `BKE_ntree_update_tag_*`
 * api.
 */
enum eNodeTreeChangedFlag {
  NTREE_CHANGED_NOTHING = 0,
  NTREE_CHANGED_ANY = (1 << 1),
  NTREE_CHANGED_NODE_PROPERTY = (1 << 2),
  NTREE_CHANGED_NODE_OUTPUT = (1 << 3),
  NTREE_CHANGED_INTERFACE = (1 << 4),
  NTREE_CHANGED_LINK = (1 << 5),
  NTREE_CHANGED_REMOVED_NODE = (1 << 6),
  NTREE_CHANGED_REMOVED_SOCKET = (1 << 7),
  NTREE_CHANGED_SOCKET_PROPERTY = (1 << 8),
  NTREE_CHANGED_INTERNAL_LINK = (1 << 9),
  NTREE_CHANGED_ALL = -1,
};

static void add_tree_tag(bNodeTree *ntree, const eNodeTreeChangedFlag flag)
{
  ntree->changed_flag |= flag;
}

static void add_node_tag(bNodeTree *ntree, bNode *node, const eNodeTreeChangedFlag flag)
{
  add_tree_tag(ntree, flag);
  node->changed_flag |= flag;
}

static void add_socket_tag(bNodeTree *ntree, bNodeSocket *socket, const eNodeTreeChangedFlag flag)
{
  add_tree_tag(ntree, flag);
  socket->changed_flag |= flag;
}

namespace blender::bke {

namespace node_field_inferencing {

static bool is_field_socket_type(eNodeSocketDatatype type)
{
  return ELEM(type, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_VECTOR, SOCK_RGBA);
}

static bool is_field_socket_type(const SocketRef &socket)
{
  return is_field_socket_type((eNodeSocketDatatype)socket.typeinfo()->type);
}

static InputSocketFieldType get_interface_input_field_type(const NodeRef &node,
                                                           const InputSocketRef &socket)
{
  if (!is_field_socket_type(socket)) {
    return InputSocketFieldType::None;
  }
  if (node.is_reroute_node()) {
    return InputSocketFieldType::IsSupported;
  }
  if (node.is_group_output_node()) {
    /* Outputs always support fields when the data type is correct. */
    return InputSocketFieldType::IsSupported;
  }
  if (node.is_undefined()) {
    return InputSocketFieldType::None;
  }

  const NodeDeclaration *node_decl = node.declaration();

  /* Node declarations should be implemented for nodes involved here. */
  BLI_assert(node_decl != nullptr);

  /* Get the field type from the declaration. */
  const SocketDeclaration &socket_decl = *node_decl->inputs()[socket.index()];
  const InputSocketFieldType field_type = socket_decl.input_field_type();
  if (field_type == InputSocketFieldType::Implicit) {
    return field_type;
  }
  if (node_decl->is_function_node()) {
    /* In a function node, every socket supports fields. */
    return InputSocketFieldType::IsSupported;
  }
  return field_type;
}

static OutputFieldDependency get_interface_output_field_dependency(const NodeRef &node,
                                                                   const OutputSocketRef &socket)
{
  if (!is_field_socket_type(socket)) {
    /* Non-field sockets always output data. */
    return OutputFieldDependency::ForDataSource();
  }
  if (node.is_reroute_node()) {
    /* The reroute just forwards what is passed in. */
    return OutputFieldDependency::ForDependentField();
  }
  if (node.is_group_input_node()) {
    /* Input nodes get special treatment in #determine_group_input_states. */
    return OutputFieldDependency::ForDependentField();
  }
  if (node.is_undefined()) {
    return OutputFieldDependency::ForDataSource();
  }

  const NodeDeclaration *node_decl = node.declaration();

  /* Node declarations should be implemented for nodes involved here. */
  BLI_assert(node_decl != nullptr);

  if (node_decl->is_function_node()) {
    /* In a generic function node, all outputs depend on all inputs. */
    return OutputFieldDependency::ForDependentField();
  }

  /* Use the socket declaration. */
  const SocketDeclaration &socket_decl = *node_decl->outputs()[socket.index()];
  return socket_decl.output_field_dependency();
}

static FieldInferencingInterface get_dummy_field_inferencing_interface(const NodeRef &node)
{
  FieldInferencingInterface inferencing_interface;
  inferencing_interface.inputs.append_n_times(InputSocketFieldType::None, node.inputs().size());
  inferencing_interface.outputs.append_n_times(OutputFieldDependency::ForDataSource(),
                                               node.outputs().size());
  return inferencing_interface;
}

/**
 * Retrieves information about how the node interacts with fields.
 * In the future, this information can be stored in the node declaration. This would allow this
 * function to return a reference, making it more efficient.
 */
static FieldInferencingInterface get_node_field_inferencing_interface(const NodeRef &node)
{
  /* Node groups already reference all required information, so just return that. */
  if (node.is_group_node()) {
    bNodeTree *group = (bNodeTree *)node.bnode()->id;
    if (group == nullptr) {
      return FieldInferencingInterface();
    }
    if (!ntreeIsRegistered(group)) {
      /* This can happen when there is a linked node group that was not found (see T92799). */
      return get_dummy_field_inferencing_interface(node);
    }
    if (group->field_inferencing_interface == nullptr) {
      /* This shouldn't happen because referenced node groups should always be updated first. */
      BLI_assert_unreachable();
    }
    return *group->field_inferencing_interface;
  }

  FieldInferencingInterface inferencing_interface;
  for (const InputSocketRef *input_socket : node.inputs()) {
    inferencing_interface.inputs.append(get_interface_input_field_type(node, *input_socket));
  }

  for (const OutputSocketRef *output_socket : node.outputs()) {
    inferencing_interface.outputs.append(
        get_interface_output_field_dependency(node, *output_socket));
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

static Vector<const InputSocketRef *> gather_input_socket_dependencies(
    const OutputFieldDependency &field_dependency, const NodeRef &node)
{
  const OutputSocketFieldType type = field_dependency.field_type();
  Vector<const InputSocketRef *> input_sockets;
  switch (type) {
    case OutputSocketFieldType::FieldSource:
    case OutputSocketFieldType::None: {
      break;
    }
    case OutputSocketFieldType::DependentField: {
      /* This output depends on all inputs. */
      input_sockets.extend(node.inputs());
      break;
    }
    case OutputSocketFieldType::PartiallyDependent: {
      /* This output depends only on a few inputs. */
      for (const int i : field_dependency.linked_input_indices()) {
        input_sockets.append(&node.input(i));
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
    const InputSocketRef &group_output_socket,
    const Span<SocketFieldState> field_state_by_socket_id)
{
  if (!is_field_socket_type(group_output_socket)) {
    return OutputFieldDependency::ForDataSource();
  }

  /* Use a Set here instead of an array indexed by socket id, because we my only need to look at
   * very few sockets. */
  Set<const InputSocketRef *> handled_sockets;
  Stack<const InputSocketRef *> sockets_to_check;

  handled_sockets.add(&group_output_socket);
  sockets_to_check.push(&group_output_socket);

  /* Keeps track of group input indices that are (indirectly) connected to the output. */
  Vector<int> linked_input_indices;

  while (!sockets_to_check.is_empty()) {
    const InputSocketRef *input_socket = sockets_to_check.pop();

    if (!input_socket->is_directly_linked() &&
        !field_state_by_socket_id[input_socket->id()].is_single) {
      /* This socket uses a field as input by default. */
      return OutputFieldDependency::ForFieldSource();
    }

    for (const OutputSocketRef *origin_socket : input_socket->directly_linked_sockets()) {
      const NodeRef &origin_node = origin_socket->node();
      const SocketFieldState &origin_state = field_state_by_socket_id[origin_socket->id()];

      if (origin_state.is_field_source) {
        if (origin_node.is_group_input_node()) {
          /* Found a group input that the group output depends on. */
          linked_input_indices.append_non_duplicates(origin_socket->index());
        }
        else {
          /* Found a field source that is not the group input. So the output is always a field. */
          return OutputFieldDependency::ForFieldSource();
        }
      }
      else if (!origin_state.is_single) {
        const FieldInferencingInterface inferencing_interface =
            get_node_field_inferencing_interface(origin_node);
        const OutputFieldDependency &field_dependency =
            inferencing_interface.outputs[origin_socket->index()];

        /* Propagate search further to the left. */
        for (const InputSocketRef *origin_input_socket :
             gather_input_socket_dependencies(field_dependency, origin_node)) {
          if (!origin_input_socket->is_available()) {
            continue;
          }
          if (!field_state_by_socket_id[origin_input_socket->id()].is_single) {
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

static void propagate_data_requirements_from_right_to_left(
    const NodeTreeRef &tree, const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  const NodeTreeRef::ToposortResult toposort_result = tree.toposort(
      NodeTreeRef::ToposortDirection::RightToLeft);

  for (const NodeRef *node : toposort_result.sorted_nodes) {
    const FieldInferencingInterface inferencing_interface = get_node_field_inferencing_interface(
        *node);

    for (const OutputSocketRef *output_socket : node->outputs()) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];

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
      for (const InputSocketRef *target_socket : output_socket->directly_linked_sockets()) {
        if (target_socket->is_available()) {
          state.requires_single |= field_state_by_socket_id[target_socket->id()].requires_single;
        }
      }

      if (state.requires_single) {
        bool any_input_is_field_implicitly = false;
        const Vector<const InputSocketRef *> connected_inputs = gather_input_socket_dependencies(
            field_dependency, *node);
        for (const InputSocketRef *input_socket : connected_inputs) {
          if (!input_socket->is_available()) {
            continue;
          }
          if (inferencing_interface.inputs[input_socket->index()] ==
              InputSocketFieldType::Implicit) {
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
          /* If the output is required to be a single value, the connected inputs in the same node
           * must not be fields as well. */
          for (const InputSocketRef *input_socket : connected_inputs) {
            field_state_by_socket_id[input_socket->id()].requires_single = true;
          }
        }
      }
    }

    /* Some inputs do not require fields independent of what the outputs are connected to. */
    for (const InputSocketRef *input_socket : node->inputs()) {
      SocketFieldState &state = field_state_by_socket_id[input_socket->id()];
      if (inferencing_interface.inputs[input_socket->index()] == InputSocketFieldType::None) {
        state.requires_single = true;
        state.is_always_single = true;
      }
    }
  }
}

static void determine_group_input_states(
    const NodeTreeRef &tree,
    FieldInferencingInterface &new_inferencing_interface,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  {
    /* Non-field inputs never support fields. */
    int index;
    LISTBASE_FOREACH_INDEX (bNodeSocket *, group_input, &tree.btree()->inputs, index) {
      if (!is_field_socket_type((eNodeSocketDatatype)group_input->type)) {
        new_inferencing_interface.inputs[index] = InputSocketFieldType::None;
      }
    }
  }
  /* Check if group inputs are required to be single values, because they are (indirectly)
   * connected to some socket that does not support fields. */
  for (const NodeRef *node : tree.nodes_by_type("NodeGroupInput")) {
    for (const OutputSocketRef *output_socket : node->outputs().drop_back(1)) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];
      if (state.requires_single) {
        new_inferencing_interface.inputs[output_socket->index()] = InputSocketFieldType::None;
      }
    }
  }
  /* If an input does not support fields, this should be reflected in all Group Input nodes. */
  for (const NodeRef *node : tree.nodes_by_type("NodeGroupInput")) {
    for (const OutputSocketRef *output_socket : node->outputs().drop_back(1)) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];
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
    SocketFieldState &dummy_socket_state = field_state_by_socket_id[node->outputs().last()->id()];
    dummy_socket_state.requires_single = true;
  }
}

static void propagate_field_status_from_left_to_right(
    const NodeTreeRef &tree, const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  const NodeTreeRef::ToposortResult toposort_result = tree.toposort(
      NodeTreeRef::ToposortDirection::LeftToRight);

  for (const NodeRef *node : toposort_result.sorted_nodes) {
    if (node->is_group_input_node()) {
      continue;
    }

    const FieldInferencingInterface inferencing_interface = get_node_field_inferencing_interface(
        *node);

    /* Update field state of input sockets, also taking into account linked origin sockets. */
    for (const InputSocketRef *input_socket : node->inputs()) {
      SocketFieldState &state = field_state_by_socket_id[input_socket->id()];
      if (state.is_always_single) {
        state.is_single = true;
        continue;
      }
      state.is_single = true;
      if (input_socket->directly_linked_sockets().is_empty()) {
        if (inferencing_interface.inputs[input_socket->index()] ==
            InputSocketFieldType::Implicit) {
          state.is_single = false;
        }
      }
      else {
        for (const OutputSocketRef *origin_socket : input_socket->directly_linked_sockets()) {
          if (!field_state_by_socket_id[origin_socket->id()].is_single) {
            state.is_single = false;
            break;
          }
        }
      }
    }

    /* Update field state of output sockets, also taking into account input sockets. */
    for (const OutputSocketRef *output_socket : node->outputs()) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];
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
          for (const InputSocketRef *input_socket :
               gather_input_socket_dependencies(field_dependency, *node)) {
            if (!input_socket->is_available()) {
              continue;
            }
            if (!field_state_by_socket_id[input_socket->id()].is_single) {
              state.is_single = false;
              break;
            }
          }
          break;
        }
      }
    }
  }
}

static void determine_group_output_states(const NodeTreeRef &tree,
                                          FieldInferencingInterface &new_inferencing_interface,
                                          const Span<SocketFieldState> field_state_by_socket_id)
{
  for (const NodeRef *group_output_node : tree.nodes_by_type("NodeGroupOutput")) {
    /* Ignore inactive group output nodes. */
    if (!(group_output_node->bnode()->flag & NODE_DO_OUTPUT)) {
      continue;
    }
    /* Determine dependencies of all group outputs. */
    for (const InputSocketRef *group_output_socket : group_output_node->inputs().drop_back(1)) {
      OutputFieldDependency field_dependency = find_group_output_dependencies(
          *group_output_socket, field_state_by_socket_id);
      new_inferencing_interface.outputs[group_output_socket->index()] = std::move(
          field_dependency);
    }
    break;
  }
}

static void update_socket_shapes(const NodeTreeRef &tree,
                                 const Span<SocketFieldState> field_state_by_socket_id)
{
  const eNodeSocketDisplayShape requires_data_shape = SOCK_DISPLAY_SHAPE_CIRCLE;
  const eNodeSocketDisplayShape data_but_can_be_field_shape = SOCK_DISPLAY_SHAPE_DIAMOND_DOT;
  const eNodeSocketDisplayShape is_field_shape = SOCK_DISPLAY_SHAPE_DIAMOND;

  auto get_shape_for_state = [&](const SocketFieldState &state) {
    if (state.is_always_single) {
      return requires_data_shape;
    }
    if (!state.is_single) {
      return is_field_shape;
    }
    if (state.requires_single) {
      return requires_data_shape;
    }
    return data_but_can_be_field_shape;
  };

  for (const InputSocketRef *socket : tree.input_sockets()) {
    bNodeSocket *bsocket = socket->bsocket();
    const SocketFieldState &state = field_state_by_socket_id[socket->id()];
    bsocket->display_shape = get_shape_for_state(state);
  }
  for (const OutputSocketRef *socket : tree.output_sockets()) {
    bNodeSocket *bsocket = socket->bsocket();
    const SocketFieldState &state = field_state_by_socket_id[socket->id()];
    bsocket->display_shape = get_shape_for_state(state);
  }
}

static bool update_field_inferencing(const NodeTreeRef &tree)
{
  bNodeTree &btree = *tree.btree();

  /* Create new inferencing interface for this node group. */
  FieldInferencingInterface *new_inferencing_interface = new FieldInferencingInterface();
  new_inferencing_interface->inputs.resize(BLI_listbase_count(&btree.inputs),
                                           InputSocketFieldType::IsSupported);
  new_inferencing_interface->outputs.resize(BLI_listbase_count(&btree.outputs),
                                            OutputFieldDependency::ForDataSource());

  /* Keep track of the state of all sockets. The index into this array is #SocketRef::id(). */
  Array<SocketFieldState> field_state_by_socket_id(tree.sockets().size());

  propagate_data_requirements_from_right_to_left(tree, field_state_by_socket_id);
  determine_group_input_states(tree, *new_inferencing_interface, field_state_by_socket_id);
  propagate_field_status_from_left_to_right(tree, field_state_by_socket_id);
  determine_group_output_states(tree, *new_inferencing_interface, field_state_by_socket_id);
  update_socket_shapes(tree, field_state_by_socket_id);

  /* Update the previous group interface. */
  const bool group_interface_changed = btree.field_inferencing_interface == nullptr ||
                                       *btree.field_inferencing_interface !=
                                           *new_inferencing_interface;
  delete btree.field_inferencing_interface;
  btree.field_inferencing_interface = new_inferencing_interface;

  return group_interface_changed;
}

}  // namespace node_field_inferencing

/**
 * Common datatype priorities, works for compositor, shader and texture nodes alike
 * defines priority of datatype connection based on output type (to):
 * `<  0`: never connect these types.
 * `>= 0`: priority of connection (higher values chosen first).
 */
static int get_internal_link_type_priority(const bNodeSocketType *from, const bNodeSocketType *to)
{
  switch (to->type) {
    case SOCK_RGBA:
      switch (from->type) {
        case SOCK_RGBA:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_INT:
          return 2;
        case SOCK_BOOLEAN:
          return 1;
      }
      return -1;
    case SOCK_VECTOR:
      switch (from->type) {
        case SOCK_VECTOR:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_INT:
          return 2;
        case SOCK_BOOLEAN:
          return 1;
      }
      return -1;
    case SOCK_FLOAT:
      switch (from->type) {
        case SOCK_FLOAT:
          return 5;
        case SOCK_INT:
          return 4;
        case SOCK_BOOLEAN:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
      }
      return -1;
    case SOCK_INT:
      switch (from->type) {
        case SOCK_INT:
          return 5;
        case SOCK_FLOAT:
          return 4;
        case SOCK_BOOLEAN:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
      }
      return -1;
    case SOCK_BOOLEAN:
      switch (from->type) {
        case SOCK_BOOLEAN:
          return 5;
        case SOCK_INT:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
      }
      return -1;
  }

  /* The rest of the socket types only allow an internal link if both the input and output socket
   * have the same type. If the sockets are custom, we check the idname instead. */
  if (to->type == from->type && (to->type != SOCK_CUSTOM || STREQ(to->idname, from->idname))) {
    return 1;
  }

  return -1;
}

using TreeNodePair = std::pair<bNodeTree *, bNode *>;
using ObjectModifierPair = std::pair<Object *, ModifierData *>;
using NodeSocketPair = std::pair<bNode *, bNodeSocket *>;

/**
 * Cache common data about node trees from the #Main database that is expensive to retrieve on
 * demand every time.
 */
struct NodeTreeRelations {
 private:
  Main *bmain_;
  std::optional<Vector<bNodeTree *>> all_trees_;
  std::optional<Map<bNodeTree *, ID *>> owner_ids_;
  std::optional<MultiValueMap<bNodeTree *, TreeNodePair>> group_node_users_;
  std::optional<MultiValueMap<bNodeTree *, ObjectModifierPair>> modifiers_users_;

 public:
  NodeTreeRelations(Main *bmain) : bmain_(bmain)
  {
  }

  void ensure_all_trees()
  {
    if (all_trees_.has_value()) {
      return;
    }
    all_trees_.emplace();
    owner_ids_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      all_trees_->append(ntree);
      if (&ntree->id != id) {
        owner_ids_->add_new(ntree, id);
      }
    }
    FOREACH_NODETREE_END;
  }

  void ensure_owner_ids()
  {
    this->ensure_all_trees();
  }

  void ensure_group_node_users()
  {
    if (group_node_users_.has_value()) {
      return;
    }
    group_node_users_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    this->ensure_all_trees();

    for (bNodeTree *ntree : *all_trees_) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->id == nullptr) {
          continue;
        }
        ID *id = node->id;
        if (GS(id->name) == ID_NT) {
          bNodeTree *group = (bNodeTree *)id;
          group_node_users_->add(group, {ntree, node});
        }
      }
    }
  }

  void ensure_modifier_users()
  {
    if (modifiers_users_.has_value()) {
      return;
    }
    modifiers_users_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    LISTBASE_FOREACH (Object *, object, &bmain_->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Nodes) {
          NodesModifierData *nmd = (NodesModifierData *)md;
          if (nmd->node_group != nullptr) {
            modifiers_users_->add(nmd->node_group, {object, md});
          }
        }
      }
    }
  }

  Span<ObjectModifierPair> get_modifier_users(bNodeTree *ntree)
  {
    BLI_assert(modifiers_users_.has_value());
    return modifiers_users_->lookup(ntree);
  }

  Span<TreeNodePair> get_group_node_users(bNodeTree *ntree)
  {
    BLI_assert(group_node_users_.has_value());
    return group_node_users_->lookup(ntree);
  }

  ID *get_owner_id(bNodeTree *ntree)
  {
    BLI_assert(owner_ids_.has_value());
    return owner_ids_->lookup_default(ntree, &ntree->id);
  }
};

struct TreeUpdateResult {
  bool interface_changed = false;
  bool output_changed = false;
};

class NodeTreeMainUpdater {
 private:
  Main *bmain_;
  NodeTreeUpdateExtraParams *params_;
  Map<bNodeTree *, TreeUpdateResult> update_result_by_tree_;
  NodeTreeRelations relations_;

 public:
  NodeTreeMainUpdater(Main *bmain, NodeTreeUpdateExtraParams *params)
      : bmain_(bmain), params_(params), relations_(bmain)
  {
  }

  void update()
  {
    Vector<bNodeTree *> changed_ntrees;
    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      if (ntree->changed_flag != NTREE_CHANGED_NOTHING) {
        changed_ntrees.append(ntree);
      }
    }
    FOREACH_NODETREE_END;
    this->update_rooted(changed_ntrees);
  }

  void update_rooted(Span<bNodeTree *> root_ntrees)
  {
    if (root_ntrees.is_empty()) {
      return;
    }

    bool is_single_tree_update = false;

    if (root_ntrees.size() == 1) {
      bNodeTree *ntree = root_ntrees[0];
      if (ntree->changed_flag == NTREE_CHANGED_NOTHING) {
        return;
      }
      const TreeUpdateResult result = this->update_tree(*ntree);
      update_result_by_tree_.add_new(ntree, result);
      if (!result.interface_changed && !result.output_changed) {
        is_single_tree_update = true;
      }
    }

    if (!is_single_tree_update) {
      Vector<bNodeTree *> ntrees_in_order = this->get_tree_update_order(root_ntrees);
      for (bNodeTree *ntree : ntrees_in_order) {
        if (ntree->changed_flag == NTREE_CHANGED_NOTHING) {
          continue;
        }
        if (!update_result_by_tree_.contains(ntree)) {
          const TreeUpdateResult result = this->update_tree(*ntree);
          update_result_by_tree_.add_new(ntree, result);
        }
        const TreeUpdateResult result = update_result_by_tree_.lookup(ntree);
        Span<TreeNodePair> dependent_trees = relations_.get_group_node_users(ntree);
        if (result.output_changed) {
          for (const TreeNodePair &pair : dependent_trees) {
            add_node_tag(pair.first, pair.second, NTREE_CHANGED_NODE_OUTPUT);
          }
        }
        if (result.interface_changed) {
          for (const TreeNodePair &pair : dependent_trees) {
            add_node_tag(pair.first, pair.second, NTREE_CHANGED_NODE_PROPERTY);
          }
        }
      }
    }

    for (const auto item : update_result_by_tree_.items()) {
      bNodeTree *ntree = item.key;
      const TreeUpdateResult &result = item.value;

      this->reset_changed_flags(*ntree);

      if (result.interface_changed) {
        if (ntree->type == NTREE_GEOMETRY) {
          relations_.ensure_modifier_users();
          for (const ObjectModifierPair &pair : relations_.get_modifier_users(ntree)) {
            Object *object = pair.first;
            ModifierData *md = pair.second;

            if (md->type == eModifierType_Nodes) {
              MOD_nodes_update_interface(object, (NodesModifierData *)md);
            }
          }
        }
      }

      if (params_) {
        relations_.ensure_owner_ids();
        ID *id = relations_.get_owner_id(ntree);
        if (params_->tree_changed_fn) {
          params_->tree_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_output_changed_fn && result.output_changed) {
          params_->tree_output_changed_fn(id, ntree, params_->user_data);
        }
      }
    }
  }

 private:
  enum class ToposortMark {
    None,
    Temporary,
    Permanent,
  };

  using ToposortMarkMap = Map<bNodeTree *, ToposortMark>;

  /**
   * Finds all trees that depend on the given trees (through node groups). Then those trees are
   * ordered such that all trees used by one tree come before it.
   */
  Vector<bNodeTree *> get_tree_update_order(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_group_node_users();

    Set<bNodeTree *> trees_to_update = get_trees_to_update(root_ntrees);

    Vector<bNodeTree *> sorted_ntrees;

    ToposortMarkMap marks;
    for (bNodeTree *ntree : trees_to_update) {
      marks.add_new(ntree, ToposortMark::None);
    }
    for (bNodeTree *ntree : trees_to_update) {
      if (marks.lookup(ntree) == ToposortMark::None) {
        const bool cycle_detected = !this->get_tree_update_order__visit_recursive(
            ntree, marks, sorted_ntrees);
        /* This should be prevented by higher level operators. */
        BLI_assert(!cycle_detected);
        UNUSED_VARS_NDEBUG(cycle_detected);
      }
    }

    std::reverse(sorted_ntrees.begin(), sorted_ntrees.end());

    return sorted_ntrees;
  }

  bool get_tree_update_order__visit_recursive(bNodeTree *ntree,
                                              ToposortMarkMap &marks,
                                              Vector<bNodeTree *> &sorted_ntrees)
  {
    ToposortMark &mark = marks.lookup(ntree);
    if (mark == ToposortMark::Permanent) {
      return true;
    }
    if (mark == ToposortMark::Temporary) {
      /* There is a dependency cycle. */
      return false;
    }

    mark = ToposortMark::Temporary;

    for (const TreeNodePair &pair : relations_.get_group_node_users(ntree)) {
      this->get_tree_update_order__visit_recursive(pair.first, marks, sorted_ntrees);
    }
    sorted_ntrees.append(ntree);

    mark = ToposortMark::Permanent;
    return true;
  }

  Set<bNodeTree *> get_trees_to_update(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_group_node_users();

    Set<bNodeTree *> reachable_trees;
    VectorSet<bNodeTree *> trees_to_check = root_ntrees;

    while (!trees_to_check.is_empty()) {
      bNodeTree *ntree = trees_to_check.pop();
      if (reachable_trees.add(ntree)) {
        for (const TreeNodePair &pair : relations_.get_group_node_users(ntree)) {
          trees_to_check.add(pair.first);
        }
      }
    }

    return reachable_trees;
  }

  TreeUpdateResult update_tree(bNodeTree &ntree)
  {
    TreeUpdateResult result;

    /* Use a #NodeTreeRef to speedup certain queries. It is rebuilt whenever the node tree topology
     * changes, which typically happens zero or one times during the entire update of the node
     * tree. */
    std::unique_ptr<NodeTreeRef> tree_ref;
    this->ensure_tree_ref(ntree, tree_ref);

    this->update_socket_link_and_use(*tree_ref);
    this->update_individual_nodes(ntree, tree_ref);
    this->update_internal_links(ntree, tree_ref);
    this->update_generic_callback(ntree, tree_ref);
    this->remove_unused_previews_when_necessary(ntree);

    this->ensure_tree_ref(ntree, tree_ref);
    if (ntree.type == NTREE_GEOMETRY) {
      if (node_field_inferencing::update_field_inferencing(*tree_ref)) {
        result.interface_changed = true;
      }
    }

    result.output_changed = this->check_if_output_changed(*tree_ref);

    this->update_socket_link_and_use(*tree_ref);
    this->update_node_levels(ntree);
    this->update_link_validation(ntree);

    if (ntree.type == NTREE_TEXTURE) {
      ntreeTexCheckCyclics(&ntree);
    }

    if (ntree.changed_flag & NTREE_CHANGED_INTERFACE || ntree.changed_flag & NTREE_CHANGED_ANY) {
      result.interface_changed = true;
    }

    if (result.interface_changed) {
      ntreeInterfaceTypeUpdate(&ntree);
    }

    return result;
  }

  void ensure_tree_ref(bNodeTree &ntree, std::unique_ptr<NodeTreeRef> &tree_ref)
  {
    if (!tree_ref) {
      tree_ref = std::make_unique<NodeTreeRef>(&ntree);
    }
  }

  void update_socket_link_and_use(const NodeTreeRef &tree)
  {
    for (const InputSocketRef *socket : tree.input_sockets()) {
      bNodeSocket *bsocket = socket->bsocket();
      if (socket->directly_linked_links().is_empty()) {
        bsocket->link = nullptr;
      }
      else {
        bsocket->link = socket->directly_linked_links()[0]->blink();
      }
    }

    this->update_socket_used_tags(tree);
  }

  void update_socket_used_tags(const NodeTreeRef &tree)
  {
    for (const SocketRef *socket : tree.sockets()) {
      bNodeSocket *bsocket = socket->bsocket();
      bsocket->flag &= ~SOCK_IN_USE;
      for (const LinkRef *link : socket->directly_linked_links()) {
        if (!link->is_muted()) {
          bsocket->flag |= SOCK_IN_USE;
          break;
        }
      }
    }
  }

  void update_individual_nodes(bNodeTree &ntree, std::unique_ptr<NodeTreeRef> &tree_ref)
  {
    /* Iterate over nodes instead of #NodeTreeRef, because the #tree_ref might be outdated after
     * some update functions. */
    LISTBASE_FOREACH (bNode *, bnode, &ntree.nodes) {
      this->ensure_tree_ref(ntree, tree_ref);
      const NodeRef &node = *tree_ref->find_node(*bnode);
      if (this->should_update_individual_node(node)) {
        const uint32_t old_changed_flag = ntree.changed_flag;
        ntree.changed_flag = NTREE_CHANGED_NOTHING;

        /* This may set #ntree.changed_flag which is detected below. */
        this->update_individual_node(node);

        if (ntree.changed_flag != NTREE_CHANGED_NOTHING) {
          /* The tree ref is outdated and needs to be rebuilt. Generally, only very few update
           * functions change the node. Typically zero or one nodes change after an update. */
          tree_ref.reset();
        }
        ntree.changed_flag |= old_changed_flag;
      }
    }
  }

  bool should_update_individual_node(const NodeRef &node)
  {
    bNodeTree &ntree = *node.btree();
    bNode &bnode = *node.bnode();
    if (ntree.changed_flag & NTREE_CHANGED_ANY) {
      return true;
    }
    if (bnode.changed_flag & NTREE_CHANGED_NODE_PROPERTY) {
      return true;
    }
    if (ntree.changed_flag & NTREE_CHANGED_LINK) {
      /* Node groups currently always rebuilt their sockets when they are updated.
       * So avoid calling the update method when no new link was added to it. */
      if (node.is_group_input_node()) {
        if (node.outputs().last()->is_directly_linked()) {
          return true;
        }
      }
      else if (node.is_group_output_node()) {
        if (node.inputs().last()->is_directly_linked()) {
          return true;
        }
      }
      else {
        /* Currently we have no way to tell if a node needs to be updated when a link changed. */
        return true;
      }
    }
    if (ntree.changed_flag & NTREE_CHANGED_INTERFACE) {
      if (node.is_group_input_node() || node.is_group_output_node()) {
        return true;
      }
    }
    return false;
  }

  void update_individual_node(const NodeRef &node)
  {
    bNodeTree &ntree = *node.btree();
    bNode &bnode = *node.bnode();
    bNodeType &ntype = *bnode.typeinfo;
    if (ntype.group_update_func) {
      ntype.group_update_func(&ntree, &bnode);
    }
    if (ntype.updatefunc) {
      ntype.updatefunc(&ntree, &bnode);
    }
  }

  void update_internal_links(bNodeTree &ntree, std::unique_ptr<NodeTreeRef> &tree_ref)
  {
    bool any_internal_links_updated = false;
    this->ensure_tree_ref(ntree, tree_ref);
    for (const NodeRef *node : tree_ref->nodes()) {
      if (!this->should_update_individual_node(*node)) {
        continue;
      }
      /* Find all expected internal links. */
      Vector<std::pair<bNodeSocket *, bNodeSocket *>> expected_internal_links;
      for (const OutputSocketRef *output_socket : node->outputs()) {
        if (!output_socket->is_available()) {
          continue;
        }
        if (!output_socket->is_directly_linked()) {
          continue;
        }
        if (output_socket->bsocket()->flag & SOCK_NO_INTERNAL_LINK) {
          continue;
        }
        const InputSocketRef *input_socket = this->find_internally_linked_input(output_socket);
        if (input_socket != nullptr) {
          expected_internal_links.append({input_socket->bsocket(), output_socket->bsocket()});
        }
      }
      /* rebuilt internal links if they have changed. */
      if (node->internal_links().size() != expected_internal_links.size()) {
        this->update_internal_links_in_node(ntree, *node->bnode(), expected_internal_links);
        any_internal_links_updated = true;
      }
      else {
        for (auto &item : expected_internal_links) {
          const bNodeSocket *from_socket = item.first;
          const bNodeSocket *to_socket = item.second;
          bool found = false;
          for (const InternalLinkRef *internal_link : node->internal_links()) {
            if (from_socket == internal_link->from().bsocket() &&
                to_socket == internal_link->to().bsocket()) {
              found = true;
            }
          }
          if (!found) {
            this->update_internal_links_in_node(ntree, *node->bnode(), expected_internal_links);
            any_internal_links_updated = true;
            break;
          }
        }
      }
    }

    if (any_internal_links_updated) {
      tree_ref.reset();
    }
  }

  const InputSocketRef *find_internally_linked_input(const OutputSocketRef *output_socket)
  {
    const InputSocketRef *selected_socket = nullptr;
    int selected_priority = -1;
    bool selected_is_linked = false;
    for (const InputSocketRef *input_socket : output_socket->node().inputs()) {
      if (!input_socket->is_available()) {
        continue;
      }
      if (input_socket->bsocket()->flag & SOCK_NO_INTERNAL_LINK) {
        continue;
      }
      const int priority = get_internal_link_type_priority(input_socket->bsocket()->typeinfo,
                                                           output_socket->bsocket()->typeinfo);
      if (priority < 0) {
        continue;
      }
      const bool is_linked = input_socket->is_directly_linked();
      const bool is_preferred = priority > selected_priority || (is_linked && !selected_is_linked);
      if (!is_preferred) {
        continue;
      }
      selected_socket = input_socket;
      selected_priority = priority;
      selected_is_linked = is_linked;
    }
    return selected_socket;
  }

  void update_internal_links_in_node(bNodeTree &ntree,
                                     bNode &node,
                                     Span<std::pair<bNodeSocket *, bNodeSocket *>> links)
  {
    BLI_freelistN(&node.internal_links);
    for (const auto &item : links) {
      bNodeSocket *from_socket = item.first;
      bNodeSocket *to_socket = item.second;
      bNodeLink *link = MEM_cnew<bNodeLink>(__func__);
      link->fromnode = &node;
      link->fromsock = from_socket;
      link->tonode = &node;
      link->tosock = to_socket;
      link->flag |= NODE_LINK_VALID;
      BLI_addtail(&node.internal_links, link);
    }
    BKE_ntree_update_tag_node_internal_link(&ntree, &node);
  }

  void update_generic_callback(bNodeTree &ntree, std::unique_ptr<NodeTreeRef> &tree_ref)
  {
    if (ntree.typeinfo->update == nullptr) {
      return;
    }

    /* Reset the changed_flag to allow detecting when the update callback changed the node tree. */
    const uint32_t old_changed_flag = ntree.changed_flag;
    ntree.changed_flag = NTREE_CHANGED_NOTHING;

    ntree.typeinfo->update(&ntree);

    if (ntree.changed_flag != NTREE_CHANGED_NOTHING) {
      /* The tree ref is outdated and needs to be rebuilt. */
      tree_ref.reset();
    }
    ntree.changed_flag |= old_changed_flag;
  }

  void remove_unused_previews_when_necessary(bNodeTree &ntree)
  {
    /* Don't trigger preview removal when only those flags are set. */
    const uint32_t allowed_flags = NTREE_CHANGED_LINK | NTREE_CHANGED_SOCKET_PROPERTY |
                                   NTREE_CHANGED_NODE_PROPERTY | NTREE_CHANGED_NODE_OUTPUT |
                                   NTREE_CHANGED_INTERFACE;
    if ((ntree.changed_flag & allowed_flags) == ntree.changed_flag) {
      return;
    }
    BKE_node_preview_remove_unused(&ntree);
  }

  void update_node_levels(bNodeTree &ntree)
  {
    ntreeUpdateNodeLevels(&ntree);
  }

  void update_link_validation(bNodeTree &ntree)
  {
    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      link->flag |= NODE_LINK_VALID;
      if (link->fromnode && link->tonode && link->fromnode->level <= link->tonode->level) {
        link->flag &= ~NODE_LINK_VALID;
      }
      else if (ntree.typeinfo->validate_link) {
        const eNodeSocketDatatype from_type = static_cast<eNodeSocketDatatype>(
            link->fromsock->type);
        const eNodeSocketDatatype to_type = static_cast<eNodeSocketDatatype>(link->tosock->type);
        if (!ntree.typeinfo->validate_link(from_type, to_type)) {
          link->flag &= ~NODE_LINK_VALID;
        }
      }
    }
  }

  bool check_if_output_changed(const NodeTreeRef &tree)
  {
    bNodeTree &btree = *tree.btree();

    /* Compute a hash that represents the node topology connected to the output. This always has to
     * be updated even if it is not used to detect changes right now. Otherwise
     * #btree.output_topology_hash will go out of date. */
    const Vector<const SocketRef *> tree_output_sockets = this->find_output_sockets(tree);
    const uint32_t old_topology_hash = btree.output_topology_hash;
    const uint32_t new_topology_hash = this->get_combined_socket_topology_hash(
        tree, tree_output_sockets);
    btree.output_topology_hash = new_topology_hash;

    if (const AnimData *adt = BKE_animdata_from_id(&btree.id)) {
      /* Drivers may copy values in the node tree around arbitrarily and may cause the output to
       * change even if it wouldn't without drivers. Only some special drivers like `frame/5` can
       * be used without causing updates all the time currently. In the future we could try to
       * handle other drivers better as well.
       * Note that this optimization only works in practice when the depsgraph didn't also get a
       * copy-on-write tag for the node tree (which happens when changing node properties). It does
       * work in a few situations like adding reroutes and duplicating nodes though. */
      LISTBASE_FOREACH (const FCurve *, fcurve, &adt->drivers) {
        const ChannelDriver *driver = fcurve->driver;
        const StringRef expression = driver->expression;
        if (expression.startswith("frame")) {
          const StringRef remaining_expression = expression.drop_known_prefix("frame");
          if (remaining_expression.find_first_not_of(" */+-0123456789.") == StringRef::not_found) {
            continue;
          }
        }
        /* Unrecognized driver, assume that the output always changes. */
        return true;
      }
    }

    if (btree.changed_flag & NTREE_CHANGED_ANY) {
      return true;
    }

    if (old_topology_hash != new_topology_hash) {
      return true;
    }

    /* The topology hash can only be used when only topology-changing operations have been done. */
    if (btree.changed_flag ==
        (btree.changed_flag & (NTREE_CHANGED_LINK | NTREE_CHANGED_REMOVED_NODE))) {
      if (old_topology_hash == new_topology_hash) {
        return false;
      }
    }

    if (!this->check_if_socket_outputs_changed_based_on_flags(tree, tree_output_sockets)) {
      return false;
    }

    return true;
  }

  Vector<const SocketRef *> find_output_sockets(const NodeTreeRef &tree)
  {
    Vector<const SocketRef *> sockets;
    for (const NodeRef *node : tree.nodes()) {
      if (!this->is_output_node(*node)) {
        continue;
      }
      for (const InputSocketRef *socket : node->inputs()) {
        if (socket->idname() != "NodeSocketVirtual") {
          sockets.append(socket);
        }
      }
    }
    return sockets;
  }

  bool is_output_node(const NodeRef &node) const
  {
    const bNode &bnode = *node.bnode();
    if (bnode.typeinfo->nclass == NODE_CLASS_OUTPUT) {
      return true;
    }
    if (bnode.type == NODE_GROUP_OUTPUT) {
      return true;
    }
    /* Assume node groups without output sockets are outputs. */
    /* TODO: Store whether a node group contains a top-level output node (e.g. Material Output) in
     * run-time information on the node group itself. */
    if (bnode.type == NODE_GROUP && node.outputs().is_empty()) {
      return true;
    }
    return false;
  }

  /**
   * Computes a hash that changes when the node tree topology connected to an output node changes.
   * Adding reroutes does not have an effect on the hash.
   */
  uint32_t get_combined_socket_topology_hash(const NodeTreeRef &tree,
                                             Span<const SocketRef *> sockets)
  {
    if (tree.has_link_cycles()) {
      /* Return dummy value when the link has any cycles. The algorithm below could be improved to
       * handle cycles more gracefully. */
      return 0;
    }
    Array<uint32_t> hashes = this->get_socket_topology_hashes(tree, sockets);
    uint32_t combined_hash = 0;
    for (uint32_t hash : hashes) {
      combined_hash = noise::hash(combined_hash, hash);
    }
    return combined_hash;
  }

  Array<uint32_t> get_socket_topology_hashes(const NodeTreeRef &tree,
                                             Span<const SocketRef *> sockets)
  {
    BLI_assert(!tree.has_link_cycles());
    Array<std::optional<uint32_t>> hash_by_socket_id(tree.sockets().size());
    Stack<const SocketRef *> sockets_to_check = sockets;

    while (!sockets_to_check.is_empty()) {
      const SocketRef &in_out_socket = *sockets_to_check.peek();
      const NodeRef &node = in_out_socket.node();

      if (hash_by_socket_id[in_out_socket.id()].has_value()) {
        sockets_to_check.pop();
        /* Socket is handled already. */
        continue;
      }

      if (in_out_socket.is_input()) {
        /* For input sockets, first compute the hashes of all linked sockets. */
        const InputSocketRef &socket = in_out_socket.as_input();
        bool all_origins_computed = true;
        for (const OutputSocketRef *origin_socket : socket.logically_linked_sockets()) {
          if (!hash_by_socket_id[origin_socket->id()].has_value()) {
            sockets_to_check.push(origin_socket);
            all_origins_computed = false;
          }
        }
        if (!all_origins_computed) {
          continue;
        }
        /* When the hashes for the linked sockets are ready, combine them into a hash for the input
         * socket. */
        const uint64_t socket_ptr = (uintptr_t)socket.bsocket();
        uint32_t socket_hash = noise::hash(socket_ptr, socket_ptr >> 32);
        for (const OutputSocketRef *origin_socket : socket.logically_linked_sockets()) {
          const uint32_t origin_socket_hash = *hash_by_socket_id[origin_socket->id()];
          socket_hash = noise::hash(socket_hash, origin_socket_hash);
        }
        hash_by_socket_id[socket.id()] = socket_hash;
        sockets_to_check.pop();
      }
      else {
        /* For output sockets, first compute the hashes of all available input sockets. */
        const OutputSocketRef &socket = in_out_socket.as_output();
        bool all_available_inputs_computed = true;
        for (const InputSocketRef *input_socket : node.inputs()) {
          if (input_socket->is_available()) {
            if (!hash_by_socket_id[input_socket->id()].has_value()) {
              sockets_to_check.push(input_socket);
              all_available_inputs_computed = false;
            }
          }
        }
        if (!all_available_inputs_computed) {
          continue;
        }
        /* When all input socket hashes have been computed, combine them into a hash for the output
         * socket. */
        const uint64_t socket_ptr = (uintptr_t)socket.bsocket();
        uint32_t socket_hash = noise::hash(socket_ptr, socket_ptr >> 32);
        for (const InputSocketRef *input_socket : node.inputs()) {
          if (input_socket->is_available()) {
            const uint32_t input_socket_hash = *hash_by_socket_id[input_socket->id()];
            socket_hash = noise::hash(socket_hash, input_socket_hash);
          }
        }
        hash_by_socket_id[socket.id()] = socket_hash;
        sockets_to_check.pop();
      }
    }

    /* Create output array. */
    Array<uint32_t> hashes(sockets.size());
    for (const int i : sockets.index_range()) {
      hashes[i] = *hash_by_socket_id[sockets[i]->id()];
    }
    return hashes;
  }

  /**
   * Returns true when any of the provided sockets changed its values. A change is detected by
   * checking the #changed_flag on connected sockets and nodes.
   */
  bool check_if_socket_outputs_changed_based_on_flags(const NodeTreeRef &tree,
                                                      Span<const SocketRef *> sockets)
  {
    /* Avoid visiting the same socket twice when multiple links point to the same socket. */
    Array<bool> pushed_by_socket_id(tree.sockets().size(), false);
    Stack<const SocketRef *> sockets_to_check = sockets;

    for (const SocketRef *socket : sockets) {
      pushed_by_socket_id[socket->id()] = true;
    }

    while (!sockets_to_check.is_empty()) {
      const SocketRef &in_out_socket = *sockets_to_check.pop();
      const NodeRef &node = in_out_socket.node();
      const bNode &bnode = *node.bnode();
      const bNodeSocket &bsocket = *in_out_socket.bsocket();
      if (bsocket.changed_flag != NTREE_CHANGED_NOTHING) {
        return true;
      }
      if (bnode.changed_flag != NTREE_CHANGED_NOTHING) {
        const bool only_unused_internal_link_changed = (bnode.flag & NODE_MUTED) == 0 &&
                                                       bnode.changed_flag ==
                                                           NTREE_CHANGED_INTERNAL_LINK;
        if (!only_unused_internal_link_changed) {
          return true;
        }
      }
      if (in_out_socket.is_input()) {
        const InputSocketRef &socket = in_out_socket.as_input();
        for (const OutputSocketRef *origin_socket : socket.logically_linked_sockets()) {
          bool &pushed = pushed_by_socket_id[origin_socket->id()];
          if (!pushed) {
            sockets_to_check.push(origin_socket);
            pushed = true;
          }
        }
      }
      else {
        const OutputSocketRef &socket = in_out_socket.as_output();
        for (const InputSocketRef *input_socket : node.inputs()) {
          if (input_socket->is_available()) {
            bool &pushed = pushed_by_socket_id[input_socket->id()];
            if (!pushed) {
              sockets_to_check.push(input_socket);
              pushed = true;
            }
          }
        }
        /* The Normal node has a special case, because the value stored in the first output socket
         * is used as input in the node. */
        if (bnode.type == SH_NODE_NORMAL && socket.index() == 1) {
          BLI_assert(socket.name() == "Dot");
          const OutputSocketRef &normal_output = node.output(0);
          BLI_assert(normal_output.name() == "Normal");
          bool &pushed = pushed_by_socket_id[normal_output.id()];
          if (!pushed) {
            sockets_to_check.push(&normal_output);
            pushed = true;
          }
        }
      }
    }
    return false;
  }

  void reset_changed_flags(bNodeTree &ntree)
  {
    ntree.changed_flag = NTREE_CHANGED_NOTHING;
    LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
      node->changed_flag = NTREE_CHANGED_NOTHING;
      node->update = 0;
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        socket->changed_flag = NTREE_CHANGED_NOTHING;
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
        socket->changed_flag = NTREE_CHANGED_NOTHING;
      }
    }
  }
};

}  // namespace blender::bke

void BKE_ntree_update_tag_all(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_ANY);
}

void BKE_ntree_update_tag_node_property(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_node_new(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_socket_property(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_socket_new(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_socket_removed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_REMOVED_SOCKET);
}

void BKE_ntree_update_tag_socket_type(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_socket_availability(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_node_removed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_REMOVED_NODE);
}

void BKE_ntree_update_tag_node_mute(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_node_internal_link(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_INTERNAL_LINK);
}

void BKE_ntree_update_tag_link_changed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_link_removed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_link_added(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_link_mute(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_active_output_changed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_ANY);
}

void BKE_ntree_update_tag_missing_runtime_data(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_ALL);
}

void BKE_ntree_update_tag_interface(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_INTERFACE);
}

void BKE_ntree_update_tag_id_changed(Main *bmain, ID *id)
{
  FOREACH_NODETREE_BEGIN (bmain, ntree, ntree_id) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->id == id) {
        node->update |= NODE_UPDATE_ID;
        add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
      }
    }
  }
  FOREACH_NODETREE_END;
}

void BKE_ntree_update_tag_image_user_changed(bNodeTree *ntree, ImageUser *UNUSED(iuser))
{
  /* Would have to search for the node that uses the image user for a more detailed tag. */
  add_tree_tag(ntree, NTREE_CHANGED_ANY);
}

/**
 * Protect from recursive calls into the updating function. Some node update functions might
 * trigger this from Python or in other cases.
 *
 * This could be added to #Main, but given that there is generally only one #Main, that's not
 * really worth it now.
 */
static bool is_updating = false;

void BKE_ntree_update_main(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update();
  is_updating = false;
}

void BKE_ntree_update_main_tree(Main *bmain, bNodeTree *ntree, NodeTreeUpdateExtraParams *params)
{
  if (ntree == nullptr) {
    BKE_ntree_update_main(bmain, params);
    return;
  }

  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update_rooted({ntree});
  is_updating = false;
}
