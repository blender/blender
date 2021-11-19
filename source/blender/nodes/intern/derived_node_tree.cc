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

#include "NOD_derived_node_tree.hh"

#include "BLI_dot_export.hh"

namespace blender::nodes {

/* Construct a new derived node tree for a given root node tree. The generated derived node tree
 * does not own the used node tree refs (so that those can be used by others as well). The caller
 * has to make sure that the node tree refs added to #node_tree_refs live at least as long as the
 * derived node tree. */
DerivedNodeTree::DerivedNodeTree(bNodeTree &btree, NodeTreeRefMap &node_tree_refs)
{
  /* Construct all possible contexts immediately. This is significantly cheaper than inlining all
   * node groups. If it still becomes a performance issue in the future, contexts could be
   * constructed lazily when they are needed. */
  root_context_ = &this->construct_context_recursively(nullptr, nullptr, btree, node_tree_refs);
}

DTreeContext &DerivedNodeTree::construct_context_recursively(DTreeContext *parent_context,
                                                             const NodeRef *parent_node,
                                                             bNodeTree &btree,
                                                             NodeTreeRefMap &node_tree_refs)
{
  DTreeContext &context = *allocator_.construct<DTreeContext>().release();
  context.parent_context_ = parent_context;
  context.parent_node_ = parent_node;
  context.derived_tree_ = this;
  context.tree_ = &get_tree_ref_from_map(node_tree_refs, btree);
  used_node_tree_refs_.add(context.tree_);

  for (const NodeRef *node : context.tree_->nodes()) {
    if (node->is_group_node()) {
      bNode *bnode = node->bnode();
      bNodeTree *child_btree = reinterpret_cast<bNodeTree *>(bnode->id);
      if (child_btree != nullptr) {
        DTreeContext &child = this->construct_context_recursively(
            &context, node, *child_btree, node_tree_refs);
        context.children_.add_new(node, &child);
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

/**
 * \return True when there is a link cycle. Unavailable sockets are ignored.
 */
bool DerivedNodeTree::has_link_cycles() const
{
  for (const NodeTreeRef *tree_ref : used_node_tree_refs_) {
    if (tree_ref->has_link_cycles()) {
      return true;
    }
  }
  return false;
}

bool DerivedNodeTree::has_undefined_nodes_or_sockets() const
{
  for (const NodeTreeRef *tree_ref : used_node_tree_refs_) {
    if (tree_ref->has_undefined_nodes_or_sockets()) {
      return true;
    }
  }
  return false;
}

/* Calls the given callback on all nodes in the (possibly nested) derived node tree. */
void DerivedNodeTree::foreach_node(FunctionRef<void(DNode)> callback) const
{
  this->foreach_node_in_context_recursive(*root_context_, callback);
}

void DerivedNodeTree::foreach_node_in_context_recursive(const DTreeContext &context,
                                                        FunctionRef<void(DNode)> callback) const
{
  for (const NodeRef *node_ref : context.tree_->nodes()) {
    callback(DNode(&context, node_ref));
  }
  for (const DTreeContext *child_context : context.children_.values()) {
    this->foreach_node_in_context_recursive(*child_context, callback);
  }
}

DOutputSocket DInputSocket::get_corresponding_group_node_output() const
{
  BLI_assert(*this);
  BLI_assert(socket_ref_->node().is_group_output_node());
  BLI_assert(socket_ref_->index() < socket_ref_->node().inputs().size() - 1);

  const DTreeContext *parent_context = context_->parent_context();
  const NodeRef *parent_node = context_->parent_node();
  BLI_assert(parent_context != nullptr);
  BLI_assert(parent_node != nullptr);

  const int socket_index = socket_ref_->index();
  return {parent_context, &parent_node->output(socket_index)};
}

Vector<DOutputSocket> DInputSocket::get_corresponding_group_input_sockets() const
{
  BLI_assert(*this);
  BLI_assert(socket_ref_->node().is_group_node());

  const DTreeContext *child_context = context_->child_context(socket_ref_->node());
  BLI_assert(child_context != nullptr);

  const NodeTreeRef &child_tree = child_context->tree();
  Span<const NodeRef *> group_input_nodes = child_tree.nodes_by_type("NodeGroupInput");
  const int socket_index = socket_ref_->index();
  Vector<DOutputSocket> sockets;
  for (const NodeRef *group_input_node : group_input_nodes) {
    sockets.append(DOutputSocket(child_context, &group_input_node->output(socket_index)));
  }
  return sockets;
}

DInputSocket DOutputSocket::get_corresponding_group_node_input() const
{
  BLI_assert(*this);
  BLI_assert(socket_ref_->node().is_group_input_node());
  BLI_assert(socket_ref_->index() < socket_ref_->node().outputs().size() - 1);

  const DTreeContext *parent_context = context_->parent_context();
  const NodeRef *parent_node = context_->parent_node();
  BLI_assert(parent_context != nullptr);
  BLI_assert(parent_node != nullptr);

  const int socket_index = socket_ref_->index();
  return {parent_context, &parent_node->input(socket_index)};
}

DInputSocket DOutputSocket::get_active_corresponding_group_output_socket() const
{
  BLI_assert(*this);
  BLI_assert(socket_ref_->node().is_group_node());

  const DTreeContext *child_context = context_->child_context(socket_ref_->node());
  if (child_context == nullptr) {
    /* Can happen when the group node references a non-existant group (e.g. when the group is
     * linked but the original file is not found). */
    return {};
  }

  const NodeTreeRef &child_tree = child_context->tree();
  Span<const NodeRef *> group_output_nodes = child_tree.nodes_by_type("NodeGroupOutput");
  const int socket_index = socket_ref_->index();
  for (const NodeRef *group_output_node : group_output_nodes) {
    if (group_output_node->bnode()->flag & NODE_DO_OUTPUT || group_output_nodes.size() == 1) {
      return {child_context, &group_output_node->input(socket_index)};
    }
  }
  return {};
}

/* Call `origin_fn` for every "real" origin socket. "Real" means that reroutes, muted nodes
 * and node groups are handled by this function. Origin sockets are ones where a node gets its
 * inputs from. */
void DInputSocket::foreach_origin_socket(FunctionRef<void(DSocket)> origin_fn) const
{
  BLI_assert(*this);
  for (const OutputSocketRef *linked_socket : socket_ref_->as_input().logically_linked_sockets()) {
    const NodeRef &linked_node = linked_socket->node();
    DOutputSocket linked_dsocket{context_, linked_socket};

    if (linked_node.is_group_input_node()) {
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
    else if (linked_node.is_group_node()) {
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

/* Calls `target_fn` for every "real" target socket. "Real" means that reroutes, muted nodes
 * and node groups are handled by this function. Target sockets are on the nodes that use the value
 * from this socket. */
void DOutputSocket::foreach_target_socket(ForeachTargetSocketFn target_fn) const
{
  TargetSocketPathInfo path_info;
  this->foreach_target_socket(target_fn, path_info);
}

void DOutputSocket::foreach_target_socket(ForeachTargetSocketFn target_fn,
                                          TargetSocketPathInfo &path_info) const
{
  for (const LinkRef *link : socket_ref_->as_output().directly_linked_links()) {
    if (link->is_muted()) {
      continue;
    }
    const DInputSocket &linked_socket{context_, &link->to()};
    if (!linked_socket->is_available()) {
      continue;
    }
    const DNode linked_node = linked_socket.node();
    if (linked_node->is_reroute_node()) {
      const DInputSocket reroute_input = linked_socket;
      const DOutputSocket reroute_output = linked_node.output(0);
      path_info.sockets.append(reroute_input);
      path_info.sockets.append(reroute_output);
      reroute_output.foreach_target_socket(target_fn, path_info);
      path_info.sockets.pop_last();
      path_info.sockets.pop_last();
    }
    else if (linked_node->is_muted()) {
      for (const InternalLinkRef *internal_link : linked_node->internal_links()) {
        if (&internal_link->from() != linked_socket.socket_ref()) {
          continue;
        }
        /* The internal link only forwards the first incoming link. */
        if (linked_socket->is_multi_input_socket()) {
          if (linked_socket->directly_linked_links()[0] != link) {
            continue;
          }
        }
        const DInputSocket mute_input = linked_socket;
        const DOutputSocket mute_output{context_, &internal_link->to()};
        path_info.sockets.append(mute_input);
        path_info.sockets.append(mute_output);
        mute_output.foreach_target_socket(target_fn, path_info);
        path_info.sockets.pop_last();
        path_info.sockets.pop_last();
        break;
      }
    }
    else if (linked_node->is_group_output_node()) {
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
    else if (linked_node->is_group_node()) {
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

/* Each nested node group gets its own cluster. Just as node groups, clusters can be nested. */
static dot::Cluster *get_dot_cluster_for_context(
    dot::DirectedGraph &digraph,
    const DTreeContext *context,
    Map<const DTreeContext *, dot::Cluster *> &dot_clusters)
{
  return dot_clusters.lookup_or_add_cb(context, [&]() -> dot::Cluster * {
    const DTreeContext *parent_context = context->parent_context();
    if (parent_context == nullptr) {
      return nullptr;
    }
    dot::Cluster *parent_cluster = get_dot_cluster_for_context(
        digraph, parent_context, dot_clusters);
    std::string cluster_name = context->tree().name() + " / " + context->parent_node()->name();
    dot::Cluster &cluster = digraph.new_cluster(cluster_name);
    cluster.set_parent_cluster(parent_cluster);
    return &cluster;
  });
}

/* Generates a graph in dot format. The generated graph has all node groups inlined. */
std::string DerivedNodeTree::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const DTreeContext *, dot::Cluster *> dot_clusters;
  Map<DInputSocket, dot::NodePort> dot_input_sockets;
  Map<DOutputSocket, dot::NodePort> dot_output_sockets;

  this->foreach_node([&](DNode node) {
    /* Ignore nodes that should not show up in the final output. */
    if (node->is_muted() || node->is_group_node() || node->is_reroute_node() || node->is_frame()) {
      return;
    }
    if (!node.context()->is_root()) {
      if (node->is_group_input_node() || node->is_group_output_node()) {
        return;
      }
    }

    dot::Cluster *cluster = get_dot_cluster_for_context(digraph, node.context(), dot_clusters);

    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_parent_cluster(cluster);
    dot_node.set_background_color("white");

    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const InputSocketRef *socket : node->inputs()) {
      if (socket->is_available()) {
        input_names.append(socket->name());
      }
    }
    for (const OutputSocketRef *socket : node->outputs()) {
      if (socket->is_available()) {
        output_names.append(socket->name());
      }
    }

    dot::NodeWithSocketsRef dot_node_with_sockets = dot::NodeWithSocketsRef(
        dot_node, node->name(), input_names, output_names);

    int input_index = 0;
    for (const InputSocketRef *socket : node->inputs()) {
      if (socket->is_available()) {
        dot_input_sockets.add_new(DInputSocket{node.context(), socket},
                                  dot_node_with_sockets.input(input_index));
        input_index++;
      }
    }
    int output_index = 0;
    for (const OutputSocketRef *socket : node->outputs()) {
      if (socket->is_available()) {
        dot_output_sockets.add_new(DOutputSocket{node.context(), socket},
                                   dot_node_with_sockets.output(output_index));
        output_index++;
      }
    }
  });

  /* Floating inputs are used for example to visualize unlinked group node inputs. */
  Map<DSocket, dot::Node *> dot_floating_inputs;

  for (const auto item : dot_input_sockets.items()) {
    DInputSocket to_socket = item.key;
    dot::NodePort dot_to_port = item.value;
    to_socket.foreach_origin_socket([&](DSocket from_socket) {
      if (from_socket->is_output()) {
        dot::NodePort *dot_from_port = dot_output_sockets.lookup_ptr(DOutputSocket(from_socket));
        if (dot_from_port != nullptr) {
          digraph.new_edge(*dot_from_port, dot_to_port);
          return;
        }
      }
      dot::Node &dot_node = *dot_floating_inputs.lookup_or_add_cb(from_socket, [&]() {
        dot::Node &dot_node = digraph.new_node(from_socket->name());
        dot_node.set_background_color("white");
        dot_node.set_shape(dot::Attr_shape::Ellipse);
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
