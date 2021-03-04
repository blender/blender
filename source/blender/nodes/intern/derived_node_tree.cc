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

#define UNINITIALIZED_ID UINT32_MAX

namespace blender::nodes {

DerivedNodeTree::DerivedNodeTree(bNodeTree *btree, NodeTreeRefMap &node_tree_refs) : btree_(btree)
{
  BLI_assert(btree != nullptr);

  const NodeTreeRef &main_tree_ref = get_tree_ref_from_map(node_tree_refs, *btree);
  used_node_tree_refs_.add_new(&main_tree_ref);

  Vector<DNode *> all_nodes;
  Vector<DGroupInput *> all_group_inputs;
  Vector<DParentNode *> all_parent_nodes;

  this->insert_nodes_and_links_in_id_order(main_tree_ref, nullptr, all_nodes);
  this->expand_groups(all_nodes, all_group_inputs, all_parent_nodes, node_tree_refs);
  this->relink_and_remove_muted_nodes(all_nodes);
  this->remove_expanded_group_interfaces(all_nodes);
  this->remove_unused_group_inputs(all_group_inputs);
  this->store_in_this_and_init_ids(
      std::move(all_nodes), std::move(all_group_inputs), std::move(all_parent_nodes));
}

BLI_NOINLINE void DerivedNodeTree::insert_nodes_and_links_in_id_order(const NodeTreeRef &tree_ref,
                                                                      DParentNode *parent,
                                                                      Vector<DNode *> &all_nodes)
{
  Array<DSocket *, 64> sockets_map(tree_ref.sockets().size());

  /* Insert nodes. */
  for (const NodeRef *node_ref : tree_ref.nodes()) {
    DNode &node = this->create_node(*node_ref, parent, sockets_map);
    all_nodes.append(&node);
  }

  /* Insert links. */
  for (const NodeRef *node_ref : tree_ref.nodes()) {
    for (const InputSocketRef *to_socket_ref : node_ref->inputs()) {
      DInputSocket *to_socket = static_cast<DInputSocket *>(sockets_map[to_socket_ref->id()]);
      for (const OutputSocketRef *from_socket_ref : to_socket_ref->linked_sockets()) {
        DOutputSocket *from_socket = static_cast<DOutputSocket *>(
            sockets_map[from_socket_ref->id()]);
        to_socket->linked_sockets_.append(from_socket);
        from_socket->linked_sockets_.append(to_socket);
      }
    }
  }
}

DNode &DerivedNodeTree::create_node(const NodeRef &node_ref,
                                    DParentNode *parent,
                                    MutableSpan<DSocket *> r_sockets_map)
{
  DNode &node = *allocator_.construct<DNode>();
  node.node_ref_ = &node_ref;
  node.parent_ = parent;
  node.id_ = UNINITIALIZED_ID;

  node.inputs_ = allocator_.construct_elements_and_pointer_array<DInputSocket>(
      node_ref.inputs().size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<DOutputSocket>(
      node_ref.outputs().size());

  for (int i : node.inputs_.index_range()) {
    const InputSocketRef &socket_ref = node_ref.input(i);
    DInputSocket &socket = *node.inputs_[i];
    socket.is_multi_input_socket_ = socket_ref.bsocket()->flag & SOCK_MULTI_INPUT;
    socket.id_ = UNINITIALIZED_ID;
    socket.node_ = &node;
    socket.socket_ref_ = &socket_ref;

    r_sockets_map[socket_ref.id()] = &socket;
  }

  for (int i : node.outputs_.index_range()) {
    const OutputSocketRef &socket_ref = node_ref.output(i);
    DOutputSocket &socket = *node.outputs_[i];

    socket.id_ = UNINITIALIZED_ID;
    socket.node_ = &node;
    socket.socket_ref_ = &socket_ref;

    r_sockets_map[socket_ref.id()] = &socket;
  }

  return node;
}

BLI_NOINLINE void DerivedNodeTree::expand_groups(Vector<DNode *> &all_nodes,
                                                 Vector<DGroupInput *> &all_group_inputs,
                                                 Vector<DParentNode *> &all_parent_nodes,
                                                 NodeTreeRefMap &node_tree_refs)
{
  for (int i = 0; i < all_nodes.size(); i++) {
    DNode &node = *all_nodes[i];
    if (node.node_ref_->is_group_node()) {
      /* Muted nodes are relinked in a separate step. */
      if (!node.node_ref_->is_muted()) {
        this->expand_group_node(
            node, all_nodes, all_group_inputs, all_parent_nodes, node_tree_refs);
      }
    }
  }
}

BLI_NOINLINE void DerivedNodeTree::expand_group_node(DNode &group_node,
                                                     Vector<DNode *> &all_nodes,
                                                     Vector<DGroupInput *> &all_group_inputs,
                                                     Vector<DParentNode *> &all_parent_nodes,
                                                     NodeTreeRefMap &node_tree_refs)
{
  const NodeRef &group_node_ref = *group_node.node_ref_;
  BLI_assert(group_node_ref.is_group_node());

  bNodeTree *btree = reinterpret_cast<bNodeTree *>(group_node_ref.bnode()->id);
  if (btree == nullptr) {
    return;
  }

  const NodeTreeRef &group_ref = get_tree_ref_from_map(node_tree_refs, *btree);
  used_node_tree_refs_.add(&group_ref);

  DParentNode &parent = *allocator_.construct<DParentNode>();
  parent.id_ = all_parent_nodes.append_and_get_index(&parent);
  parent.parent_ = group_node.parent_;
  parent.node_ref_ = &group_node_ref;

  this->insert_nodes_and_links_in_id_order(group_ref, &parent, all_nodes);
  Span<DNode *> new_nodes_by_id = all_nodes.as_span().take_back(group_ref.nodes().size());

  this->create_group_inputs_for_unlinked_inputs(group_node, all_group_inputs);
  this->relink_group_inputs(group_ref, new_nodes_by_id, group_node);
  this->relink_group_outputs(group_ref, new_nodes_by_id, group_node);
}

BLI_NOINLINE void DerivedNodeTree::create_group_inputs_for_unlinked_inputs(
    DNode &node, Vector<DGroupInput *> &all_group_inputs)
{
  for (DInputSocket *input_socket : node.inputs_) {
    if (input_socket->is_linked()) {
      continue;
    }

    DGroupInput &group_input = *allocator_.construct<DGroupInput>();
    group_input.id_ = UNINITIALIZED_ID;
    group_input.socket_ref_ = &input_socket->socket_ref();
    group_input.parent_ = node.parent_;

    group_input.linked_sockets_.append(input_socket);
    input_socket->linked_group_inputs_.append(&group_input);
    all_group_inputs.append(&group_input);
  }
}

BLI_NOINLINE void DerivedNodeTree::relink_group_inputs(const NodeTreeRef &group_ref,
                                                       Span<DNode *> nodes_by_id,
                                                       DNode &group_node)
{
  Span<const NodeRef *> node_refs = group_ref.nodes_by_type("NodeGroupInput");
  if (node_refs.size() == 0) {
    return;
  }

  int input_amount = group_node.inputs().size();

  for (int input_index : IndexRange(input_amount)) {
    DInputSocket *outside_group = group_node.inputs_[input_index];

    for (DOutputSocket *outside_connected : outside_group->linked_sockets_) {
      outside_connected->linked_sockets_.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DGroupInput *outside_connected : outside_group->linked_group_inputs_) {
      outside_connected->linked_sockets_.remove_first_occurrence_and_reorder(outside_group);
    }

    for (const NodeRef *input_node_ref : node_refs) {
      DNode &input_node = *nodes_by_id[input_node_ref->id()];
      DOutputSocket *inside_group = input_node.outputs_[input_index];

      for (DInputSocket *inside_connected : inside_group->linked_sockets_) {
        inside_connected->linked_sockets_.remove_first_occurrence_and_reorder(inside_group);

        for (DOutputSocket *outside_connected : outside_group->linked_sockets_) {
          inside_connected->linked_sockets_.append(outside_connected);
          outside_connected->linked_sockets_.append(inside_connected);
        }

        for (DGroupInput *outside_connected : outside_group->linked_group_inputs_) {
          inside_connected->linked_group_inputs_.append(outside_connected);
          outside_connected->linked_sockets_.append(inside_connected);
        }
      }

      inside_group->linked_sockets_.clear();
    }

    outside_group->linked_sockets_.clear();
    outside_group->linked_group_inputs_.clear();
  }
}

BLI_NOINLINE void DerivedNodeTree::relink_group_outputs(const NodeTreeRef &group_ref,
                                                        Span<DNode *> nodes_by_id,
                                                        DNode &group_node)
{
  Span<const NodeRef *> node_refs = group_ref.nodes_by_type("NodeGroupOutput");
  if (node_refs.size() == 0) {
    return;
  }
  /* TODO: Pick correct group output node if there are more than one. */
  const NodeRef &output_node_ref = *node_refs[0];
  DNode &output_node = *nodes_by_id[output_node_ref.id()];

  int output_amount = group_node.outputs().size();
  BLI_assert(output_amount == output_node_ref.inputs().size() - 1);

  for (int output_index : IndexRange(output_amount)) {
    DOutputSocket *outside_group = group_node.outputs_[output_index];
    DInputSocket *inside_group = output_node.inputs_[output_index];

    for (DInputSocket *outside_connected : outside_group->linked_sockets_) {
      outside_connected->linked_sockets_.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DOutputSocket *inside_connected : inside_group->linked_sockets_) {
      inside_connected->linked_sockets_.remove_first_occurrence_and_reorder(inside_group);

      for (DInputSocket *outside_connected : outside_group->linked_sockets_) {
        inside_connected->linked_sockets_.append(outside_connected);
        outside_connected->linked_sockets_.append(inside_connected);
      }
    }

    for (DGroupInput *inside_connected : inside_group->linked_group_inputs_) {
      inside_connected->linked_sockets_.remove_first_occurrence_and_reorder(inside_group);

      for (DInputSocket *outside_connected : outside_group->linked_sockets_) {
        inside_connected->linked_sockets_.append(outside_connected);
        outside_connected->linked_group_inputs_.append(inside_connected);
      }
    }

    outside_group->linked_sockets_.clear();
    inside_group->linked_sockets_.clear();
  }
}

BLI_NOINLINE void DerivedNodeTree::remove_expanded_group_interfaces(Vector<DNode *> &all_nodes)
{
  int index = 0;
  while (index < all_nodes.size()) {
    DNode &node = *all_nodes[index];
    const NodeRef &node_ref = *node.node_ref_;
    if (node_ref.is_group_node() ||
        (node.parent_ != nullptr &&
         (node_ref.is_group_input_node() || node_ref.is_group_output_node()))) {
      all_nodes.remove_and_reorder(index);
      node.destruct_with_sockets();
    }
    else {
      index++;
    }
  }
}

BLI_NOINLINE void DerivedNodeTree::remove_unused_group_inputs(
    Vector<DGroupInput *> &all_group_inputs)
{
  int index = 0;
  while (index < all_group_inputs.size()) {
    DGroupInput &group_input = *all_group_inputs[index];
    if (group_input.linked_sockets_.is_empty()) {
      all_group_inputs.remove_and_reorder(index);
      group_input.~DGroupInput();
    }
    else {
      index++;
    }
  }
}

BLI_NOINLINE void DerivedNodeTree::relink_and_remove_muted_nodes(Vector<DNode *> &all_nodes)
{
  int index = 0;
  while (index < all_nodes.size()) {
    DNode &node = *all_nodes[index];
    const NodeRef &node_ref = *node.node_ref_;
    if (node_ref.is_muted()) {
      this->relink_muted_node(node);
      all_nodes.remove_and_reorder(index);
      node.destruct_with_sockets();
    }
    else {
      index++;
    }
  }
}

BLI_NOINLINE void DerivedNodeTree::relink_muted_node(DNode &node)
{
  const bNode &bnode = *node.bnode();
  LISTBASE_FOREACH (const bNodeLink *, internal_link, &bnode.internal_links) {
    BLI_assert(internal_link->fromnode == &bnode);
    BLI_assert(internal_link->tonode == &bnode);
    bNodeSocket *input_bsocket = internal_link->fromsock;
    bNodeSocket *output_bsocket = internal_link->tosock;

    /* Find internally linked sockets. */
    DInputSocket *input_socket = nullptr;
    DOutputSocket *output_socket = nullptr;
    for (DInputSocket *socket : node.inputs_) {
      if (socket->bsocket() == input_bsocket) {
        input_socket = socket;
        break;
      }
    }
    for (DOutputSocket *socket : node.outputs_) {
      if (socket->bsocket() == output_bsocket) {
        output_socket = socket;
        break;
      }
    }
    BLI_assert(input_socket != nullptr);
    BLI_assert(output_socket != nullptr);

    /* Link sockets connected to the input to sockets that are connected to the internally linked
     * output. */
    for (DInputSocket *to_socket : output_socket->linked_sockets_) {
      for (DOutputSocket *from_socket : input_socket->linked_sockets_) {
        from_socket->linked_sockets_.append_non_duplicates(to_socket);
        to_socket->linked_sockets_.append_non_duplicates(from_socket);
      }
      for (DGroupInput *group_input : input_socket->linked_group_inputs_) {
        group_input->linked_sockets_.append_non_duplicates(to_socket);
        to_socket->linked_group_inputs_.append_non_duplicates(group_input);
      }
    }
  }

  /* Remove remaining links from muted node. */
  for (DInputSocket *to_socket : node.inputs_) {
    for (DOutputSocket *from_socket : to_socket->linked_sockets_) {
      from_socket->linked_sockets_.remove_first_occurrence_and_reorder(to_socket);
    }
    for (DGroupInput *from_group_input : to_socket->linked_group_inputs_) {
      from_group_input->linked_sockets_.remove_first_occurrence_and_reorder(to_socket);
    }
    to_socket->linked_sockets_.clear();
    to_socket->linked_group_inputs_.clear();
  }
  for (DOutputSocket *from_socket : node.outputs_) {
    for (DInputSocket *to_socket : from_socket->linked_sockets_) {
      to_socket->linked_sockets_.remove_first_occurrence_and_reorder(from_socket);
    }
    from_socket->linked_sockets_.clear();
  }
}

void DNode::destruct_with_sockets()
{
  for (DInputSocket *socket : inputs_) {
    socket->~DInputSocket();
  }
  for (DOutputSocket *socket : outputs_) {
    socket->~DOutputSocket();
  }
  this->~DNode();
}

BLI_NOINLINE void DerivedNodeTree::store_in_this_and_init_ids(
    Vector<DNode *> &&all_nodes,
    Vector<DGroupInput *> &&all_group_inputs,
    Vector<DParentNode *> &&all_parent_nodes)
{
  nodes_by_id_ = std::move(all_nodes);
  group_inputs_ = std::move(all_group_inputs);
  parent_nodes_ = std::move(all_parent_nodes);

  for (int node_index : nodes_by_id_.index_range()) {
    DNode *node = nodes_by_id_[node_index];
    node->id_ = node_index;

    const bNodeType *nodetype = node->node_ref_->bnode()->typeinfo;
    nodes_by_type_.add(nodetype, node);

    for (DInputSocket *socket : node->inputs_) {
      socket->id_ = sockets_by_id_.append_and_get_index(socket);
      input_sockets_.append(socket);
    }
    for (DOutputSocket *socket : node->outputs_) {
      socket->id_ = sockets_by_id_.append_and_get_index(socket);
      output_sockets_.append(socket);
    }
  }

  for (int i : group_inputs_.index_range()) {
    group_inputs_[i]->id_ = i;
  }
}

DerivedNodeTree::~DerivedNodeTree()
{
  for (DInputSocket *socket : input_sockets_) {
    socket->~DInputSocket();
  }
  for (DOutputSocket *socket : output_sockets_) {
    socket->~DOutputSocket();
  }
  for (DNode *node : nodes_by_id_) {
    node->~DNode();
  }
  for (DGroupInput *group_input : group_inputs_) {
    group_input->~DGroupInput();
  }
  for (DParentNode *parent : parent_nodes_) {
    parent->~DParentNode();
  }
}

bool DerivedNodeTree::has_link_cycles() const
{
  for (const NodeTreeRef *tree : used_node_tree_refs_) {
    if (tree->has_link_cycles()) {
      return true;
    }
  }
  return false;
}

static dot::Cluster *get_cluster_for_parent(dot::DirectedGraph &graph,
                                            Map<const DParentNode *, dot::Cluster *> &clusters,
                                            const DParentNode *parent)
{
  if (parent == nullptr) {
    return nullptr;
  }
  return clusters.lookup_or_add_cb(parent, [&]() {
    dot::Cluster *parent_cluster = get_cluster_for_parent(graph, clusters, parent->parent());
    bNodeTree *btree = reinterpret_cast<bNodeTree *>(parent->node_ref().bnode()->id);
    dot::Cluster *new_cluster = &graph.new_cluster(parent->node_ref().name() + " / " +
                                                   StringRef(btree->id.name + 2));
    new_cluster->set_parent_cluster(parent_cluster);
    return new_cluster;
  });
}

std::string DerivedNodeTree::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const DNode *, dot::NodeWithSocketsRef> dot_nodes;
  Map<const DGroupInput *, dot::NodeWithSocketsRef> dot_group_inputs;
  Map<const DParentNode *, dot::Cluster *> dot_clusters;

  for (const DNode *node : nodes_by_id_) {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_background_color("white");

    Vector<std::string> input_names;
    for (const DInputSocket *socket : node->inputs()) {
      input_names.append(socket->name());
    }
    Vector<std::string> output_names;
    for (const DOutputSocket *socket : node->outputs()) {
      output_names.append(socket->name());
    }

    dot_nodes.add_new(node,
                      dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));

    dot::Cluster *cluster = get_cluster_for_parent(digraph, dot_clusters, node->parent());
    dot_node.set_parent_cluster(cluster);
  }

  for (const DGroupInput *group_input : group_inputs_) {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_background_color("white");

    std::string group_input_name = group_input->name();
    dot_group_inputs.add_new(
        group_input, dot::NodeWithSocketsRef(dot_node, "Group Input", {}, {group_input_name}));

    dot::Cluster *cluster = get_cluster_for_parent(digraph, dot_clusters, group_input->parent());
    dot_node.set_parent_cluster(cluster);
  }

  for (const DNode *to_node : nodes_by_id_) {
    dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(to_node);

    for (const DInputSocket *to_socket : to_node->inputs()) {
      for (const DOutputSocket *from_socket : to_socket->linked_sockets()) {
        const DNode *from_node = &from_socket->node();
        dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(from_node);

        digraph.new_edge(from_dot_node.output(from_socket->index()),
                         to_dot_node.input(to_socket->index()));
      }
      for (const DGroupInput *group_input : to_socket->linked_group_inputs()) {
        dot::NodeWithSocketsRef &from_dot_node = dot_group_inputs.lookup(group_input);

        digraph.new_edge(from_dot_node.output(0), to_dot_node.input(to_socket->index()));
      }
    }
  }

  digraph.set_random_cluster_bgcolors();
  return digraph.to_dot_string();
}

}  // namespace blender::nodes
