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

#include "BKE_node_tree_ref.hh"

#include "BLI_dot_export.hh"

namespace BKE {

NodeTreeRef::NodeTreeRef(bNodeTree *btree) : m_btree(btree)
{
  Map<bNode *, NodeRef *> node_mapping;

  LISTBASE_FOREACH (bNode *, bnode, &btree->nodes) {
    NodeRef &node = *m_allocator.construct<NodeRef>();

    node.m_tree = this;
    node.m_bnode = bnode;
    node.m_id = m_nodes_by_id.append_and_get_index(&node);
    RNA_pointer_create(&btree->id, &RNA_Node, bnode, &node.m_rna);

    LISTBASE_FOREACH (bNodeSocket *, bsocket, &bnode->inputs) {
      InputSocketRef &socket = *m_allocator.construct<InputSocketRef>();
      socket.m_node = &node;
      socket.m_index = node.m_inputs.append_and_get_index(&socket);
      socket.m_is_input = true;
      socket.m_bsocket = bsocket;
      socket.m_id = m_sockets_by_id.append_and_get_index(&socket);
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &socket.m_rna);
    }

    LISTBASE_FOREACH (bNodeSocket *, bsocket, &bnode->outputs) {
      OutputSocketRef &socket = *m_allocator.construct<OutputSocketRef>();
      socket.m_node = &node;
      socket.m_index = node.m_outputs.append_and_get_index(&socket);
      socket.m_is_input = false;
      socket.m_bsocket = bsocket;
      socket.m_id = m_sockets_by_id.append_and_get_index(&socket);
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &socket.m_rna);
    }

    m_input_sockets.extend(node.m_inputs);
    m_output_sockets.extend(node.m_outputs);

    node_mapping.add_new(bnode, &node);
  }

  LISTBASE_FOREACH (bNodeLink *, blink, &btree->links) {
    OutputSocketRef &from_socket = this->find_output_socket(
        node_mapping, blink->fromnode, blink->fromsock);
    InputSocketRef &to_socket = this->find_input_socket(
        node_mapping, blink->tonode, blink->tosock);

    from_socket.m_directly_linked_sockets.append(&to_socket);
    to_socket.m_directly_linked_sockets.append(&from_socket);
  }

  for (OutputSocketRef *socket : m_output_sockets) {
    if (!socket->m_node->is_reroute_node()) {
      this->find_targets_skipping_reroutes(*socket, socket->m_linked_sockets);
      for (SocketRef *target : socket->m_linked_sockets) {
        target->m_linked_sockets.append(socket);
      }
    }
  }

  for (NodeRef *node : m_nodes_by_id) {
    m_nodes_by_idname.lookup_or_add_default(node->idname()).append(node);
  }
}

NodeTreeRef::~NodeTreeRef()
{
  for (NodeRef *node : m_nodes_by_id) {
    node->~NodeRef();
  }
  for (InputSocketRef *socket : m_input_sockets) {
    socket->~InputSocketRef();
  }
  for (OutputSocketRef *socket : m_output_sockets) {
    socket->~OutputSocketRef();
  }
}

InputSocketRef &NodeTreeRef::find_input_socket(Map<bNode *, NodeRef *> &node_mapping,
                                               bNode *bnode,
                                               bNodeSocket *bsocket)
{
  NodeRef *node = node_mapping.lookup(bnode);
  for (SocketRef *socket : node->m_inputs) {
    if (socket->m_bsocket == bsocket) {
      return *(InputSocketRef *)socket;
    }
  }
  BLI_assert(false);
  return *node->m_inputs[0];
}

OutputSocketRef &NodeTreeRef::find_output_socket(Map<bNode *, NodeRef *> &node_mapping,
                                                 bNode *bnode,
                                                 bNodeSocket *bsocket)
{
  NodeRef *node = node_mapping.lookup(bnode);
  for (SocketRef *socket : node->m_outputs) {
    if (socket->m_bsocket == bsocket) {
      return *(OutputSocketRef *)socket;
    }
  }
  BLI_assert(false);
  return *node->m_outputs[0];
}

void NodeTreeRef::find_targets_skipping_reroutes(OutputSocketRef &socket,
                                                 Vector<SocketRef *> &r_targets)
{
  for (SocketRef *direct_target : socket.m_directly_linked_sockets) {
    if (direct_target->m_node->is_reroute_node()) {
      this->find_targets_skipping_reroutes(*direct_target->m_node->m_outputs[0], r_targets);
    }
    else {
      r_targets.append_non_duplicates(direct_target);
    }
  }
}

std::string NodeTreeRef::to_dot() const
{
  namespace Dot = blender::DotExport;

  Dot::DirectedGraph digraph;
  digraph.set_rankdir(Dot::Attr_rankdir::LeftToRight);

  Map<const NodeRef *, Dot::NodeWithSocketsRef> dot_nodes;

  for (const NodeRef *node : m_nodes_by_id) {
    Dot::Node &dot_node = digraph.new_node("");
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
                      Dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }

  for (const OutputSocketRef *from_socket : m_output_sockets) {
    for (const InputSocketRef *to_socket : from_socket->directly_linked_sockets()) {
      Dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(&from_socket->node());
      Dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(&to_socket->node());

      digraph.new_edge(from_dot_node.output(from_socket->index()),
                       to_dot_node.input(to_socket->index()));
    }
  }

  return digraph.to_dot_string();
}

}  // namespace BKE
