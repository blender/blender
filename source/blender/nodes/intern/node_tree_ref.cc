/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "NOD_node_tree_ref.hh"

#include "BLI_dot_export.hh"
#include "BLI_stack.hh"

#include "RNA_prototypes.h"

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
      BLI_assert(internal_link.from_ != nullptr);
      BLI_assert(internal_link.to_ != nullptr);
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

  this->create_socket_identifier_maps();
  this->create_linked_socket_caches();

  for (NodeRef *node : nodes_by_id_) {
    const bNodeType *nodetype = node->bnode_->typeinfo;
    nodes_by_type_.add(nodetype, node);
  }

  const Span<const NodeRef *> group_output_nodes = this->nodes_by_type("NodeGroupOutput");
  if (group_output_nodes.is_empty()) {
    group_output_node_ = nullptr;
  }
  else if (group_output_nodes.size() == 1) {
    group_output_node_ = group_output_nodes.first();
  }
  else {
    for (const NodeRef *group_output : group_output_nodes) {
      if (group_output->bnode_->flag & NODE_DO_OUTPUT) {
        group_output_node_ = group_output;
        break;
      }
    }
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
    Vector<const InputSocketRef *> seen_sockets_stack;
    socket->foreach_logical_origin(
        [&](const OutputSocketRef &origin) { logically_linked_sockets.append(&origin); },
        [&](const SocketRef &socket) { logically_linked_skipped_sockets.append(&socket); },
        false,
        seen_sockets_stack);
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

void InputSocketRef::foreach_logical_origin(
    FunctionRef<void(const OutputSocketRef &)> origin_fn,
    FunctionRef<void(const SocketRef &)> skipped_fn,
    bool only_follow_first_input_link,
    Vector<const InputSocketRef *> &seen_sockets_stack) const
{
  /* Protect against loops. */
  if (seen_sockets_stack.contains(this)) {
    return;
  }
  seen_sockets_stack.append(this);

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
    if (!origin.is_available()) {
      /* Non available sockets are ignored. */
    }
    else if (origin_node.is_reroute_node()) {
      const InputSocketRef &reroute_input = origin_node.input(0);
      const OutputSocketRef &reroute_output = origin_node.output(0);
      skipped_fn.call_safe(reroute_input);
      skipped_fn.call_safe(reroute_output);
      reroute_input.foreach_logical_origin(origin_fn, skipped_fn, false, seen_sockets_stack);
    }
    else if (origin_node.is_muted()) {
      for (const InternalLinkRef *internal_link : origin_node.internal_links()) {
        if (&internal_link->to() == &origin) {
          const InputSocketRef &mute_input = internal_link->from();
          skipped_fn.call_safe(origin);
          skipped_fn.call_safe(mute_input);
          mute_input.foreach_logical_origin(origin_fn, skipped_fn, true, seen_sockets_stack);
        }
      }
    }
    else {
      origin_fn(origin);
    }
  }

  seen_sockets_stack.pop_last();
}

void OutputSocketRef::foreach_logical_target(
    FunctionRef<void(const InputSocketRef &)> target_fn,
    FunctionRef<void(const SocketRef &)> skipped_fn,
    Vector<const OutputSocketRef *> &seen_sockets_stack) const
{
  /* Protect against loops. */
  if (seen_sockets_stack.contains(this)) {
    return;
  }
  seen_sockets_stack.append(this);

  for (const LinkRef *link : this->directly_linked_links()) {
    if (link->is_muted()) {
      continue;
    }
    const InputSocketRef &target = link->to();
    const NodeRef &target_node = target.node();
    if (!target.is_available()) {
      /* Non available sockets are ignored. */
    }
    else if (target_node.is_reroute_node()) {
      const OutputSocketRef &reroute_output = target_node.output(0);
      skipped_fn.call_safe(target);
      skipped_fn.call_safe(reroute_output);
      reroute_output.foreach_logical_target(target_fn, skipped_fn, seen_sockets_stack);
    }
    else if (target_node.is_muted()) {
      skipped_fn.call_safe(target);
      for (const InternalLinkRef *internal_link : target_node.internal_links()) {
        if (&internal_link->from() == &target) {
          /* The internal link only forwards the first incoming link. */
          if (target.is_multi_input_socket()) {
            if (target.directly_linked_links()[0] != link) {
              continue;
            }
          }
          const OutputSocketRef &mute_output = internal_link->to();
          skipped_fn.call_safe(target);
          skipped_fn.call_safe(mute_output);
          mute_output.foreach_logical_target(target_fn, skipped_fn, seen_sockets_stack);
        }
      }
    }
    else {
      target_fn(target);
    }
  }

  seen_sockets_stack.pop_last();
}

namespace {
struct SocketByIdentifierMap {
  SocketIndexByIdentifierMap *map = nullptr;
  std::unique_ptr<SocketIndexByIdentifierMap> owned_map;
};
}  // namespace

static std::unique_ptr<SocketIndexByIdentifierMap> create_identifier_map(const ListBase &sockets)
{
  std::unique_ptr<SocketIndexByIdentifierMap> map = std::make_unique<SocketIndexByIdentifierMap>();
  int index;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &sockets, index) {
    map->add_new(socket->identifier, index);
  }
  return map;
}

/* This function is not threadsafe. */
static SocketByIdentifierMap get_or_create_identifier_map(
    const bNode &node, const ListBase &sockets, const bNodeSocketTemplate *sockets_template)
{
  SocketByIdentifierMap map;
  if (sockets_template == nullptr) {
    if (BLI_listbase_is_empty(&sockets)) {
      static SocketIndexByIdentifierMap empty_map;
      map.map = &empty_map;
    }
    else if (node.type == NODE_REROUTE) {
      if (&node.inputs == &sockets) {
        static SocketIndexByIdentifierMap reroute_input_map = [] {
          SocketIndexByIdentifierMap map;
          map.add_new("Input", 0);
          return map;
        }();
        map.map = &reroute_input_map;
      }
      else {
        static SocketIndexByIdentifierMap reroute_output_map = [] {
          SocketIndexByIdentifierMap map;
          map.add_new("Output", 0);
          return map;
        }();
        map.map = &reroute_output_map;
      }
    }
    else {
      /* The node has a dynamic amount of sockets. Therefore we need to create a new map. */
      map.owned_map = create_identifier_map(sockets);
      map.map = &*map.owned_map;
    }
  }
  else {
    /* Cache only one map for nodes that have the same sockets. */
    static Map<const bNodeSocketTemplate *, std::unique_ptr<SocketIndexByIdentifierMap>> maps;
    map.map = &*maps.lookup_or_add_cb(sockets_template,
                                      [&]() { return create_identifier_map(sockets); });
  }
  return map;
}

void NodeTreeRef::create_socket_identifier_maps()
{
  /* `get_or_create_identifier_map` is not threadsafe, therefore we have to hold a lock here. */
  static std::mutex mutex;
  std::lock_guard lock{mutex};

  for (NodeRef *node : nodes_by_id_) {
    bNode &bnode = *node->bnode_;
    SocketByIdentifierMap inputs_map = get_or_create_identifier_map(
        bnode, bnode.inputs, bnode.typeinfo->inputs);
    SocketByIdentifierMap outputs_map = get_or_create_identifier_map(
        bnode, bnode.outputs, bnode.typeinfo->outputs);
    node->input_index_by_identifier_ = inputs_map.map;
    node->output_index_by_identifier_ = outputs_map.map;
    if (inputs_map.owned_map) {
      owned_identifier_maps_.append(std::move(inputs_map.owned_map));
    }
    if (outputs_map.owned_map) {
      owned_identifier_maps_.append(std::move(outputs_map.owned_map));
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
    if (!from_socket->is_available()) {
      continue;
    }
    for (const InputSocketRef *to_socket : from_socket->directly_linked_sockets()) {
      if (!to_socket->is_available()) {
        continue;
      }
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

bool NodeTreeRef::has_undefined_nodes_or_sockets() const
{
  for (const NodeRef *node : nodes_by_id_) {
    if (node->is_undefined()) {
      return true;
    }
  }
  for (const SocketRef *socket : sockets_by_id_) {
    if (socket->is_undefined()) {
      return true;
    }
  }
  return false;
}

bool NodeRef::any_input_is_directly_linked() const
{
  for (const SocketRef *socket : inputs_) {
    if (!socket->directly_linked_sockets().is_empty()) {
      return true;
    }
  }
  return false;
}

bool NodeRef::any_output_is_directly_linked() const
{
  for (const SocketRef *socket : outputs_) {
    if (!socket->directly_linked_sockets().is_empty()) {
      return true;
    }
  }
  return false;
}

bool NodeRef::any_socket_is_directly_linked(eNodeSocketInOut in_out) const
{
  if (in_out == SOCK_IN) {
    return this->any_input_is_directly_linked();
  }
  return this->any_output_is_directly_linked();
}

struct ToposortNodeState {
  bool is_done = false;
  bool is_in_stack = false;
};

static void toposort_from_start_node(const NodeTreeRef::ToposortDirection direction,
                                     const NodeRef &start_node,
                                     MutableSpan<ToposortNodeState> node_states,
                                     NodeTreeRef::ToposortResult &result)
{
  struct Item {
    const NodeRef *node;
    /* Index of the next socket that is checked in the depth-first search. */
    int socket_index = 0;
    /* Link index in the next socket that is checked in the depth-first search. */
    int link_index = 0;
  };

  /* Do a depth-first search to sort nodes topologically. */
  Stack<Item, 64> nodes_to_check;
  nodes_to_check.push({&start_node});
  while (!nodes_to_check.is_empty()) {
    Item &item = nodes_to_check.peek();
    const NodeRef &node = *item.node;
    const Span<const SocketRef *> sockets = node.sockets(
        direction == NodeTreeRef::ToposortDirection::LeftToRight ? SOCK_IN : SOCK_OUT);

    while (true) {
      if (item.socket_index == sockets.size()) {
        /* All sockets have already been visited. */
        break;
      }
      const SocketRef &socket = *sockets[item.socket_index];
      const Span<const SocketRef *> linked_sockets = socket.directly_linked_sockets();
      if (item.link_index == linked_sockets.size()) {
        /* All links connected to this socket have already been visited. */
        item.socket_index++;
        item.link_index = 0;
        continue;
      }
      const SocketRef &linked_socket = *linked_sockets[item.link_index];
      const NodeRef &linked_node = linked_socket.node();
      ToposortNodeState &linked_node_state = node_states[linked_node.id()];
      if (linked_node_state.is_done) {
        /* The linked node has already been visited. */
        item.link_index++;
        continue;
      }
      if (linked_node_state.is_in_stack) {
        result.has_cycle = true;
      }
      else {
        nodes_to_check.push({&linked_node});
        linked_node_state.is_in_stack = true;
      }
      break;
    }

    /* If no other element has been pushed, the current node can be pushed to the sorted list. */
    if (&item == &nodes_to_check.peek()) {
      ToposortNodeState &node_state = node_states[node.id()];
      node_state.is_done = true;
      node_state.is_in_stack = false;
      result.sorted_nodes.append(&node);
      nodes_to_check.pop();
    }
  }
}

NodeTreeRef::ToposortResult NodeTreeRef::toposort(const ToposortDirection direction) const
{
  ToposortResult result;
  result.sorted_nodes.reserve(nodes_by_id_.size());

  Array<ToposortNodeState> node_states(nodes_by_id_.size());

  for (const NodeRef *node : nodes_by_id_) {
    if (node_states[node->id()].is_done) {
      /* Ignore nodes that are done already. */
      continue;
    }
    if (node->any_socket_is_directly_linked(
            direction == ToposortDirection::LeftToRight ? SOCK_OUT : SOCK_IN)) {
      /* Ignore non-start nodes. */
      continue;
    }

    toposort_from_start_node(direction, *node, node_states, result);
  }

  /* Check if the loop above forgot some nodes because there is a cycle. */
  if (result.sorted_nodes.size() < nodes_by_id_.size()) {
    result.has_cycle = true;
    for (const NodeRef *node : nodes_by_id_) {
      if (node_states[node->id()].is_done) {
        /* Ignore nodes that are done already. */
        continue;
      }
      /* Start toposort at this node which is somewhere in the middle of a loop. */
      toposort_from_start_node(direction, *node, node_states, result);
    }
  }

  BLI_assert(result.sorted_nodes.size() == nodes_by_id_.size());
  return result;
}

const NodeRef *NodeTreeRef::find_node(const bNode &bnode) const
{
  for (const NodeRef *node : this->nodes_by_type(bnode.typeinfo)) {
    if (node->bnode_ == &bnode) {
      return node;
    }
  }
  return nullptr;
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
