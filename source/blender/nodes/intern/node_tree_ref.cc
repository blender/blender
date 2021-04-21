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

#include "NOD_node_tree_ref.hh"

#include "BLI_dot_export.hh"

namespace blender::nodes {

NodeTreeRef::NodeTreeRef(bNodeTree *btree) : btree_(btree)
{
  Map<bNode *, NodeRef *> node_mapping;

  LISTBASE_FOREACH (bNode *, bnode, &btree->nodes) {
    NodeRef &node = *allocator_.construct<NodeRef>().release();

    node.tree_ = this;
    node.bnode_ = bnode;
    node.id_ = nodes_by_id_.append_and_get_index(&node);
    RNA_pointer_create(&btree->id, &RNA_Node, bnode, &node.rna_);

    LISTBASE_FOREACH (bNodeSocket *, bsocket, &bnode->inputs) {
      InputSocketRef &socket = *allocator_.construct<InputSocketRef>().release();
      socket.node_ = &node;
      socket.index_ = node.inputs_.append_and_get_index(&socket);
      socket.is_input_ = true;
      socket.bsocket_ = bsocket;
      socket.id_ = sockets_by_id_.append_and_get_index(&socket);
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &socket.rna_);
    }

    LISTBASE_FOREACH (bNodeSocket *, bsocket, &bnode->outputs) {
      OutputSocketRef &socket = *allocator_.construct<OutputSocketRef>().release();
      socket.node_ = &node;
      socket.index_ = node.outputs_.append_and_get_index(&socket);
      socket.is_input_ = false;
      socket.bsocket_ = bsocket;
      socket.id_ = sockets_by_id_.append_and_get_index(&socket);
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &socket.rna_);
    }

    LISTBASE_FOREACH (bNodeLink *, blink, &bnode->internal_links) {
      InternalLinkRef &internal_link = *allocator_.construct<InternalLinkRef>().release();
      internal_link.blink_ = blink;
      for (InputSocketRef *socket_ref : node.inputs_) {
        if (socket_ref->bsocket_ == blink->fromsock) {
          internal_link.from_ = socket_ref;
          break;
        }
      }
      for (OutputSocketRef *socket_ref : node.outputs_) {
        if (socket_ref->bsocket_ == blink->tosock) {
          internal_link.to_ = socket_ref;
          break;
        }
      }
      node.internal_links_.append(&internal_link);
    }

    input_sockets_.extend(node.inputs_.as_span());
    output_sockets_.extend(node.outputs_.as_span());

    node_mapping.add_new(bnode, &node);
  }

  LISTBASE_FOREACH (bNodeLink *, blink, &btree->links) {
    OutputSocketRef &from_socket = this->find_output_socket(
        node_mapping, blink->fromnode, blink->fromsock);
    InputSocketRef &to_socket = this->find_input_socket(
        node_mapping, blink->tonode, blink->tosock);

    LinkRef &link = *allocator_.construct<LinkRef>().release();
    link.from_ = &from_socket;
    link.to_ = &to_socket;
    link.blink_ = blink;

    links_.append(&link);

    from_socket.directly_linked_links_.append(&link);
    to_socket.directly_linked_links_.append(&link);
  }

  for (InputSocketRef *input_socket : input_sockets_) {
    if (input_socket->is_multi_input_socket()) {
      std::sort(input_socket->directly_linked_links_.begin(),
                input_socket->directly_linked_links_.end(),
                [&](const LinkRef *a, const LinkRef *b) -> bool {
                  int index_a = a->blink()->multi_input_socket_index;
                  int index_b = b->blink()->multi_input_socket_index;
                  return index_a > index_b;
                });
    }
  }

  this->create_linked_socket_caches();

  for (NodeRef *node : nodes_by_id_) {
    const bNodeType *nodetype = node->bnode_->typeinfo;
    nodes_by_type_.add(nodetype, node);
  }
}

NodeTreeRef::~NodeTreeRef()
{
  /* The destructor has to be called manually, because these types are allocated in a linear
   * allocator. */
  for (NodeRef *node : nodes_by_id_) {
    node->~NodeRef();
  }
  for (InputSocketRef *socket : input_sockets_) {
    socket->~InputSocketRef();
  }
  for (OutputSocketRef *socket : output_sockets_) {
    socket->~OutputSocketRef();
  }
  for (LinkRef *link : links_) {
    link->~LinkRef();
  }
}

InputSocketRef &NodeTreeRef::find_input_socket(Map<bNode *, NodeRef *> &node_mapping,
                                               bNode *bnode,
                                               bNodeSocket *bsocket)
{
  NodeRef *node = node_mapping.lookup(bnode);
  for (InputSocketRef *socket : node->inputs_) {
    if (socket->bsocket_ == bsocket) {
      return *socket;
    }
  }
  BLI_assert_unreachable();
  return *node->inputs_[0];
}

OutputSocketRef &NodeTreeRef::find_output_socket(Map<bNode *, NodeRef *> &node_mapping,
                                                 bNode *bnode,
                                                 bNodeSocket *bsocket)
{
  NodeRef *node = node_mapping.lookup(bnode);
  for (OutputSocketRef *socket : node->outputs_) {
    if (socket->bsocket_ == bsocket) {
      return *socket;
    }
  }
  BLI_assert_unreachable();
  return *node->outputs_[0];
}

void NodeTreeRef::create_linked_socket_caches()
{
  for (InputSocketRef *socket : input_sockets_) {
    /* Find directly linked socket based on incident links. */
    Vector<const SocketRef *> directly_linked_sockets;
    for (LinkRef *link : socket->directly_linked_links_) {
      directly_linked_sockets.append(link->from_);
    }
    socket->directly_linked_sockets_ = allocator_.construct_array_copy(
        directly_linked_sockets.as_span());

    /* Find logically linked sockets. */
    Vector<const SocketRef *> logically_linked_sockets;
    Vector<const SocketRef *> logically_linked_skipped_sockets;
    Vector<const InputSocketRef *> handled_sockets;
    socket->foreach_logical_origin(
        [&](const OutputSocketRef &origin) { logically_linked_sockets.append(&origin); },
        [&](const SocketRef &socket) { logically_linked_skipped_sockets.append(&socket); },
        false,
        handled_sockets);
    if (logically_linked_sockets == directly_linked_sockets) {
      socket->logically_linked_sockets_ = socket->directly_linked_sockets_;
    }
    else {
      socket->logically_linked_sockets_ = allocator_.construct_array_copy(
          logically_linked_sockets.as_span());
    }
    socket->logically_linked_skipped_sockets_ = allocator_.construct_array_copy(
        logically_linked_skipped_sockets.as_span());
  }

  for (OutputSocketRef *socket : output_sockets_) {
    /* Find directly linked socket based on incident links. */
    Vector<const SocketRef *> directly_linked_sockets;
    for (LinkRef *link : socket->directly_linked_links_) {
      directly_linked_sockets.append(link->to_);
    }
    socket->directly_linked_sockets_ = allocator_.construct_array_copy(
        directly_linked_sockets.as_span());

    /* Find logically linked sockets. */
    Vector<const SocketRef *> logically_linked_sockets;
    Vector<const SocketRef *> logically_linked_skipped_sockets;
    Vector<const OutputSocketRef *> handled_sockets;
    socket->foreach_logical_target(
        [&](const InputSocketRef &target) { logically_linked_sockets.append(&target); },
        [&](const SocketRef &socket) { logically_linked_skipped_sockets.append(&socket); },
        handled_sockets);
    if (logically_linked_sockets == directly_linked_sockets) {
      socket->logically_linked_sockets_ = socket->directly_linked_sockets_;
    }
    else {
      socket->logically_linked_sockets_ = allocator_.construct_array_copy(
          logically_linked_sockets.as_span());
    }
    socket->logically_linked_skipped_sockets_ = allocator_.construct_array_copy(
        logically_linked_skipped_sockets.as_span());
  }
}

void InputSocketRef::foreach_logical_origin(FunctionRef<void(const OutputSocketRef &)> origin_fn,
                                            FunctionRef<void(const SocketRef &)> skipped_fn,
                                            bool only_follow_first_input_link,
                                            Vector<const InputSocketRef *> &handled_sockets) const
{
  /* Protect against loops. */
  if (handled_sockets.contains(this)) {
    return;
  }
  handled_sockets.append(this);

  Span<const LinkRef *> links_to_check = this->directly_linked_links();
  if (only_follow_first_input_link) {
    links_to_check = links_to_check.take_front(1);
  }
  for (const LinkRef *link : links_to_check) {
    if (link->is_muted()) {
      continue;
    }
    const OutputSocketRef &origin = link->from();
    const NodeRef &origin_node = origin.node();
    if (origin_node.is_reroute_node()) {
      const InputSocketRef &reroute_input = origin_node.input(0);
      const OutputSocketRef &reroute_output = origin_node.output(0);
      skipped_fn.call_safe(reroute_input);
      skipped_fn.call_safe(reroute_output);
      reroute_input.foreach_logical_origin(origin_fn, skipped_fn, false, handled_sockets);
    }
    else if (origin_node.is_muted()) {
      for (const InternalLinkRef *internal_link : origin_node.internal_links()) {
        if (&internal_link->to() == &origin) {
          const InputSocketRef &mute_input = internal_link->from();
          skipped_fn.call_safe(origin);
          skipped_fn.call_safe(mute_input);
          mute_input.foreach_logical_origin(origin_fn, skipped_fn, true, handled_sockets);
          break;
        }
      }
    }
    else {
      origin_fn(origin);
    }
  }
}

void OutputSocketRef::foreach_logical_target(
    FunctionRef<void(const InputSocketRef &)> target_fn,
    FunctionRef<void(const SocketRef &)> skipped_fn,
    Vector<const OutputSocketRef *> &handled_sockets) const
{
  /* Protect against loops. */
  if (handled_sockets.contains(this)) {
    return;
  }
  handled_sockets.append(this);

  for (const LinkRef *link : this->directly_linked_links()) {
    if (link->is_muted()) {
      continue;
    }
    const InputSocketRef &target = link->to();
    const NodeRef &target_node = target.node();
    if (target_node.is_reroute_node()) {
      const OutputSocketRef &reroute_output = target_node.output(0);
      skipped_fn.call_safe(target);
      skipped_fn.call_safe(reroute_output);
      reroute_output.foreach_logical_target(target_fn, skipped_fn, handled_sockets);
    }
    else if (target_node.is_muted()) {
      skipped_fn.call_safe(target);
      for (const InternalLinkRef *internal_link : target_node.internal_links()) {
        if (&internal_link->from() == &target) {
          const OutputSocketRef &mute_output = internal_link->to();
          skipped_fn.call_safe(target);
          skipped_fn.call_safe(mute_output);
          mute_output.foreach_logical_target(target_fn, skipped_fn, handled_sockets);
        }
      }
    }
    else {
      target_fn(target);
    }
  }
}

static bool has_link_cycles_recursive(const NodeRef &node,
                                      MutableSpan<bool> visited,
                                      MutableSpan<bool> is_in_stack)
{
  const int node_id = node.id();
  if (is_in_stack[node_id]) {
    return true;
  }
  if (visited[node_id]) {
    return false;
  }

  visited[node_id] = true;
  is_in_stack[node_id] = true;

  for (const OutputSocketRef *from_socket : node.outputs()) {
    for (const InputSocketRef *to_socket : from_socket->directly_linked_sockets()) {
      const NodeRef &to_node = to_socket->node();
      if (has_link_cycles_recursive(to_node, visited, is_in_stack)) {
        return true;
      }
    }
  }

  is_in_stack[node_id] = false;
  return false;
}

bool NodeTreeRef::has_link_cycles() const
{
  const int node_amount = nodes_by_id_.size();
  Array<bool> visited(node_amount, false);
  Array<bool> is_in_stack(node_amount, false);

  for (const NodeRef *node : nodes_by_id_) {
    if (has_link_cycles_recursive(*node, visited, is_in_stack)) {
      return true;
    }
  }
  return false;
}

std::string NodeTreeRef::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const NodeRef *, dot::NodeWithSocketsRef> dot_nodes;

  for (const NodeRef *node : nodes_by_id_) {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_background_color("white");

    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const InputSocketRef *socket : node->inputs()) {
      input_names.append(socket->name());
    }
    for (const OutputSocketRef *socket : node->outputs()) {
      output_names.append(socket->name());
    }

    dot_nodes.add_new(node,
                      dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }

  for (const OutputSocketRef *from_socket : output_sockets_) {
    for (const InputSocketRef *to_socket : from_socket->directly_linked_sockets()) {
      dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(&from_socket->node());
      dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(&to_socket->node());

      digraph.new_edge(from_dot_node.output(from_socket->index()),
                       to_dot_node.input(to_socket->index()));
    }
  }

  return digraph.to_dot_string();
}

const NodeTreeRef &get_tree_ref_from_map(NodeTreeRefMap &node_tree_refs, bNodeTree &btree)
{
  return *node_tree_refs.lookup_or_add_cb(&btree,
                                          [&]() { return std::make_unique<NodeTreeRef>(&btree); });
}

}  // namespace blender::nodes
