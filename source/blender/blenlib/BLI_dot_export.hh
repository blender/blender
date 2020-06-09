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

#ifndef __BLI_DOT_EXPORT_HH__
#define __BLI_DOT_EXPORT_HH__

/**
 * Language grammar: https://www.graphviz.org/doc/info/lang.html
 * Attributes: https://www.graphviz.org/doc/info/attrs.html
 * Node Shapes: https://www.graphviz.org/doc/info/shapes.html
 * Preview: https://dreampuf.github.io/GraphvizOnline
 */

#include "BLI_map.hh"
#include "BLI_optional.hh"
#include "BLI_set.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "BLI_dot_export_attribute_enums.hh"

#include <sstream>

namespace blender {
namespace DotExport {

class Graph;
class DirectedGraph;
class UndirectedGraph;
class Node;
class NodePort;
class DirectedEdge;
class UndirectedEdge;
class Cluster;
class AttributeList;

class AttributeList {
 private:
  Map<std::string, std::string> m_attributes;

 public:
  void export__as_bracket_list(std::stringstream &ss) const;

  void set(StringRef key, StringRef value)
  {
    m_attributes.add_overwrite(key, value);
  }
};

class Graph {
 private:
  AttributeList m_attributes;
  Vector<std::unique_ptr<Node>> m_nodes;
  Vector<std::unique_ptr<Cluster>> m_clusters;

  Set<Node *> m_top_level_nodes;
  Set<Cluster *> m_top_level_clusters;

  friend Cluster;
  friend Node;

 public:
  Node &new_node(StringRef label);
  Cluster &new_cluster(StringRef label = "");

  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_rankdir(Attr_rankdir rankdir)
  {
    this->set_attribute("rankdir", rankdir_to_string(rankdir));
  }

  void set_random_cluster_bgcolors();
};

class Cluster {
 private:
  AttributeList m_attributes;
  Graph &m_graph;
  Cluster *m_parent = nullptr;
  Set<Cluster *> m_children;
  Set<Node *> m_nodes;

  friend Graph;
  friend Node;

  Cluster(Graph &graph) : m_graph(graph)
  {
  }

 public:
  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_parent_cluster(Cluster *cluster);
  void set_parent_cluster(Cluster &cluster)
  {
    this->set_parent_cluster(&cluster);
  }

  void set_random_cluster_bgcolors();
};

class Node {
 private:
  AttributeList m_attributes;
  Graph &m_graph;
  Cluster *m_cluster = nullptr;

  friend Graph;

  Node(Graph &graph) : m_graph(graph)
  {
  }

 public:
  const AttributeList &attributes() const
  {
    return m_attributes;
  }

  AttributeList &attributes()
  {
    return m_attributes;
  }

  void set_parent_cluster(Cluster *cluster);
  void set_parent_cluster(Cluster &cluster)
  {
    this->set_parent_cluster(&cluster);
  }

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_shape(Attr_shape shape)
  {
    this->set_attribute("shape", shape_to_string(shape));
  }

  /* See https://www.graphviz.org/doc/info/attrs.html#k:color. */
  void set_background_color(StringRef name)
  {
    this->set_attribute("fillcolor", name);
    this->set_attribute("style", "filled");
  }

  void export__as_id(std::stringstream &ss) const;

  void export__as_declaration(std::stringstream &ss) const;
};

class UndirectedGraph final : public Graph {
 private:
  Vector<std::unique_ptr<UndirectedEdge>> m_edges;

 public:
  std::string to_dot_string() const;

  UndirectedEdge &new_edge(NodePort a, NodePort b);
};

class DirectedGraph final : public Graph {
 private:
  Vector<std::unique_ptr<DirectedEdge>> m_edges;

 public:
  std::string to_dot_string() const;

  DirectedEdge &new_edge(NodePort from, NodePort to);
};

class NodePort {
 private:
  Node *m_node;
  Optional<std::string> m_port_name;

 public:
  NodePort(Node &node, Optional<std::string> port_name = {})
      : m_node(&node), m_port_name(std::move(port_name))
  {
  }

  void to_dot_string(std::stringstream &ss) const;
};

class Edge : blender::NonCopyable, blender::NonMovable {
 protected:
  AttributeList m_attributes;
  NodePort m_a;
  NodePort m_b;

 public:
  Edge(NodePort a, NodePort b) : m_a(std::move(a)), m_b(std::move(b))
  {
  }

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_arrowhead(Attr_arrowType type)
  {
    this->set_attribute("arrowhead", arrowType_to_string(type));
  }

  void set_arrowtail(Attr_arrowType type)
  {
    this->set_attribute("arrowtail", arrowType_to_string(type));
  }

  void set_dir(Attr_dirType type)
  {
    this->set_attribute("dir", dirType_to_string(type));
  }
};

class DirectedEdge : public Edge {
 public:
  DirectedEdge(NodePort from, NodePort to) : Edge(std::move(from), std::move(to))
  {
  }

  void export__as_edge_statement(std::stringstream &ss) const;
};

class UndirectedEdge : public Edge {
 public:
  UndirectedEdge(NodePort a, NodePort b) : Edge(std::move(a), std::move(b))
  {
  }

  void export__as_edge_statement(std::stringstream &ss) const;
};

std::string color_attr_from_hsv(float h, float s, float v);

class NodeWithSocketsRef {
 private:
  Node *m_node;

 public:
  NodeWithSocketsRef(Node &node,
                     StringRef name,
                     Span<std::string> input_names,
                     Span<std::string> output_names);

  NodePort input(uint index) const
  {
    std::string port = "\"in" + std::to_string(index) + "\"";
    return NodePort(*m_node, port);
  }

  NodePort output(uint index) const
  {
    std::string port = "\"out" + std::to_string(index) + "\"";
    return NodePort(*m_node, port);
  }
};

}  // namespace DotExport
}  // namespace blender

#endif /* __BLI_DOT_EXPORT_HH__ */
