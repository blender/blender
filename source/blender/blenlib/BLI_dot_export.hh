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

#pragma once

/**
 * Language grammar: https://www.graphviz.org/doc/info/lang.html
 * Attributes: https://www.graphviz.org/doc/info/attrs.html
 * Node Shapes: https://www.graphviz.org/doc/info/shapes.html
 * Preview: https://dreampuf.github.io/GraphvizOnline
 */

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "BLI_dot_export_attribute_enums.hh"

#include <optional>
#include <sstream>

namespace blender::dot {

class Graph;
class DirectedGraph;
class UndirectedGraph;
class Node;
class NodePort;
class DirectedEdge;
class UndirectedEdge;
class Cluster;

class Attributes {
 private:
  Map<std::string, std::string> attributes_;

 public:
  void export__as_bracket_list(std::stringstream &ss) const;

  void set(StringRef key, StringRef value)
  {
    attributes_.add_overwrite(key, value);
  }

  void set(StringRef key, float value)
  {
    attributes_.add_overwrite(key, std::to_string(value));
  }
};

class Graph {
 private:
  Vector<std::unique_ptr<Node>> nodes_;
  Vector<std::unique_ptr<Cluster>> clusters_;

  Set<Node *> top_level_nodes_;
  Set<Cluster *> top_level_clusters_;

  friend Cluster;
  friend Node;

 public:
  Attributes attributes;

 public:
  Node &new_node(StringRef label);
  Cluster &new_cluster(StringRef label = "");

  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  void set_rankdir(Attr_rankdir rankdir)
  {
    attributes.set("rankdir", rankdir_to_string(rankdir));
  }

  void set_random_cluster_bgcolors();
};

class Cluster {
 private:
  Graph &graph_;
  Cluster *parent_ = nullptr;
  Set<Cluster *> children_;
  Set<Node *> nodes_;

  friend Graph;
  friend Node;

 public:
  Attributes attributes;

  Cluster(Graph &graph) : graph_(graph)
  {
  }

 public:
  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  std::string name() const
  {
    return "cluster_" + std::to_string((uintptr_t)this);
  }

  void set_parent_cluster(Cluster *new_parent);
  void set_parent_cluster(Cluster &cluster)
  {
    this->set_parent_cluster(&cluster);
  }

  Cluster *parent_cluster()
  {
    return parent_;
  }

  void set_random_cluster_bgcolors();

  bool contains(Node &node) const;
};

class Node {
 private:
  Graph &graph_;
  Cluster *cluster_ = nullptr;

  friend Graph;

 public:
  Attributes attributes;

  Node(Graph &graph) : graph_(graph)
  {
  }

 public:
  void set_parent_cluster(Cluster *cluster);
  void set_parent_cluster(Cluster &cluster)
  {
    this->set_parent_cluster(&cluster);
  }

  Cluster *parent_cluster()
  {
    return cluster_;
  }

  void set_shape(Attr_shape shape)
  {
    attributes.set("shape", shape_to_string(shape));
  }

  /* See https://www.graphviz.org/doc/info/attrs.html#k:color. */
  void set_background_color(StringRef name)
  {
    attributes.set("fillcolor", name);
    attributes.set("style", "filled");
  }

  void export__as_id(std::stringstream &ss) const;

  void export__as_declaration(std::stringstream &ss) const;
};

class UndirectedGraph final : public Graph {
 private:
  Vector<std::unique_ptr<UndirectedEdge>> edges_;

 public:
  std::string to_dot_string() const;

  UndirectedEdge &new_edge(NodePort a, NodePort b);
};

class DirectedGraph final : public Graph {
 private:
  Vector<std::unique_ptr<DirectedEdge>> edges_;

 public:
  std::string to_dot_string() const;

  DirectedEdge &new_edge(NodePort from, NodePort to);
};

class NodePort {
 private:
  Node *node_;
  std::optional<std::string> port_name_;

 public:
  NodePort(Node &node, std::optional<std::string> port_name = {})
      : node_(&node), port_name_(std::move(port_name))
  {
  }

  void to_dot_string(std::stringstream &ss) const;
};

class Edge : blender::NonCopyable, blender::NonMovable {
 protected:
  NodePort a_;
  NodePort b_;

 public:
  Attributes attributes;

 public:
  Edge(NodePort a, NodePort b) : a_(std::move(a)), b_(std::move(b))
  {
  }

  void set_arrowhead(Attr_arrowType type)
  {
    attributes.set("arrowhead", arrowType_to_string(type));
  }

  void set_arrowtail(Attr_arrowType type)
  {
    attributes.set("arrowtail", arrowType_to_string(type));
  }

  void set_dir(Attr_dirType type)
  {
    attributes.set("dir", dirType_to_string(type));
  }

  void set_label(StringRef label)
  {
    attributes.set("label", label);
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
  Node *node_;

 public:
  NodeWithSocketsRef(Node &node,
                     StringRef name,
                     Span<std::string> input_names,
                     Span<std::string> output_names);

  Node &node()
  {
    return *node_;
  }

  NodePort input(int index) const
  {
    std::string port = "\"in" + std::to_string(index) + "\"";
    return NodePort(*node_, port);
  }

  NodePort output(int index) const
  {
    std::string port = "\"out" + std::to_string(index) + "\"";
    return NodePort(*node_, port);
  }
};

}  // namespace blender::dot
