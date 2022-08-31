/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.h"
#include "BKE_node_runtime.hh"

#include "DNA_node_types.h"

#include "BLI_function_ref.hh"
#include "BLI_stack.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

namespace blender::bke::node_tree_runtime {

static void double_checked_lock(std::mutex &mutex, bool &data_is_dirty, FunctionRef<void()> fn)
{
  if (!data_is_dirty) {
    return;
  }
  std::lock_guard lock{mutex};
  if (!data_is_dirty) {
    return;
  }
  fn();
  data_is_dirty = false;
}

static void double_checked_lock_with_task_isolation(std::mutex &mutex,
                                                    bool &data_is_dirty,
                                                    FunctionRef<void()> fn)
{
  double_checked_lock(mutex, data_is_dirty, [&]() { threading::isolate_task(fn); });
}

static void update_node_vector(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.nodes.clear();
  tree_runtime.has_undefined_nodes_or_sockets = false;
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    node->runtime->index_in_tree = tree_runtime.nodes.append_and_get_index(node);
    node->runtime->owner_tree = const_cast<bNodeTree *>(&ntree);
    tree_runtime.has_undefined_nodes_or_sockets |= node->typeinfo == &NodeTypeUndefined;
  }
}

static void update_link_vector(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.links.clear();
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    tree_runtime.links.append(link);
  }
}

static void update_internal_links(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  for (bNode *node : tree_runtime.nodes) {
    node->runtime->internal_links.clear();
    for (bNodeSocket *socket : node->runtime->outputs) {
      socket->runtime->internal_link_input = nullptr;
    }
    LISTBASE_FOREACH (bNodeLink *, link, &node->internal_links) {
      node->runtime->internal_links.append(link);
      link->tosock->runtime->internal_link_input = link->fromsock;
    }
  }
}

static void update_socket_vectors_and_owner_node(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.sockets.clear();
  tree_runtime.input_sockets.clear();
  tree_runtime.output_sockets.clear();
  for (bNode *node : tree_runtime.nodes) {
    bNodeRuntime &node_runtime = *node->runtime;
    node_runtime.inputs.clear();
    node_runtime.outputs.clear();
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      socket->runtime->index_in_node = node_runtime.inputs.append_and_get_index(socket);
      socket->runtime->index_in_all_sockets = tree_runtime.sockets.append_and_get_index(socket);
      socket->runtime->index_in_inout_sockets = tree_runtime.input_sockets.append_and_get_index(
          socket);
      socket->runtime->owner_node = node;
      tree_runtime.has_undefined_nodes_or_sockets |= socket->typeinfo == &NodeSocketTypeUndefined;
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      socket->runtime->index_in_node = node_runtime.outputs.append_and_get_index(socket);
      socket->runtime->index_in_all_sockets = tree_runtime.sockets.append_and_get_index(socket);
      socket->runtime->index_in_inout_sockets = tree_runtime.output_sockets.append_and_get_index(
          socket);
      socket->runtime->owner_node = node;
      tree_runtime.has_undefined_nodes_or_sockets |= socket->typeinfo == &NodeSocketTypeUndefined;
    }
  }
}

static void update_directly_linked_links_and_sockets(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  for (bNode *node : tree_runtime.nodes) {
    for (bNodeSocket *socket : node->runtime->inputs) {
      socket->runtime->directly_linked_links.clear();
      socket->runtime->directly_linked_sockets.clear();
    }
    for (bNodeSocket *socket : node->runtime->outputs) {
      socket->runtime->directly_linked_links.clear();
      socket->runtime->directly_linked_sockets.clear();
    }
    node->runtime->has_linked_inputs = false;
    node->runtime->has_linked_outputs = false;
  }
  for (bNodeLink *link : tree_runtime.links) {
    link->fromsock->runtime->directly_linked_links.append(link);
    link->fromsock->runtime->directly_linked_sockets.append(link->tosock);
    link->tosock->runtime->directly_linked_links.append(link);
    link->fromnode->runtime->has_linked_outputs = true;
    link->tonode->runtime->has_linked_inputs = true;
  }
  for (bNodeSocket *socket : tree_runtime.input_sockets) {
    if (socket->flag & SOCK_MULTI_INPUT) {
      std::sort(socket->runtime->directly_linked_links.begin(),
                socket->runtime->directly_linked_links.end(),
                [&](const bNodeLink *a, const bNodeLink *b) {
                  return a->multi_input_socket_index > b->multi_input_socket_index;
                });
    }
  }
  for (bNodeSocket *socket : tree_runtime.input_sockets) {
    for (bNodeLink *link : socket->runtime->directly_linked_links) {
      /* Do this after sorting the input links. */
      socket->runtime->directly_linked_sockets.append(link->fromsock);
    }
  }
}

static void find_logical_origins_for_socket_recursive(
    bNodeSocket &input_socket,
    bool only_follow_first_input_link,
    Vector<bNodeSocket *, 16> &sockets_in_current_chain,
    Vector<bNodeSocket *> &r_logical_origins,
    Vector<bNodeSocket *> &r_skipped_origins)
{
  if (sockets_in_current_chain.contains(&input_socket)) {
    /* Protect against reroute recursions. */
    return;
  }
  sockets_in_current_chain.append(&input_socket);

  Span<bNodeLink *> links_to_check = input_socket.runtime->directly_linked_links;
  if (only_follow_first_input_link) {
    links_to_check = links_to_check.take_front(1);
  }
  for (bNodeLink *link : links_to_check) {
    if (link->flag & NODE_LINK_MUTED) {
      continue;
    }
    bNodeSocket &origin_socket = *link->fromsock;
    bNode &origin_node = *link->fromnode;
    if (!origin_socket.is_available()) {
      /* Non available sockets are ignored. */
      continue;
    }
    if (origin_node.type == NODE_REROUTE) {
      bNodeSocket &reroute_input = *origin_node.runtime->inputs[0];
      bNodeSocket &reroute_output = *origin_node.runtime->outputs[0];
      r_skipped_origins.append(&reroute_input);
      r_skipped_origins.append(&reroute_output);
      find_logical_origins_for_socket_recursive(
          reroute_input, false, sockets_in_current_chain, r_logical_origins, r_skipped_origins);
      continue;
    }
    if (origin_node.is_muted()) {
      if (bNodeSocket *mute_input = origin_socket.runtime->internal_link_input) {
        r_skipped_origins.append(&origin_socket);
        r_skipped_origins.append(mute_input);
        find_logical_origins_for_socket_recursive(
            *mute_input, true, sockets_in_current_chain, r_logical_origins, r_skipped_origins);
      }
      continue;
    }
    r_logical_origins.append(&origin_socket);
  }

  sockets_in_current_chain.pop_last();
}

static void update_logical_origins(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  threading::parallel_for(tree_runtime.nodes.index_range(), 128, [&](const IndexRange range) {
    for (const int i : range) {
      bNode &node = *tree_runtime.nodes[i];
      for (bNodeSocket *socket : node.runtime->inputs) {
        Vector<bNodeSocket *, 16> sockets_in_current_chain;
        socket->runtime->logically_linked_sockets.clear();
        socket->runtime->logically_linked_skipped_sockets.clear();
        find_logical_origins_for_socket_recursive(
            *socket,
            false,
            sockets_in_current_chain,
            socket->runtime->logically_linked_sockets,
            socket->runtime->logically_linked_skipped_sockets);
      }
    }
  });
}

static void update_nodes_by_type(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.nodes_by_type.clear();
  for (bNode *node : tree_runtime.nodes) {
    tree_runtime.nodes_by_type.add(node->typeinfo, node);
  }
}

static void update_sockets_by_identifier(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  threading::parallel_for(tree_runtime.nodes.index_range(), 128, [&](const IndexRange range) {
    for (bNode *node : tree_runtime.nodes.as_span().slice(range)) {
      node->runtime->inputs_by_identifier.clear();
      node->runtime->outputs_by_identifier.clear();
      for (bNodeSocket *socket : node->runtime->inputs) {
        node->runtime->inputs_by_identifier.add_new(socket->identifier, socket);
      }
      for (bNodeSocket *socket : node->runtime->outputs) {
        node->runtime->outputs_by_identifier.add_new(socket->identifier, socket);
      }
    }
  });
}

enum class ToposortDirection {
  LeftToRight,
  RightToLeft,
};

struct ToposortNodeState {
  bool is_done = false;
  bool is_in_stack = false;
};

static void toposort_from_start_node(const ToposortDirection direction,
                                     bNode &start_node,
                                     MutableSpan<ToposortNodeState> node_states,
                                     Vector<bNode *> &r_sorted_nodes,
                                     bool &r_cycle_detected)
{
  struct Item {
    bNode *node;
    int socket_index = 0;
    int link_index = 0;
  };

  Stack<Item, 64> nodes_to_check;
  nodes_to_check.push({&start_node});
  while (!nodes_to_check.is_empty()) {
    Item &item = nodes_to_check.peek();
    bNode &node = *item.node;
    const Span<bNodeSocket *> sockets = (direction == ToposortDirection::LeftToRight) ?
                                            node.runtime->inputs :
                                            node.runtime->outputs;
    while (true) {
      if (item.socket_index == sockets.size()) {
        /* All sockets have already been visited. */
        break;
      }
      bNodeSocket &socket = *sockets[item.socket_index];
      const Span<bNodeSocket *> linked_sockets = socket.runtime->directly_linked_sockets;
      if (item.link_index == linked_sockets.size()) {
        /* All links connected to this socket have already been visited. */
        item.socket_index++;
        item.link_index = 0;
        continue;
      }
      bNodeSocket &linked_socket = *linked_sockets[item.link_index];
      bNode &linked_node = *linked_socket.runtime->owner_node;
      ToposortNodeState &linked_node_state = node_states[linked_node.runtime->index_in_tree];
      if (linked_node_state.is_done) {
        /* The linked node has already been visited. */
        item.link_index++;
        continue;
      }
      if (linked_node_state.is_in_stack) {
        r_cycle_detected = true;
      }
      else {
        nodes_to_check.push({&linked_node});
        linked_node_state.is_in_stack = true;
      }
      break;
    }

    /* If no other element has been pushed, the current node can be pushed to the sorted list. */
    if (&item == &nodes_to_check.peek()) {
      ToposortNodeState &node_state = node_states[node.runtime->index_in_tree];
      node_state.is_done = true;
      node_state.is_in_stack = false;
      r_sorted_nodes.append(&node);
      nodes_to_check.pop();
    }
  }
}

static void update_toposort(const bNodeTree &ntree,
                            const ToposortDirection direction,
                            Vector<bNode *> &r_sorted_nodes,
                            bool &r_cycle_detected)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  r_sorted_nodes.clear();
  r_sorted_nodes.reserve(tree_runtime.nodes.size());
  r_cycle_detected = false;

  Array<ToposortNodeState> node_states(tree_runtime.nodes.size());
  for (bNode *node : tree_runtime.nodes) {
    if (node_states[node->runtime->index_in_tree].is_done) {
      /* Ignore nodes that are done already. */
      continue;
    }
    if ((direction == ToposortDirection::LeftToRight) ? node->runtime->has_linked_outputs :
                                                        node->runtime->has_linked_inputs) {
      /* Ignore non-start nodes. */
      continue;
    }
    toposort_from_start_node(direction, *node, node_states, r_sorted_nodes, r_cycle_detected);
  }

  if (r_sorted_nodes.size() < tree_runtime.nodes.size()) {
    r_cycle_detected = true;
    for (bNode *node : tree_runtime.nodes) {
      if (node_states[node->runtime->index_in_tree].is_done) {
        /* Ignore nodes that are done already. */
        continue;
      }
      /* Start toposort at this node which is somewhere in the middle of a loop. */
      toposort_from_start_node(direction, *node, node_states, r_sorted_nodes, r_cycle_detected);
    }
  }

  BLI_assert(tree_runtime.nodes.size() == r_sorted_nodes.size());
}

static void update_group_output_node(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  const bNodeType *node_type = nodeTypeFind("NodeGroupOutput");
  const Span<bNode *> group_output_nodes = tree_runtime.nodes_by_type.lookup(node_type);
  if (group_output_nodes.is_empty()) {
    tree_runtime.group_output_node = nullptr;
  }
  else if (group_output_nodes.size() == 1) {
    tree_runtime.group_output_node = group_output_nodes[0];
  }
  else {
    for (bNode *group_output : group_output_nodes) {
      if (group_output->flag & NODE_DO_OUTPUT) {
        tree_runtime.group_output_node = group_output;
        break;
      }
    }
  }
}

static void ensure_topology_cache(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  double_checked_lock_with_task_isolation(
      tree_runtime.topology_cache_mutex, tree_runtime.topology_cache_is_dirty, [&]() {
        update_node_vector(ntree);
        update_link_vector(ntree);
        update_socket_vectors_and_owner_node(ntree);
        update_internal_links(ntree);
        update_directly_linked_links_and_sockets(ntree);
        threading::parallel_invoke([&]() { update_logical_origins(ntree); },
                                   [&]() { update_nodes_by_type(ntree); },
                                   [&]() { update_sockets_by_identifier(ntree); },
                                   [&]() {
                                     update_toposort(ntree,
                                                     ToposortDirection::LeftToRight,
                                                     tree_runtime.toposort_left_to_right,
                                                     tree_runtime.has_link_cycle);
                                   },
                                   [&]() {
                                     bool dummy;
                                     update_toposort(ntree,
                                                     ToposortDirection::RightToLeft,
                                                     tree_runtime.toposort_right_to_left,
                                                     dummy);
                                   });
        update_group_output_node(ntree);
        tree_runtime.topology_cache_exists = true;
      });
}

}  // namespace blender::bke::node_tree_runtime

void bNodeTree::ensure_topology_cache() const
{
  blender::bke::node_tree_runtime::ensure_topology_cache(*this);
}
