/* SPDX-FileCopyrightText: 2011-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_USD
#  include <pxr/base/tf/stringUtils.h>
#endif

#include "BLI_hash.hh"
#include "BLI_string_utils.hh"

#include "BKE_node_runtime.hh"

#include "node_graph.h"

namespace blender::nodes::materialx {

/* Prefix for nodes that don't map directly to a Blender shader node. */
static const char *ANONYMOUS_NODE_NAME_PREFIX = "node";

/* Valid name for MaterialX and USD. */
static std::string valid_name(const StringRef name)
{
#ifdef WITH_USD
  /* Node name should suite to MatX and USD valid names.
   * It shouldn't start from '_', due to error occurred in Storm delegate. */
  std::string res = MaterialX::createValidName(pxr::TfMakeValidIdentifier(name));
#else
  std::string res = MaterialX::createValidName(name);
#endif
  if (res[0] == '_') {
    res = "node" + res;
  }
  return res;
}

/* Node Key */

uint64_t NodeGraph::NodeKey::hash() const
{
  return get_default_hash(node, socket_name, to_type, graph_element);
}

bool NodeGraph::NodeKey::operator==(const NodeGraph::NodeKey &other) const
{
  return node == other.node && socket_name == other.socket_name && to_type == other.to_type &&
         graph_element == other.graph_element;
}

/* Node Graph */

NodeGraph::NodeGraph(const Depsgraph *depsgraph,
                     const Material *material,
                     const ExportParams &export_params,
                     const MaterialX::DocumentPtr &document)
    : depsgraph(depsgraph),
      material(material),
      export_params(export_params),
      graph_element_(document.get()),
      key_to_name_map_(root_key_to_name_map_)
{
}

NodeGraph::NodeGraph(const NodeGraph &parent, const StringRef child_name)
    : depsgraph(parent.depsgraph),
      material(parent.material),
      export_params(parent.export_params),
#ifdef USE_MATERIALX_NODEGRAPH
      key_to_name_map_(root_key_to_name_map_)
#else
      key_to_name_map_(parent.key_to_name_map_)
#endif
{
  std::string valid_child_name = valid_name(child_name);

#ifdef USE_MATERIALX_NODEGRAPH
  MaterialX::NodeGraphPtr graph = parent.graph_element_->getChildOfType<MaterialX::NodeGraph>(
      valid_child_name);
  if (!graph) {
    CLOG_DEBUG(LOG_IO_MATERIALX, "<nodegraph name=%s>", valid_child_name.c_str());
    graph = parent.graph_element_->addChild<MaterialX::NodeGraph>(valid_child_name);
  }
  graph_element_ = graph.get();
#else
  graph_element_ = parent.graph_element_;
  node_name_prefix_ = node_name_prefix_ + valid_child_name + "_";
#endif
}

NodeItem NodeGraph::empty_node() const
{
  return NodeItem(graph_element_);
}

NodeItem NodeGraph::get_node(const StringRef name) const
{
  NodeItem item = empty_node();
  item.node = graph_element_->getNode(name);
  return item;
}

NodeItem NodeGraph::get_output(const StringRef name) const
{
  NodeItem item = empty_node();
  item.output = graph_element_->getOutput(name);
  return item;
}

NodeItem NodeGraph::get_input(const StringRef name) const
{
  NodeItem item = empty_node();
  item.input = graph_element_->getInput(name);
  return item;
}

std::string NodeGraph::unique_node_name(const bNode *node,
                                        const StringRef socket_out_name,
                                        NodeItem::Type to_type)
{
  /* Reuse existing name, important in case it got changed due to conflicts. */
  NodeKey key{node, socket_out_name, to_type, graph_element_};
  const std::string *existing_name = key_to_name_map_.lookup_ptr(key);
  if (existing_name) {
    return *existing_name;
  }

  /* Generate name based on node, socket, to type and node groups. */
  std::string name = node->name;

  if (!socket_out_name.is_empty() && node->output_sockets().size() > 1) {
    name += std::string("_") + socket_out_name;
  }
  if (to_type != NodeItem::Type::Empty) {
    name += "_" + NodeItem::type(to_type);
  }

  name = node_name_prefix_ + valid_name(name);

  /* Avoid conflicts with anonymous node names. */
  if (StringRef(name).startswith(ANONYMOUS_NODE_NAME_PREFIX)) {
    name = "b" + name;
  }

  /* Ensure the name does not conflict with other nodes in the graph, which may happen when
   * another Blender node name happens to match the complete name here. Can not just check
   * the graph because the node with this name might not get added to it immediately. */
  name = BLI_uniquename_cb(
      [this](const StringRef check_name) {
        return check_name == export_params.output_node_name ||
               graph_element_->getNode(check_name) != nullptr ||
               used_node_names_.contains(check_name);
      },
      '_',
      name);

  used_node_names_.add(name);
  key_to_name_map_.add_new(key, name);
  return name;
}

void NodeGraph::set_output_node_name(const NodeItem &item) const
{
  if (item.node) {
    item.node->setName(export_params.output_node_name);
  }
}

std::string NodeGraph::unique_anonymous_node_name(MaterialX::GraphElement *graph_element)
{
  return BLI_uniquename_cb(
      [graph_element](const StringRef check_name) {
        return graph_element->getNode(check_name) != nullptr;
      },
      '_',
      ANONYMOUS_NODE_NAME_PREFIX);
}

}  // namespace blender::nodes::materialx
