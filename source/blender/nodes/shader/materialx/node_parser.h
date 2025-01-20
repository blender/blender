/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "material.h"
#include "node_graph.h"
#include "node_item.h"

#include "DNA_node_types.h"

#include "CLG_log.h"

namespace blender::nodes::materialx {

extern struct CLG_LogRef *LOG_MATERIALX_SHADER;

class GroupNodeParser;

/**
 * This is base abstraction class for parsing Blender nodes into MaterialX nodes.
 * #NodeParser::compute() should be overridden in child classes.
 */
class NodeParser {
 protected:
  NodeGraph &graph_;
  const bNode *node_;
  const bNodeSocket *socket_out_;
  NodeItem::Type to_type_;
  GroupNodeParser *group_parser_;

 public:
  NodeParser(NodeGraph &graph,
             const bNode *node,
             const bNodeSocket *socket_out,
             NodeItem::Type to_type,
             GroupNodeParser *group_parser);
  virtual ~NodeParser() = default;

  virtual NodeItem compute() = 0;
  virtual NodeItem compute_full();

 protected:
  std::string node_name(const char *override_output_name = nullptr) const;
  NodeItem create_node(const std::string &category, NodeItem::Type type);
  NodeItem create_node(const std::string &category,
                       NodeItem::Type type,
                       const NodeItem::Inputs &inputs);
  NodeItem create_input(const std::string &name, const NodeItem &item);
  NodeItem create_output(const std::string &name, const NodeItem &item);
  NodeItem get_input_default(const std::string &name, NodeItem::Type to_type);
  NodeItem get_input_default(int index, NodeItem::Type to_type);
  NodeItem get_output_default(const std::string &name, NodeItem::Type to_type);
  NodeItem get_output_default(int index, NodeItem::Type to_type);
  NodeItem get_input_link(const std::string &name, NodeItem::Type to_type);
  NodeItem get_input_link(int index, NodeItem::Type to_type);
  NodeItem get_input_value(const std::string &name, NodeItem::Type to_type);
  NodeItem get_input_value(int index, NodeItem::Type to_type);
  NodeItem empty() const;
  template<class T> NodeItem val(const T &data) const;
  NodeItem texcoord_node(NodeItem::Type type = NodeItem::Type::Vector2,
                         const std::string &attribute_name = "");

 private:
  NodeItem get_default(const bNodeSocket &socket, NodeItem::Type to_type);
  NodeItem get_input_link(const bNodeSocket &socket,
                          NodeItem::Type to_type,
                          bool use_group_default);
  NodeItem get_input_value(const bNodeSocket &socket, NodeItem::Type to_type);
};

template<class T> NodeItem NodeParser::val(const T &data) const
{
  return empty().val(data);
}

/**
 * Defines for including MaterialX node parsing code into node_shader_<name>.cc
 *
 * Example:
 * \code{.c}
 * NODE_SHADER_MATERIALX_BEGIN
 * #ifdef WITH_MATERIALX
 * {
 *   NodeItem color = get_input_value("Color", NodeItem::Type::Color4);
 *   NodeItem gamma = get_input_value("Gamma", NodeItem::Type::Float);
 *   return color ^ gamma;
 * }
 * #endif
 * NODE_SHADER_MATERIALX_END
 * \endcode
 */
struct NodeParserData {
  NodeGraph &graph;
  NodeItem::Type to_type;
  GroupNodeParser *group_parser;
  NodeItem result;
  ExportParams export_params;
};

#define NODE_SHADER_MATERIALX_BEGIN \
  class MaterialXNodeParser : public materialx::NodeParser { \
   public: \
    using materialx::NodeParser::NodeParser; \
    materialx::NodeItem compute() override; \
  }; \
\
  materialx::NodeItem MaterialXNodeParser::compute() \
  { \
    using NodeItem = materialx::NodeItem;

#define NODE_SHADER_MATERIALX_END \
  } \
\
  static void node_shader_materialx(void *data, struct bNode *node, struct bNodeSocket *out) \
  { \
    materialx::NodeParserData *d = reinterpret_cast<materialx::NodeParserData *>(data); \
    d->result = \
        MaterialXNodeParser(d->graph, node, out, d->to_type, d->group_parser).compute_full(); \
  }

}  // namespace blender::nodes::materialx
