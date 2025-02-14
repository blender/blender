/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <queue>

#include "NOD_partial_eval.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

namespace blender::nodes::partial_eval {

bool is_supported_value_node(const bNode &node)
{
  return ELEM(node.type_legacy,
              SH_NODE_VALUE,
              FN_NODE_INPUT_VECTOR,
              FN_NODE_INPUT_BOOL,
              FN_NODE_INPUT_INT,
              FN_NODE_INPUT_ROTATION);
}

/**
 * Creates a vector of integer for a node in a context that can be used to order them for
 * evaluation.
 */
static Vector<int> get_global_node_sort_vector_right_to_left(const ComputeContext *initial_context,
                                                             const bNode &initial_node)
{
  Vector<int> vec;
  vec.append(initial_node.runtime->toposort_right_to_left_index);
  for (const ComputeContext *context = initial_context; context; context = context->parent()) {
    if (const auto *group_context = dynamic_cast<const bke::GroupNodeComputeContext *>(context)) {
      const bNode *caller_group_node = group_context->caller_group_node();
      BLI_assert(caller_group_node != nullptr);
      vec.append(caller_group_node->runtime->toposort_right_to_left_index);
    }
  }
  std::reverse(vec.begin(), vec.end());
  return vec;
}

/** Same as above but for the case when evaluating nodes in the opposite order. */
static Vector<int> get_global_node_sort_vector_left_to_right(const ComputeContext *initial_context,
                                                             const bNode &initial_node)
{
  Vector<int> vec;
  vec.append(initial_node.runtime->toposort_left_to_right_index);
  for (const ComputeContext *context = initial_context; context; context = context->parent()) {
    if (const auto *group_context = dynamic_cast<const bke::GroupNodeComputeContext *>(context)) {
      const bNode *caller_group_node = group_context->caller_group_node();
      BLI_assert(caller_group_node != nullptr);
      vec.append(caller_group_node->runtime->toposort_left_to_right_index);
    }
  }
  std::reverse(vec.begin(), vec.end());
  return vec;
}

/**
 * Defines a partial order of #NodeInContext that can be used to evaluate nodes right to left
 * (upstream).
 * - Downstream nodes are sorted before upstream nodes.
 * - Nodes inside a node group are sorted before the group node.
 */
struct NodeInContextUpstreamComparator {
  bool operator()(const NodeInContext &a, const NodeInContext &b) const
  {
    const Vector<int> a_sort_vec = get_global_node_sort_vector_right_to_left(a.context, *a.node);
    const Vector<int> b_sort_vec = get_global_node_sort_vector_right_to_left(b.context, *b.node);
    const int common_length = std::min(a_sort_vec.size(), b_sort_vec.size());
    const Span<int> a_common = Span<int>(a_sort_vec).take_front(common_length);
    const Span<int> b_common = Span<int>(b_sort_vec).take_front(common_length);
    if (a_common == b_common) {
      return a_sort_vec.size() < b_sort_vec.size();
    }
    return std::lexicographical_compare(
        b_common.begin(), b_common.end(), a_common.begin(), a_common.end());
  }
};

/**
 * Defines a partial order of #NodeInContext that can be used to evaluate nodes left to right
 * (downstream).
 * - Upstream nodes are sorted before downstream nodes.
 * - Nodes inside a node group are sorted before the group node.
 */
struct NodeInContextDownstreamComparator {
  bool operator()(const NodeInContext &a, const NodeInContext &b) const
  {
    const Vector<int> a_sort_vec = get_global_node_sort_vector_left_to_right(a.context, *a.node);
    const Vector<int> b_sort_vec = get_global_node_sort_vector_left_to_right(b.context, *b.node);
    const int common_length = std::min(a_sort_vec.size(), b_sort_vec.size());
    const Span<int> a_common = Span<int>(a_sort_vec).take_front(common_length);
    const Span<int> b_common = Span<int>(b_sort_vec).take_front(common_length);
    if (a_common == b_common) {
      return a_sort_vec.size() < b_sort_vec.size();
    }
    return std::lexicographical_compare(
        b_common.begin(), b_common.end(), a_common.begin(), a_common.end());
  }
};

void eval_downstream(
    const Span<SocketInContext> initial_sockets,
    ResourceScope &scope,
    FunctionRef<void(const NodeInContext &ctx_node,
                     Vector<const bNodeSocket *> &r_outputs_to_propagate)> evaluate_node_fn,
    FunctionRef<bool(const SocketInContext &ctx_from, const SocketInContext &ctx_to)>
        propagate_value_fn)
{
  /* Priority queue that makes sure that nodes are evaluated in the right order. */
  std::priority_queue<NodeInContext, std::vector<NodeInContext>, NodeInContextDownstreamComparator>
      scheduled_nodes_queue;
  /* Used to make sure that the same node is not scheduled more than once. */
  Set<NodeInContext> scheduled_nodes_set;

  const auto schedule_node = [&](const NodeInContext &ctx_node) {
    if (scheduled_nodes_set.add(ctx_node)) {
      scheduled_nodes_queue.push(ctx_node);
    }
  };

  const auto forward_group_node_input_into_group =
      [&](const SocketInContext &ctx_group_node_input) {
        const bNode &node = ctx_group_node_input.socket->owner_node();
        BLI_assert(node.is_group());
        const bNodeTree *group_tree = reinterpret_cast<const bNodeTree *>(node.id);
        if (!group_tree) {
          return;
        }
        group_tree->ensure_topology_cache();
        if (group_tree->has_available_link_cycle()) {
          return;
        }
        const auto &group_context = scope.construct<bke::GroupNodeComputeContext>(
            ctx_group_node_input.context, node, node.owner_tree());
        const int socket_index = ctx_group_node_input.socket->index();
        /* Forward the value to every group input node. */
        for (const bNode *group_input_node : group_tree->group_input_nodes()) {
          if (propagate_value_fn(ctx_group_node_input,
                                 {&group_context, &group_input_node->output_socket(socket_index)}))
          {
            schedule_node({&group_context, group_input_node});
          }
        }
      };

  const auto forward_output = [&](const SocketInContext &ctx_output_socket) {
    const ComputeContext *context = ctx_output_socket.context;
    for (const bNodeLink *link : ctx_output_socket.socket->directly_linked_links()) {
      if (!link->is_used()) {
        continue;
      }
      const bNode &target_node = *link->tonode;
      const bNodeSocket &target_socket = *link->tosock;
      if (!propagate_value_fn(ctx_output_socket, {context, &target_socket})) {
        continue;
      }
      schedule_node({context, &target_node});
      if (target_node.is_group()) {
        forward_group_node_input_into_group({context, &target_socket});
      }
    }
  };

  /* Do initial scheduling based on initial sockets. */
  for (const SocketInContext &ctx_socket : initial_sockets) {
    if (ctx_socket.socket->is_input()) {
      const bNode &node = ctx_socket.socket->owner_node();
      if (node.is_group()) {
        forward_group_node_input_into_group(ctx_socket);
      }
      schedule_node({ctx_socket.context, &node});
    }
    else {
      forward_output(ctx_socket);
    }
  }

  /* Reused in multiple places to avoid allocating it multiple times. Should be cleared before
   * using it. */
  Vector<const bNodeSocket *> sockets_vec;

  /* Handle all scheduled nodes in the right order until no more nodes are scheduled. */
  while (!scheduled_nodes_queue.empty()) {
    const NodeInContext ctx_node = scheduled_nodes_queue.top();
    scheduled_nodes_queue.pop();

    const bNode &node = *ctx_node.node;
    const ComputeContext *context = ctx_node.context;

    if (node.is_reroute()) {
      if (propagate_value_fn({context, &node.input_socket(0)}, {context, &node.output_socket(0)}))
      {
        forward_output({context, &node.output_socket(0)});
      }
    }
    else if (node.is_muted()) {
      for (const bNodeLink &link : node.internal_links()) {
        if (propagate_value_fn({context, link.fromsock}, {context, link.tosock})) {
          forward_output({context, link.tosock});
        }
      }
    }
    else if (node.is_group()) {
      const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id);
      if (!group) {
        continue;
      }
      group->ensure_topology_cache();
      if (group->has_available_link_cycle()) {
        continue;
      }
      const bNode *group_output = group->group_output_node();
      if (!group_output) {
        continue;
      }
      const ComputeContext &group_context = scope.construct<bke::GroupNodeComputeContext>(
          context, node, node.owner_tree());
      /* Propagate the values from the group output node to the outputs of the group node and
       * continue forwarding them from there. */
      for (const int index : group->interface_outputs().index_range()) {
        if (propagate_value_fn({&group_context, &group_output->input_socket(index)},
                               {context, &node.output_socket(index)}))
        {
          forward_output({context, &node.output_socket(index)});
        }
      }
    }
    else if (node.is_group_input()) {
      for (const bNodeSocket *output_socket : node.output_sockets()) {
        forward_output({context, output_socket});
      }
    }
    else {
      sockets_vec.clear();
      evaluate_node_fn(ctx_node, sockets_vec);
      for (const bNodeSocket *socket : sockets_vec) {
        forward_output({context, socket});
      }
    }
  }
}

UpstreamEvalTargets eval_upstream(
    const Span<SocketInContext> initial_sockets,
    ResourceScope &scope,
    FunctionRef<void(const NodeInContext &ctx_node,
                     Vector<const bNodeSocket *> &r_modified_inputs)> evaluate_node_fn,
    FunctionRef<bool(const SocketInContext &ctx_from, const SocketInContext &ctx_to)>
        propagate_value_fn,
    FunctionRef<void(const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_sockets)>
        get_inputs_to_propagate_fn)
{
  /* Priority queue that makes sure that nodes are evaluated in the right order. */
  std::priority_queue<NodeInContext, std::vector<NodeInContext>, NodeInContextUpstreamComparator>
      scheduled_nodes_queue;
  /* Used to make sure that the same node is not scheduled more than once. */
  Set<NodeInContext> scheduled_nodes_set;

  UpstreamEvalTargets eval_targets;

  const auto schedule_node = [&](const NodeInContext &ctx_node) {
    if (scheduled_nodes_set.add(ctx_node)) {
      scheduled_nodes_queue.push(ctx_node);
    }
  };

  const auto forward_group_node_output_into_group = [&](const SocketInContext &ctx_output_socket) {
    const ComputeContext *context = ctx_output_socket.context;
    const bNode &group_node = ctx_output_socket.socket->owner_node();
    const bNodeTree *group = reinterpret_cast<const bNodeTree *>(group_node.id);
    if (!group) {
      return;
    }
    group->ensure_topology_cache();
    if (group->has_available_link_cycle()) {
      return;
    }
    const bNode *group_output = group->group_output_node();
    if (!group_output) {
      return;
    }
    const ComputeContext &group_context = scope.construct<bke::GroupNodeComputeContext>(
        context, group_node, group_node.owner_tree());
    propagate_value_fn(
        ctx_output_socket,
        {&group_context, &group_output->input_socket(ctx_output_socket.socket->index())});
    schedule_node({&group_context, group_output});
  };

  const auto forward_group_input_to_parent = [&](const SocketInContext &ctx_output_socket) {
    const auto *group_context = dynamic_cast<const bke::GroupNodeComputeContext *>(
        ctx_output_socket.context);
    if (!group_context) {
      eval_targets.group_inputs.add(ctx_output_socket);
      return;
    }
    const bNodeTree &caller_tree = *group_context->caller_tree();
    caller_tree.ensure_topology_cache();
    if (caller_tree.has_available_link_cycle()) {
      return;
    }
    const bNode &caller_node = *group_context->caller_group_node();
    const bNodeSocket &caller_input_socket = caller_node.input_socket(
        ctx_output_socket.socket->index());
    const ComputeContext *parent_context = ctx_output_socket.context->parent();
    /* Note that we might propagate multiple values to the same input of the group node. The
     * callback has to handle that case gracefully. */
    propagate_value_fn(ctx_output_socket, {parent_context, &caller_input_socket});
    schedule_node({parent_context, &caller_node});
  };

  const auto forward_input = [&](const SocketInContext &ctx_input_socket) {
    const ComputeContext *context = ctx_input_socket.context;
    if (!ctx_input_socket.socket->is_logically_linked()) {
      eval_targets.sockets.add(ctx_input_socket);
      return;
    }
    for (const bNodeLink *link : ctx_input_socket.socket->directly_linked_links()) {
      if (!link->is_used()) {
        continue;
      }
      const bNode &origin_node = *link->fromnode;
      const bNodeSocket &origin_socket = *link->fromsock;
      if (!propagate_value_fn(ctx_input_socket, {context, &origin_socket})) {
        continue;
      }
      schedule_node({context, &origin_node});
      if (origin_node.is_group()) {
        forward_group_node_output_into_group({context, &origin_socket});
        continue;
      }
      if (origin_node.is_group_input()) {
        forward_group_input_to_parent({context, &origin_socket});
        continue;
      }
    }
  };

  /* Do initial scheduling based on initial sockets. */
  for (const SocketInContext &ctx_socket : initial_sockets) {
    if (ctx_socket.socket->is_input()) {
      forward_input(ctx_socket);
    }
    else {
      const bNode &node = ctx_socket.socket->owner_node();
      if (node.is_group()) {
        forward_group_node_output_into_group(ctx_socket);
      }
      else if (node.is_group_input()) {
        forward_group_input_to_parent(ctx_socket);
      }
      else {
        schedule_node({ctx_socket.context, &node});
      }
    }
  }

  /* Reused in multiple places to avoid allocating it multiple times. Should be cleared before
   * using it. */
  Vector<const bNodeSocket *> sockets_vec;

  /* Handle all nodes in the right order until there are no more nodes to evaluate. */
  while (!scheduled_nodes_queue.empty()) {
    const NodeInContext ctx_node = scheduled_nodes_queue.top();
    scheduled_nodes_queue.pop();

    const bNode &node = *ctx_node.node;
    const ComputeContext *context = ctx_node.context;

    if (is_supported_value_node(node)) {
      /* Can't go back further from here, but remember that we reached a value node. */
      eval_targets.value_nodes.add(ctx_node);
    }
    else if (node.is_reroute()) {
      propagate_value_fn({context, &node.output_socket(0)}, {context, &node.input_socket(0)});
      forward_input({context, &node.input_socket(0)});
    }
    else if (node.is_muted()) {
      for (const bNodeLink &link : node.internal_links()) {
        if (propagate_value_fn({context, link.tosock}, {context, link.fromsock})) {
          forward_input({context, link.fromsock});
        }
      }
    }
    else if (node.is_group()) {
      /* Once we get here, the nodes within the group have all been evaluated already and the
       * inputs of the group node are already set properly by #forward_group_input_to_parent. */
      sockets_vec.clear();
      get_inputs_to_propagate_fn(ctx_node, sockets_vec);
      for (const bNodeSocket *socket : sockets_vec) {
        forward_input({context, socket});
      }
    }
    else if (node.is_group_output()) {
      sockets_vec.clear();
      get_inputs_to_propagate_fn(ctx_node, sockets_vec);
      for (const bNodeSocket *socket : sockets_vec) {
        forward_input({context, socket});
      }
    }
    else {
      sockets_vec.clear();
      evaluate_node_fn(ctx_node, sockets_vec);
      for (const bNodeSocket *input_socket : sockets_vec) {
        forward_input({context, input_socket});
      }
    }
  }

  return eval_targets;
}

}  // namespace blender::nodes::partial_eval
