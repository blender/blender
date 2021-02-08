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

namespace blender::dot {

/* Graph Building
 ************************************************/

Node &Graph::new_node(StringRef label)
{
  Node *node = new Node(*this);
  nodes_.append(std::unique_ptr<Node>(node));
  top_level_nodes_.add_new(node);
  node->attributes.set("label", label);
  return *node;
}

Cluster &Graph::new_cluster(StringRef label)
{
  Cluster *cluster = new Cluster(*this);
  clusters_.append(std::unique_ptr<Cluster>(cluster));
  top_level_clusters_.add_new(cluster);
  cluster->attributes.set("label", label);
  return *cluster;
}

UndirectedEdge &UndirectedGraph::new_edge(NodePort a, NodePort b)
{
  UndirectedEdge *edge = new UndirectedEdge(a, b);
  edges_.append(std::unique_ptr<UndirectedEdge>(edge));
  return *edge;
}

DirectedEdge &DirectedGraph::new_edge(NodePort from, NodePort to)
{
  DirectedEdge *edge = new DirectedEdge(from, to);
  edges_.append(std::unique_ptr<DirectedEdge>(edge));
  return *edge;
}

void Cluster::set_parent_cluster(Cluster *new_parent)
{
  if (parent_ == new_parent) {
    return;
  }
  if (parent_ == nullptr) {
    graph_.top_level_clusters_.remove(this);
    new_parent->children_.add_new(this);
  }
  else if (new_parent == nullptr) {
    parent_->children_.remove(this);
    graph_.top_level_clusters_.add_new(this);
  }
  else {
    parent_->children_.remove(this);
    new_parent->children_.add_new(this);
  }
  parent_ = new_parent;
}

void Node::set_parent_cluster(Cluster *cluster)
{
  if (cluster_ == cluster) {
    return;
  }
  if (cluster_ == nullptr) {
    graph_.top_level_nodes_.remove(this);
    cluster->nodes_.add_new(this);
  }
  else if (cluster == nullptr) {
    cluster_->nodes_.remove(this);
    graph_.top_level_nodes_.add_new(this);
  }
  else {
    cluster_->nodes_.remove(this);
    cluster->nodes_.add_new(this);
  }
  cluster_ = cluster;
}

/* Utility methods
 **********************************************/

void Graph::set_random_cluster_bgcolors()
{
  for (Cluster *cluster : top_level_clusters_) {
    cluster->set_random_cluster_bgcolors();
  }
}

void Cluster::set_random_cluster_bgcolors()
{
  float hue = rand() / (float)RAND_MAX;
  float staturation = 0.3f;
  float value = 0.8f;
  this->attributes.set("bgcolor", color_attr_from_hsv(hue, staturation, value));

  for (Cluster *cluster : children_) {
    cluster->set_random_cluster_bgcolors();
  }
}

bool Cluster::contains(Node &node) const
{
  Cluster *current = node.parent_cluster();
  while (current != nullptr) {
    if (current == this) {
      return true;
    }
    current = current->parent_;
  }
  return false;
}

/* Dot Generation
 **********************************************/

std::string DirectedGraph::to_dot_string() const
{
  std::stringstream ss;
  ss << "digraph {\n";
  this->export__declare_nodes_and_clusters(ss);
  ss << "\n";

  for (const std::unique_ptr<DirectedEdge> &edge : edges_) {
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

  for (const std::unique_ptr<UndirectedEdge> &edge : edges_) {
    edge->export__as_edge_statement(ss);
    ss << "\n";
  }

  ss << "}\n";
  return ss.str();
}

void Graph::export__declare_nodes_and_clusters(std::stringstream &ss) const
{
  ss << "graph ";
  attributes.export__as_bracket_list(ss);
  ss << "\n\n";

  for (Node *node : top_level_nodes_) {
    node->export__as_declaration(ss);
  }

  for (Cluster *cluster : top_level_clusters_) {
    cluster->export__declare_nodes_and_clusters(ss);
  }
}

void Cluster::export__declare_nodes_and_clusters(std::stringstream &ss) const
{
  ss << "subgraph " << this->name() << " {\n";

  ss << "graph ";
  attributes.export__as_bracket_list(ss);
  ss << "\n\n";

  for (Node *node : nodes_) {
    node->export__as_declaration(ss);
  }

  for (Cluster *cluster : children_) {
    cluster->export__declare_nodes_and_clusters(ss);
  }

  ss << "}\n";
}

void DirectedEdge::export__as_edge_statement(std::stringstream &ss) const
{
  a_.to_dot_string(ss);
  ss << " -> ";
  b_.to_dot_string(ss);
  ss << " ";
  attributes.export__as_bracket_list(ss);
}

void UndirectedEdge::export__as_edge_statement(std::stringstream &ss) const
{
  a_.to_dot_string(ss);
  ss << " -- ";
  b_.to_dot_string(ss);
  ss << " ";
  attributes.export__as_bracket_list(ss);
}

void Attributes::export__as_bracket_list(std::stringstream &ss) const
{
  ss << "[";
  attributes_.foreach_item([&](StringRef key, StringRef value) {
    if (StringRef(value).startswith("<")) {
      /* Don't draw the quotes, this is an html-like value. */
      ss << key << "=" << value << ", ";
    }
    else {
      ss << key << "=\"";
      for (char c : value) {
        if (c == '\"') {
          /* Escape double quotes. */
          ss << '\\';
        }
        ss << c;
      }
      ss << "\", ";
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
  attributes.export__as_bracket_list(ss);
  ss << "\n";
}

void NodePort::to_dot_string(std::stringstream &ss) const
{
  node_->export__as_id(ss);
  if (port_name_.has_value()) {
    ss << ":" << *port_name_;
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
    : node_(&node)
{
  std::stringstream ss;

  ss << R"(<<table border="0" cellspacing="3">)";

  /* Header */
  ss << R"(<tr><td colspan="3" align="center"><b>)";
  ss << ((name.size() == 0) ? "No Name" : name);
  ss << "</b></td></tr>";

  /* Sockets */
  int socket_max_amount = std::max(input_names.size(), output_names.size());
  for (int i = 0; i < socket_max_amount; i++) {
    ss << "<tr>";
    if (i < input_names.size()) {
      StringRef name = input_names[i];
      if (name.size() == 0) {
        name = "No Name";
      }
      ss << R"(<td align="left" port="in)" << i << "\">";
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
      ss << R"(<td align="right" port="out)" << i << "\">";
      ss << name;
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "</tr>";
  }

  ss << "</table>>";

  node_->attributes.set("label", ss.str());
  node_->set_shape(Attr_shape::Rectangle);
}

}  // namespace blender::dot
