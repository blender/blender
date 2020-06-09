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

#include "BKE_derived_node_tree.hh"

#include "BLI_dot_export.hh"

#define UNINITIALIZED_ID UINT32_MAX

namespace BKE {

static const NodeTreeRef &get_tree_ref(NodeTreeRefMap &node_tree_refs, bNodeTree *btree)
{
  return *node_tree_refs.lookup_or_add(btree,
                                       [&]() { return blender::make_unique<NodeTreeRef>(btree); });
}

DerivedNodeTree::DerivedNodeTree(bNodeTree *btree, NodeTreeRefMap &node_tree_refs) : m_btree(btree)
{
  const NodeTreeRef &main_tree_ref = get_tree_ref(node_tree_refs, btree);

  Vector<DNode *> all_nodes;
  Vector<DGroupInput *> all_group_inputs;
  Vector<DParentNode *> all_parent_nodes;

  this->insert_nodes_and_links_in_id_order(main_tree_ref, nullptr, all_nodes);
  this->expand_groups(all_nodes, all_group_inputs, all_parent_nodes, node_tree_refs);
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
      DInputSocket *to_socket = (DInputSocket *)sockets_map[to_socket_ref->id()];
      for (const OutputSocketRef *from_socket_ref : to_socket_ref->linked_sockets()) {
        DOutputSocket *from_socket = (DOutputSocket *)sockets_map[from_socket_ref->id()];
        to_socket->m_linked_sockets.append(from_socket);
        from_socket->m_linked_sockets.append(to_socket);
      }
    }
  }
}

DNode &DerivedNodeTree::create_node(const NodeRef &node_ref,
                                    DParentNode *parent,
                                    MutableSpan<DSocket *> r_sockets_map)
{
  DNode &node = *m_allocator.construct<DNode>();
  node.m_node_ref = &node_ref;
  node.m_parent = parent;
  node.m_id = UNINITIALIZED_ID;

  node.m_inputs = m_allocator.construct_elements_and_pointer_array<DInputSocket>(
      node_ref.inputs().size());
  node.m_outputs = m_allocator.construct_elements_and_pointer_array<DOutputSocket>(
      node_ref.outputs().size());

  for (uint i : node.m_inputs.index_range()) {
    const InputSocketRef &socket_ref = node_ref.input(i);
    DInputSocket &socket = *node.m_inputs[i];

    socket.m_id = UNINITIALIZED_ID;
    socket.m_node = &node;
    socket.m_socket_ref = &socket_ref;

    r_sockets_map[socket_ref.id()] = &socket;
  }

  for (uint i : node.m_outputs.index_range()) {
    const OutputSocketRef &socket_ref = node_ref.output(i);
    DOutputSocket &socket = *node.m_outputs[i];

    socket.m_id = UNINITIALIZED_ID;
    socket.m_node = &node;
    socket.m_socket_ref = &socket_ref;

    r_sockets_map[socket_ref.id()] = &socket;
  }

  return node;
}

BLI_NOINLINE void DerivedNodeTree::expand_groups(Vector<DNode *> &all_nodes,
                                                 Vector<DGroupInput *> &all_group_inputs,
                                                 Vector<DParentNode *> &all_parent_nodes,
                                                 NodeTreeRefMap &node_tree_refs)
{
  for (uint i = 0; i < all_nodes.size(); i++) {
    DNode &node = *all_nodes[i];
    if (node.m_node_ref->is_group_node()) {
      this->expand_group_node(node, all_nodes, all_group_inputs, all_parent_nodes, node_tree_refs);
    }
  }
}

BLI_NOINLINE void DerivedNodeTree::expand_group_node(DNode &group_node,
                                                     Vector<DNode *> &all_nodes,
                                                     Vector<DGroupInput *> &all_group_inputs,
                                                     Vector<DParentNode *> &all_parent_nodes,
                                                     NodeTreeRefMap &node_tree_refs)
{
  const NodeRef &group_node_ref = *group_node.m_node_ref;
  BLI_assert(group_node_ref.is_group_node());

  bNodeTree *btree = (bNodeTree *)group_node_ref.bnode()->id;
  if (btree == nullptr) {
    return;
  }

  const NodeTreeRef &group_ref = get_tree_ref(node_tree_refs, btree);

  DParentNode &parent = *m_allocator.construct<DParentNode>();
  parent.m_id = all_parent_nodes.append_and_get_index(&parent);
  parent.m_parent = group_node.m_parent;
  parent.m_node_ref = &group_node_ref;

  this->insert_nodes_and_links_in_id_order(group_ref, &parent, all_nodes);
  Span<DNode *> new_nodes_by_id = all_nodes.as_span().take_back(group_ref.nodes().size());

  this->create_group_inputs_for_unlinked_inputs(group_node, all_group_inputs);
  this->relink_group_inputs(group_ref, new_nodes_by_id, group_node);
  this->relink_group_outputs(group_ref, new_nodes_by_id, group_node);
}

BLI_NOINLINE void DerivedNodeTree::create_group_inputs_for_unlinked_inputs(
    DNode &node, Vector<DGroupInput *> &all_group_inputs)
{
  for (DInputSocket *input_socket : node.m_inputs) {
    if (input_socket->is_linked()) {
      continue;
    }

    DGroupInput &group_input = *m_allocator.construct<DGroupInput>();
    group_input.m_id = UNINITIALIZED_ID;
    group_input.m_socket_ref = &input_socket->socket_ref();
    group_input.m_parent = node.m_parent;

    group_input.m_linked_sockets.append(input_socket);
    input_socket->m_linked_group_inputs.append(&group_input);
    all_group_inputs.append(&group_input);
  }
}

BLI_NOINLINE void DerivedNodeTree::relink_group_inputs(const NodeTreeRef &group_ref,
                                                       Span<DNode *> nodes_by_id,
                                                       DNode &group_node)
{
  Span<const NodeRef *> node_refs = group_ref.nodes_with_idname("NodeGroupInput");
  if (node_refs.size() == 0) {
    return;
  }
  /* TODO: Pick correct group input node if there are more than one. */
  const NodeRef &input_node_ref = *node_refs[0];
  DNode &input_node = *nodes_by_id[input_node_ref.id()];

  uint input_amount = group_node.inputs().size();
  BLI_assert(input_amount == input_node_ref.outputs().size() - 1);

  for (uint input_index : IndexRange(input_amount)) {
    DInputSocket *outside_group = group_node.m_inputs[input_index];
    DOutputSocket *inside_group = input_node.m_outputs[input_index];

    for (DOutputSocket *outside_connected : outside_group->m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DGroupInput *outside_connected : outside_group->m_linked_group_inputs) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DInputSocket *inside_connected : inside_group->m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(inside_group);

      for (DOutputSocket *outside_connected : outside_group->m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }

      for (DGroupInput *outside_connected : outside_group->m_linked_group_inputs) {
        inside_connected->m_linked_group_inputs.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    inside_group->m_linked_sockets.clear();
    outside_group->m_linked_sockets.clear();
    outside_group->m_linked_group_inputs.clear();
  }
}

BLI_NOINLINE void DerivedNodeTree::relink_group_outputs(const NodeTreeRef &group_ref,
                                                        Span<DNode *> nodes_by_id,
                                                        DNode &group_node)
{
  Span<const NodeRef *> node_refs = group_ref.nodes_with_idname("NodeGroupOutput");
  if (node_refs.size() == 0) {
    return;
  }
  /* TODO: Pick correct group output node if there are more than one. */
  const NodeRef &output_node_ref = *node_refs[0];
  DNode &output_node = *nodes_by_id[output_node_ref.id()];

  uint output_amount = group_node.outputs().size();
  BLI_assert(output_amount == output_node_ref.inputs().size() - 1);

  for (uint output_index : IndexRange(output_amount)) {
    DOutputSocket *outside_group = group_node.m_outputs[output_index];
    DInputSocket *inside_group = output_node.m_inputs[output_index];

    for (DInputSocket *outside_connected : outside_group->m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DOutputSocket *inside_connected : inside_group->m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(inside_group);

      for (DInputSocket *outside_connected : outside_group->m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    for (DGroupInput *inside_connected : inside_group->m_linked_group_inputs) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(inside_group);

      for (DInputSocket *outside_connected : outside_group->m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_group_inputs.append(inside_connected);
      }
    }

    outside_group->m_linked_sockets.clear();
    inside_group->m_linked_sockets.clear();
  }
}

BLI_NOINLINE void DerivedNodeTree::remove_expanded_group_interfaces(Vector<DNode *> &all_nodes)
{
  int index = 0;
  while (index < all_nodes.size()) {
    DNode &node = *all_nodes[index];
    const NodeRef &node_ref = *node.m_node_ref;
    if (node_ref.is_group_node() ||
        (node.m_parent != nullptr &&
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
    if (group_input.m_linked_sockets.is_empty()) {
      all_group_inputs.remove_and_reorder(index);
      group_input.~DGroupInput();
    }
    else {
      index++;
    }
  }
}

void DNode::destruct_with_sockets()
{
  for (DInputSocket *socket : m_inputs) {
    socket->~DInputSocket();
  }
  for (DOutputSocket *socket : m_outputs) {
    socket->~DOutputSocket();
  }
  this->~DNode();
}

BLI_NOINLINE void DerivedNodeTree::store_in_this_and_init_ids(
    Vector<DNode *> &&all_nodes,
    Vector<DGroupInput *> &&all_group_inputs,
    Vector<DParentNode *> &&all_parent_nodes)
{
  m_nodes_by_id = std::move(all_nodes);
  m_group_inputs = std::move(all_group_inputs);
  m_parent_nodes = std::move(all_parent_nodes);

  for (uint node_index : m_nodes_by_id.index_range()) {
    DNode *node = m_nodes_by_id[node_index];
    node->m_id = node_index;

    m_nodes_by_idname.lookup_or_add_default(node->idname()).append(node);

    for (DInputSocket *socket : node->m_inputs) {
      socket->m_id = m_sockets_by_id.append_and_get_index(socket);
      m_input_sockets.append(socket);
    }
    for (DOutputSocket *socket : node->m_outputs) {
      socket->m_id = m_sockets_by_id.append_and_get_index(socket);
      m_output_sockets.append(socket);
    }
  }

  for (uint i : m_group_inputs.index_range()) {
    m_group_inputs[i]->m_id = i;
  }
}

DerivedNodeTree::~DerivedNodeTree()
{
  for (DInputSocket *socket : m_input_sockets) {
    socket->~DInputSocket();
  }
  for (DOutputSocket *socket : m_output_sockets) {
    socket->~DOutputSocket();
  }
  for (DNode *node : m_nodes_by_id) {
    node->~DNode();
  }
  for (DGroupInput *group_input : m_group_inputs) {
    group_input->~DGroupInput();
  }
  for (DParentNode *parent : m_parent_nodes) {
    parent->~DParentNode();
  }
}

namespace Dot = blender::DotExport;

static Dot::Cluster *get_cluster_for_parent(Dot::DirectedGraph &graph,
                                            Map<const DParentNode *, Dot::Cluster *> &clusters,
                                            const DParentNode *parent)
{
  if (parent == nullptr) {
    return nullptr;
  }
  return clusters.lookup_or_add(parent, [&]() {
    Dot::Cluster *parent_cluster = get_cluster_for_parent(graph, clusters, parent->parent());
    bNodeTree *btree = (bNodeTree *)parent->node_ref().bnode()->id;
    Dot::Cluster *new_cluster = &graph.new_cluster(parent->node_ref().name() + " / " +
                                                   StringRef(btree->id.name + 2));
    new_cluster->set_parent_cluster(parent_cluster);
    return new_cluster;
  });
}

std::string DerivedNodeTree::to_dot() const
{
  Dot::DirectedGraph digraph;
  digraph.set_rankdir(Dot::Attr_rankdir::LeftToRight);

  Map<const DNode *, Dot::NodeWithSocketsRef> dot_nodes;
  Map<const DGroupInput *, Dot::NodeWithSocketsRef> dot_group_inputs;
  Map<const DParentNode *, Dot::Cluster *> dot_clusters;

  for (const DNode *node : m_nodes_by_id) {
    Dot::Node &dot_node = digraph.new_node("");
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
                      Dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));

    Dot::Cluster *cluster = get_cluster_for_parent(digraph, dot_clusters, node->parent());
    dot_node.set_parent_cluster(cluster);
  }

  for (const DGroupInput *group_input : m_group_inputs) {
    Dot::Node &dot_node = digraph.new_node("");
    dot_node.set_background_color("white");

    std::string group_input_name = group_input->name();
    dot_group_inputs.add_new(
        group_input, Dot::NodeWithSocketsRef(dot_node, "Group Input", {}, {group_input_name}));

    Dot::Cluster *cluster = get_cluster_for_parent(digraph, dot_clusters, group_input->parent());
    dot_node.set_parent_cluster(cluster);
  }

  for (const DNode *to_node : m_nodes_by_id) {
    Dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(to_node);

    for (const DInputSocket *to_socket : to_node->inputs()) {
      for (const DOutputSocket *from_socket : to_socket->linked_sockets()) {
        const DNode *from_node = &from_socket->node();
        Dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(from_node);

        digraph.new_edge(from_dot_node.output(from_socket->index()),
                         to_dot_node.input(to_socket->index()));
      }
      for (const DGroupInput *group_input : to_socket->linked_group_inputs()) {
        Dot::NodeWithSocketsRef &from_dot_node = dot_group_inputs.lookup(group_input);

        digraph.new_edge(from_dot_node.output(0), to_dot_node.input(to_socket->index()));
      }
    }
  }

  digraph.set_random_cluster_bgcolors();
  return digraph.to_dot_string();
}

}  // namespace BKE
