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

#include <iomanip>

#include "BLI_dot_export.hh"

namespace blender {
namespace DotExport {

/* Graph Building
 ************************************************/

Node &Graph::new_node(StringRef label)
{
  Node *node = new Node(*this);
  m_nodes.append(std::unique_ptr<Node>(node));
  m_top_level_nodes.add_new(node);
  node->set_attribute("label", label);
  return *node;
}

Cluster &Graph::new_cluster(StringRef label)
{
  Cluster *cluster = new Cluster(*this);
  m_clusters.append(std::unique_ptr<Cluster>(cluster));
  m_top_level_clusters.add_new(cluster);
  cluster->set_attribute("label", label);
  return *cluster;
}

UndirectedEdge &UndirectedGraph::new_edge(NodePort a, NodePort b)
{
  UndirectedEdge *edge = new UndirectedEdge(a, b);
  m_edges.append(std::unique_ptr<UndirectedEdge>(edge));
  return *edge;
}

DirectedEdge &DirectedGraph::new_edge(NodePort from, NodePort to)
{
  DirectedEdge *edge = new DirectedEdge(from, to);
  m_edges.append(std::unique_ptr<DirectedEdge>(edge));
  return *edge;
}

void Cluster::set_parent_cluster(Cluster *new_parent)
{
  if (m_parent == new_parent) {
    return;
  }
  else if (m_parent == nullptr) {
    m_graph.m_top_level_clusters.remove(this);
    new_parent->m_children.add_new(this);
  }
  else if (new_parent == nullptr) {
    m_parent->m_children.remove(this);
    m_graph.m_top_level_clusters.add_new(this);
  }
  else {
    m_parent->m_children.remove(this);
    new_parent->m_children.add_new(this);
  }
  m_parent = new_parent;
}

void Node::set_parent_cluster(Cluster *cluster)
{
  if (m_cluster == cluster) {
    return;
  }
  else if (m_cluster == nullptr) {
    m_graph.m_top_level_nodes.remove(this);
    cluster->m_nodes.add_new(this);
  }
  else if (cluster == nullptr) {
    m_cluster->m_nodes.remove(this);
    m_graph.m_top_level_nodes.add_new(this);
  }
  else {
    m_cluster->m_nodes.remove(this);
    cluster->m_nodes.add_new(this);
  }
  m_cluster = cluster;
}

/* Utility methods
 **********************************************/

void Graph::set_random_cluster_bgcolors()
{
  for (Cluster *cluster : m_top_level_clusters) {
    cluster->set_random_cluster_bgcolors();
  }
}

void Cluster::set_random_cluster_bgcolors()
{
  float hue = rand() / (float)RAND_MAX;
  float staturation = 0.3f;
  float value = 0.8f;
  this->set_attribute("bgcolor", color_attr_from_hsv(hue, staturation, value));

  for (Cluster *cluster : m_children) {
    cluster->set_random_cluster_bgcolors();
  }
}

/* Dot Generation
 **********************************************/

std::string DirectedGraph::to_dot_string() const
{
  std::stringstream ss;
  ss << "digraph {\n";
  this->export__declare_nodes_and_clusters(ss);
  ss << "\n";

  for (const std::unique_ptr<DirectedEdge> &edge : m_edges) {
    edge->export__as_edge_statement(ss);
    ss << "\n";
  }

  ss << "}\n";
  return ss.str();
}

std::string UndirectedGraph::to_dot_string() const
{
  std::stringstream ss;
  ss << "graph {\n";
  this->export__declare_nodes_and_clusters(ss);
  ss << "\n";

  for (const std::unique_ptr<UndirectedEdge> &edge : m_edges) {
    edge->export__as_edge_statement(ss);
    ss << "\n";
  }

  ss << "}\n";
  return ss.str();
}

void Graph::export__declare_nodes_and_clusters(std::stringstream &ss) const
{
  ss << "graph ";
  m_attributes.export__as_bracket_list(ss);
  ss << "\n\n";

  for (Node *node : m_top_level_nodes) {
    node->export__as_declaration(ss);
  }

  for (Cluster *cluster : m_top_level_clusters) {
    cluster->export__declare_nodes_and_clusters(ss);
  }
}

void Cluster::export__declare_nodes_and_clusters(std::stringstream &ss) const
{
  ss << "subgraph cluster_" << (uintptr_t)this << " {\n";

  ss << "graph ";
  m_attributes.export__as_bracket_list(ss);
  ss << "\n\n";

  for (Node *node : m_nodes) {
    node->export__as_declaration(ss);
  }

  for (Cluster *cluster : m_children) {
    cluster->export__declare_nodes_and_clusters(ss);
  }

  ss << "}\n";
}

void DirectedEdge::export__as_edge_statement(std::stringstream &ss) const
{
  m_a.to_dot_string(ss);
  ss << " -> ";
  m_b.to_dot_string(ss);
  ss << " ";
  m_attributes.export__as_bracket_list(ss);
}

void UndirectedEdge::export__as_edge_statement(std::stringstream &ss) const
{
  m_a.to_dot_string(ss);
  ss << " -- ";
  m_b.to_dot_string(ss);
  ss << " ";
  m_attributes.export__as_bracket_list(ss);
}

void AttributeList::export__as_bracket_list(std::stringstream &ss) const
{
  ss << "[";
  m_attributes.foreach_item([&](StringRef key, StringRef value) {
    if (StringRef(value).startswith("<")) {
      /* Don't draw the quotes, this is an html-like value. */
      ss << key << "=" << value << ", ";
    }
    else {
      ss << key << "=\"" << value << "\", ";
    }
  });
  ss << "]";
}

void Node::export__as_id(std::stringstream &ss) const
{
  ss << '"' << (uintptr_t)this << '"';
}

void Node::export__as_declaration(std::stringstream &ss) const
{
  this->export__as_id(ss);
  ss << " ";
  m_attributes.export__as_bracket_list(ss);
  ss << "\n";
}

void NodePort::to_dot_string(std::stringstream &ss) const
{
  m_node->export__as_id(ss);
  if (m_port_name.has_value()) {
    ss << ":" << m_port_name.value();
  }
}

std::string color_attr_from_hsv(float h, float s, float v)
{
  std::stringstream ss;
  ss << std::setprecision(4) << h << ' ' << s << ' ' << v;
  return ss.str();
}

NodeWithSocketsRef::NodeWithSocketsRef(Node &node,
                                       StringRef name,
                                       Span<std::string> input_names,
                                       Span<std::string> output_names)
    : m_node(&node)
{
  std::stringstream ss;

  ss << "<<table border=\"0\" cellspacing=\"3\">";

  /* Header */
  ss << "<tr><td colspan=\"3\" align=\"center\"><b>";
  ss << ((name.size() == 0) ? "No Name" : name);
  ss << "</b></td></tr>";

  /* Sockets */
  uint socket_max_amount = std::max(input_names.size(), output_names.size());
  for (uint i = 0; i < socket_max_amount; i++) {
    ss << "<tr>";
    if (i < input_names.size()) {
      StringRef name = input_names[i];
      if (name.size() == 0) {
        name = "No Name";
      }
      ss << "<td align=\"left\" port=\"in" << i << "\">";
      ss << name;
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "<td></td>";
    if (i < output_names.size()) {
      StringRef name = output_names[i];
      if (name.size() == 0) {
        name = "No Name";
      }
      ss << "<td align=\"right\" port=\"out" << i << "\">";
      ss << name;
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "</tr>";
  }

  ss << "</table>>";

  m_node->set_attribute("label", ss.str());
  m_node->set_shape(Attr_shape::Rectangle);
}

}  // namespace DotExport
}  // namespace blender
