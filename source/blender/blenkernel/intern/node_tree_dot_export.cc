/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_runtime.hh"
#include "BKE_node_tree_dot_export.hh"

#include "BLI_dot_export.hh"

namespace blender::bke {

std::string bNodeTreeToDotOptions::socket_name(const bNodeSocket &socket) const
{
  return socket.identifier;
}

std::optional<std::string> bNodeTreeToDotOptions::socket_font_color(
    const bNodeSocket &socket) const
{
  if (!socket.is_available()) {
    return "#999999";
  }
  return std::nullopt;
}

void bNodeTreeToDotOptions::add_edge_attributes(const bNodeLink &link,
                                                dot::DirectedEdge &dot_edge) const
{
  if (!link.is_used()) {
    dot_edge.attributes.set("color", "#999999");
  }
}

std::string node_tree_to_dot(const bNodeTree &tree, const bNodeTreeToDotOptions &options)
{
  tree.ensure_topology_cache();

  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const bNode *, dot::NodeWithSocketsRef> dot_nodes;

  for (const bNode *node : tree.all_nodes()) {
    dot::Node &dot_node = digraph.new_node("");
    dot::NodeWithSockets dot_node_with_sockets;
    dot_node_with_sockets.node_name = node->label_or_name();
    for (const bNodeSocket *socket : node->input_sockets()) {
      dot::NodeWithSockets::Input &dot_input = dot_node_with_sockets.add_input(
          options.socket_name(*socket));
      dot_input.fontcolor = options.socket_font_color(*socket);
    }
    for (const bNodeSocket *socket : node->output_sockets()) {
      dot::NodeWithSockets::Output &dot_output = dot_node_with_sockets.add_output(
          options.socket_name(*socket));
      dot_output.fontcolor = options.socket_font_color(*socket);
    }
    dot_nodes.add_new(node, dot::NodeWithSocketsRef(dot_node, dot_node_with_sockets));
  }

  LISTBASE_FOREACH (const bNodeLink *, link, &tree.links) {
    const dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(link->fromnode);
    const dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(link->tonode);
    const dot::NodePort from_dot_port = from_dot_node.output(link->fromsock->index());
    const dot::NodePort to_dot_port = to_dot_node.input(link->tosock->index());

    dot::DirectedEdge &dot_edge = digraph.new_edge(from_dot_port, to_dot_port);
    options.add_edge_attributes(*link, dot_edge);
  }

  return digraph.to_dot_string();
}

}  // namespace blender::bke
