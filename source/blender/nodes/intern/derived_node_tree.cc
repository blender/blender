/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_derived_node_tree.hh"

#include "BLI_dot_export.hh"

namespace blender::nodes {

DerivedNodeTree::DerivedNodeTree(const bNodeTree &btree)
{
  /* Construct all possible contexts immediately. This is significantly cheaper than inlining all
   * node groups. If it still becomes a performance issue in the future, contexts could be
   * constructed lazily when they are needed. */
  root_context_ = &this->construct_context_recursively(
      nullptr, nullptr, btree, bke::NODE_INSTANCE_KEY_BASE);
}

DTreeContext &DerivedNodeTree::construct_context_recursively(DTreeContext *parent_context,
                                                             const bNode *parent_node,
                                                             const bNodeTree &btree,
                                                             const bNodeInstanceKey instance_key)
{
  btree.ensure_topology_cache();
  DTreeContext &context = *allocator_.construct<DTreeContext>().release();
  context.parent_context_ = parent_context;
  context.parent_node_ = parent_node;
  context.derived_tree_ = this;
  context.btree_ = &btree;
  context.instance_key_ = instance_key;
  used_btrees_.add(context.btree_);

  for (const bNode *bnode : context.btree_->all_nodes()) {
    if (bnode->is_group()) {
      bNodeTree *child_btree = reinterpret_cast<bNodeTree *>(bnode->id);
      if (child_btree != nullptr) {
        const bNodeInstanceKey child_key = bke::node_instance_key(instance_key, &btree, bnode);
        DTreeContext &child = this->construct_context_recursively(
            &context, bnode, *child_btree, child_key);
        context.children_.add_new(bnode, &child);
      }
    }
  }

  return context;
}

DerivedNodeTree::~DerivedNodeTree()
{
  /* Has to be destructed manually, because the context info is allocated in a linear allocator. */
  this->destruct_context_recursively(root_context_);
}

void DerivedNodeTree::destruct_context_recursively(DTreeContext *context)
{
  for (DTreeContext *child : context->children_.values()) {
    this->destruct_context_recursively(child);
  }
  context->~DTreeContext();
}

bool DerivedNodeTree::has_link_cycles() const
{
  for (const bNodeTree *btree : used_btrees_) {
    if (btree->has_available_link_cycle()) {
      return true;
    }
  }
  return false;
}

bool DerivedNodeTree::has_undefined_nodes_or_sockets() const
{
  for (const bNodeTree *btree : used_btrees_) {
    if (btree->has_undefined_nodes_or_sockets()) {
      return true;
    }
  }
  return false;
}

void DerivedNodeTree::foreach_node(FunctionRef<void(DNode)> callback) const
{
  this->foreach_node_in_context_recursive(*root_context_, callback);
}

void DerivedNodeTree::foreach_node_in_context_recursive(const DTreeContext &context,
                                                        FunctionRef<void(DNode)> callback) const
{
  for (const bNode *bnode : context.btree_->all_nodes()) {
    callback(DNode(&context, bnode));
  }
  for (const DTreeContext *child_context : context.children_.values()) {
    this->foreach_node_in_context_recursive(*child_context, callback);
  }
}

bNodeInstanceKey DNode::instance_key() const
{
  return bke::node_instance_key(context()->instance_key(), &context()->btree(), bnode());
}

DOutputSocket DInputSocket::get_corresponding_group_node_output() const
{
  BLI_assert(*this);
  BLI_assert(bsocket_->owner_node().is_group_output());
  BLI_assert(bsocket_->index() < bsocket_->owner_node().input_sockets().size() - 1);

  const DTreeContext *parent_context = context_->parent_context();
  const bNode *parent_node = context_->parent_node();
  BLI_assert(parent_context != nullptr);
  BLI_assert(parent_node != nullptr);

  const int socket_index = bsocket_->index();
  return {parent_context, &parent_node->output_socket(socket_index)};
}

Vector<DOutputSocket> DInputSocket::get_corresponding_group_input_sockets() const
{
  BLI_assert(*this);
  BLI_assert(bsocket_->owner_node().is_group());

  const DTreeContext *child_context = context_->child_context(bsocket_->owner_node());
  BLI_assert(child_context != nullptr);

  const bNodeTree &child_tree = child_context->btree();
  Span<const bNode *> group_input_nodes = child_tree.group_input_nodes();
  const int socket_index = bsocket_->index();
  Vector<DOutputSocket> sockets;
  for (const bNode *group_input_node : group_input_nodes) {
    sockets.append(DOutputSocket(child_context, &group_input_node->output_socket(socket_index)));
  }
  return sockets;
}

DInputSocket DOutputSocket::get_corresponding_group_node_input() const
{
  BLI_assert(*this);
  BLI_assert(bsocket_->owner_node().is_group_input());
  BLI_assert(bsocket_->index() < bsocket_->owner_node().output_sockets().size() - 1);

  const DTreeContext *parent_context = context_->parent_context();
  const bNode *parent_node = context_->parent_node();
  BLI_assert(parent_context != nullptr);
  BLI_assert(parent_node != nullptr);

  const int socket_index = bsocket_->index();
  return {parent_context, &parent_node->input_socket(socket_index)};
}

DInputSocket DOutputSocket::get_active_corresponding_group_output_socket() const
{
  BLI_assert(*this);
  BLI_assert(bsocket_->owner_node().is_group());

  const DTreeContext *child_context = context_->child_context(bsocket_->owner_node());
  if (child_context == nullptr) {
    /* Can happen when the group node references a non-existent group (e.g. when the group is
     * linked but the original file is not found). */
    return {};
  }

  const bNodeTree &child_tree = child_context->btree();
  Span<const bNode *> group_output_nodes = child_tree.nodes_by_type("NodeGroupOutput");
  const int socket_index = bsocket_->index();
  for (const bNode *group_output_node : group_output_nodes) {
    if (group_output_node->flag & NODE_DO_OUTPUT || group_output_nodes.size() == 1) {
      return {child_context, &group_output_node->input_socket(socket_index)};
    }
  }
  return {};
}

void DInputSocket::foreach_origin_socket(FunctionRef<void(DSocket)> origin_fn) const
{
  BLI_assert(*this);
  for (const bNodeSocket *linked_socket : bsocket_->logically_linked_sockets()) {
    const bNode &linked_node = linked_socket->owner_node();
    DOutputSocket linked_dsocket{context_, linked_socket};

    if (linked_node.is_group_input()) {
      if (context_->is_root()) {
        /* This is a group input in the root node group. */
        origin_fn(linked_dsocket);
      }
      else {
        DInputSocket socket_in_parent_group = linked_dsocket.get_corresponding_group_node_input();
        if (socket_in_parent_group->is_logically_linked()) {
          /* Follow the links coming into the corresponding socket on the parent group node. */
          socket_in_parent_group.foreach_origin_socket(origin_fn);
        }
        else {
          /* The corresponding input on the parent group node is not connected. Therefore, we use
           * the value of that input socket directly. */
          origin_fn(socket_in_parent_group);
        }
      }
    }
    else if (linked_node.is_group()) {
      DInputSocket socket_in_group = linked_dsocket.get_active_corresponding_group_output_socket();
      if (socket_in_group) {
        if (socket_in_group->is_logically_linked()) {
          /* Follow the links coming into the group output node of the child node group. */
          socket_in_group.foreach_origin_socket(origin_fn);
        }
        else {
          /* The output of the child node group is not connected, so we have to get the value from
           * that socket. */
          origin_fn(socket_in_group);
        }
      }
    }
    else {
      /* The normal case: just use the value of a linked output socket. */
      origin_fn(linked_dsocket);
    }
  }
}

void DOutputSocket::foreach_target_socket(ForeachTargetSocketFn target_fn) const
{
  TargetSocketPathInfo path_info;
  this->foreach_target_socket(target_fn, path_info);
}

void DOutputSocket::foreach_target_socket(ForeachTargetSocketFn target_fn,
                                          TargetSocketPathInfo &path_info) const
{
  for (const bNodeLink *link : bsocket_->directly_linked_links()) {
    if (link->is_muted()) {
      continue;
    }
    const DInputSocket &linked_socket{context_, link->tosock};
    if (!linked_socket->is_available()) {
      continue;
    }
    const DNode linked_node = linked_socket.node();
    if (linked_node->is_reroute()) {
      const DInputSocket reroute_input = linked_socket;
      const DOutputSocket reroute_output = linked_node.output(0);
      path_info.sockets.append(reroute_input);
      path_info.sockets.append(reroute_output);
      reroute_output.foreach_target_socket(target_fn, path_info);
      path_info.sockets.pop_last();
      path_info.sockets.pop_last();
    }
    else if (linked_node->is_muted()) {
      for (const bNodeLink &internal_link : linked_node->internal_links()) {
        if (internal_link.fromsock != linked_socket.bsocket()) {
          continue;
        }
        /* The internal link only forwards the first incoming link. */
        if (linked_socket->is_multi_input()) {
          if (linked_socket->directly_linked_links()[0] != link) {
            continue;
          }
        }
        const DInputSocket mute_input = linked_socket;
        const DOutputSocket mute_output{context_, internal_link.tosock};
        path_info.sockets.append(mute_input);
        path_info.sockets.append(mute_output);
        mute_output.foreach_target_socket(target_fn, path_info);
        path_info.sockets.pop_last();
        path_info.sockets.pop_last();
      }
    }
    else if (linked_node->is_group_output()) {
      if (linked_node.bnode() != context_->btree().group_output_node()) {
        continue;
      }
      if (context_->is_root()) {
        /* This is a group output in the root node group. */
        path_info.sockets.append(linked_socket);
        target_fn(linked_socket, path_info);
        path_info.sockets.pop_last();
      }
      else {
        /* Follow the links going out of the group node in the parent node group. */
        const DOutputSocket socket_in_parent_group =
            linked_socket.get_corresponding_group_node_output();
        path_info.sockets.append(linked_socket);
        path_info.sockets.append(socket_in_parent_group);
        socket_in_parent_group.foreach_target_socket(target_fn, path_info);
        path_info.sockets.pop_last();
        path_info.sockets.pop_last();
      }
    }
    else if (linked_node->is_group()) {
      /* Follow the links within the nested node group. */
      path_info.sockets.append(linked_socket);
      const Vector<DOutputSocket> sockets_in_group =
          linked_socket.get_corresponding_group_input_sockets();
      for (const DOutputSocket &socket_in_group : sockets_in_group) {
        path_info.sockets.append(socket_in_group);
        socket_in_group.foreach_target_socket(target_fn, path_info);
        path_info.sockets.pop_last();
      }
      path_info.sockets.pop_last();
    }
    else {
      /* The normal case: just use the linked input socket as target. */
      path_info.sockets.append(linked_socket);
      target_fn(linked_socket, path_info);
      path_info.sockets.pop_last();
    }
  }
}

/* Find the active context from the given context and its descendants contexts. The active context
 * is the one whose node instance key matches the active_viewer_key stored in the root node tree.
 * The instance key of each context is computed by calling node_instance_key given the key of
 * the parent as well as the group node making the context. */
static const DTreeContext *find_active_context_recursive(const DTreeContext *context)
{
  const bNodeInstanceKey key = context->instance_key();

  /* The instance key of the given context matches the active viewer instance key, so this is the
   * active context, return it. */
  if (key.value == context->derived_tree().root_context().btree().active_viewer_key.value) {
    return context;
  }

  /* For each of the group nodes, compute their instance key and contexts and call this function
   * recursively. */
  for (const bNode *group_node : context->btree().group_nodes()) {
    /* No valid context exists for node groups without node trees. */
    if (!group_node->id) {
      continue;
    }
    const DTreeContext *child_context = context->child_context(*group_node);
    const DTreeContext *found_context = find_active_context_recursive(child_context);

    /* If the found context is null, that means neither the child context nor one of its descendant
     * contexts is active. */
    if (!found_context) {
      continue;
    }

    /* Otherwise, we have found our active context, return it. */
    return found_context;
  }

  /* Neither the given context nor one of its descendant contexts is active, so return null. */
  return nullptr;
}

const DTreeContext &DerivedNodeTree::active_context() const
{
  /* If the active viewer key is bke::NODE_INSTANCE_KEY_NONE, that means it is not yet initialized
   * and we return the root context in that case. See the find_active_context_recursive function.
   */
  if (root_context().btree().active_viewer_key.value == bke::NODE_INSTANCE_KEY_NONE.value) {
    return root_context();
  }

  const DTreeContext *found_context = find_active_context_recursive(&root_context());
  if (found_context == nullptr) {
    /* There should always be a valid active context. */
    BLI_assert_unreachable();
    return root_context();
  }

  return *found_context;
}

/* Each nested node group gets its own cluster. Just as node groups, clusters can be nested. */
static dot_export::Cluster *get_dot_cluster_for_context(
    dot_export::DirectedGraph &digraph,
    const DTreeContext *context,
    Map<const DTreeContext *, dot_export::Cluster *> &dot_clusters)
{
  return dot_clusters.lookup_or_add_cb(context, [&]() -> dot_export::Cluster * {
    const DTreeContext *parent_context = context->parent_context();
    if (parent_context == nullptr) {
      return nullptr;
    }
    dot_export::Cluster *parent_cluster = get_dot_cluster_for_context(
        digraph, parent_context, dot_clusters);
    std::string cluster_name = StringRef(context->btree().id.name + 2) + " / " +
                               context->parent_node()->name;
    dot_export::Cluster &cluster = digraph.new_cluster(cluster_name);
    cluster.set_parent_cluster(parent_cluster);
    return &cluster;
  });
}

std::string DerivedNodeTree::to_dot() const
{
  namespace dot = dot_export;

  dot_export::DirectedGraph digraph;
  digraph.set_rankdir(dot_export::Attr_rankdir::LeftToRight);

  Map<const DTreeContext *, dot_export::Cluster *> dot_clusters;
  Map<DInputSocket, dot_export::NodePort> dot_input_sockets;
  Map<DOutputSocket, dot_export::NodePort> dot_output_sockets;

  this->foreach_node([&](DNode node) {
    /* Ignore nodes that should not show up in the final output. */
    if (node->is_muted() || node->is_group() || node->is_reroute() || node->is_frame()) {
      return;
    }
    if (!node.context()->is_root()) {
      if (node->is_group_input() || node->is_group_output()) {
        return;
      }
    }

    dot_export::Cluster *cluster = get_dot_cluster_for_context(
        digraph, node.context(), dot_clusters);

    dot_export::Node &dot_node = digraph.new_node("");
    dot_node.set_parent_cluster(cluster);
    dot_node.set_background_color("white");

    dot_export::NodeWithSockets dot_node_with_sockets;
    for (const bNodeSocket *socket : node->input_sockets()) {
      if (socket->is_available()) {
        dot_node_with_sockets.add_input(socket->name);
      }
    }
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (socket->is_available()) {
        dot_node_with_sockets.add_output(socket->name);
      }
    }

    dot_export::NodeWithSocketsRef dot_node_with_sockets_ref = dot_export::NodeWithSocketsRef(
        dot_node, dot_node_with_sockets);

    int input_index = 0;
    for (const bNodeSocket *socket : node->input_sockets()) {
      if (socket->is_available()) {
        dot_input_sockets.add_new(DInputSocket{node.context(), socket},
                                  dot_node_with_sockets_ref.input(input_index));
        input_index++;
      }
    }
    int output_index = 0;
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (socket->is_available()) {
        dot_output_sockets.add_new(DOutputSocket{node.context(), socket},
                                   dot_node_with_sockets_ref.output(output_index));
        output_index++;
      }
    }
  });

  /* Floating inputs are used for example to visualize unlinked group node inputs. */
  Map<DSocket, dot_export::Node *> dot_floating_inputs;

  for (const auto item : dot_input_sockets.items()) {
    DInputSocket to_socket = item.key;
    dot_export::NodePort dot_to_port = item.value;
    to_socket.foreach_origin_socket([&](DSocket from_socket) {
      if (from_socket->is_output()) {
        dot_export::NodePort *dot_from_port = dot_output_sockets.lookup_ptr(
            DOutputSocket(from_socket));
        if (dot_from_port != nullptr) {
          digraph.new_edge(*dot_from_port, dot_to_port);
          return;
        }
      }
      dot_export::Node &dot_node = *dot_floating_inputs.lookup_or_add_cb(from_socket, [&]() {
        dot_export::Node &dot_node = digraph.new_node(from_socket->name);
        dot_node.set_background_color("white");
        dot_node.set_shape(dot_export::Attr_shape::Ellipse);
        dot_node.set_parent_cluster(
            get_dot_cluster_for_context(digraph, from_socket.context(), dot_clusters));
        return &dot_node;
      });
      digraph.new_edge(dot_node, dot_to_port);
    });
  }

  digraph.set_random_cluster_bgcolors();

  return digraph.to_dot_string();
}

}  // namespace blender::nodes
