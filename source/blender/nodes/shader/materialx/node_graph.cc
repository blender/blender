/* SPDX-FileCopyrightText: 2011-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_USD
#  include <pxr/base/tf/stringUtils.h>
#endif

#include "node_graph.h"

namespace blender::nodes::materialx {

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

/* Node Graph */

NodeGraph::NodeGraph(const Depsgraph *depsgraph,
                     const Material *material,
                     const ExportParams &export_params,
                     const MaterialX::DocumentPtr &document)
    : depsgraph(depsgraph),
      material(material),
      export_params(export_params),
      graph_element_(document.get())
{
}

NodeGraph::NodeGraph(const NodeGraph &parent, const StringRef child_name)
    : depsgraph(parent.depsgraph), material(parent.material), export_params(parent.export_params)
{
#ifdef USE_MATERIALX_NODEGRAPH
  MaterialX::NodeGraphPtr graph = parent.graph_element_->getChildOfType<MaterialX::NodeGraph>(
      child_name);
  if (!graph) {
    CLOG_INFO(LOG_MATERIALX_SHADER, 1, "<nodegraph name=%s>", child_name.c_str());
    graph = parent.graph_element_->addChild<MaterialX::NodeGraph>(child_name);
  }
  graph_element_ = graph.get();
#else
  graph_element_ = parent.graph_element_;
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

}  // namespace blender::nodes::materialx
