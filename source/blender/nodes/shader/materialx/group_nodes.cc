/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "group_nodes.h"
#include "node_parser.h"

#include "BKE_node_runtime.hh"

#ifdef USE_MATERIALX_NODEGRAPH
#  include "BLI_vector.hh"
#endif

namespace blender::nodes::materialx {

GroupNodeParser::GroupNodeParser(NodeGraph &graph,
                                 const bNode *node,
                                 const bNodeSocket *socket_out,
                                 NodeItem::Type to_type,
                                 GroupNodeParser *group_parser,
                                 bool use_group_default)
    : NodeParser(graph, node, socket_out, to_type, group_parser),
      use_group_default_(use_group_default)
{
}

NodeItem GroupNodeParser::compute()
{
  const bNodeTree *ngroup = reinterpret_cast<const bNodeTree *>(node_->id);
  ngroup->ensure_topology_cache();
  const bNode *node_out = ngroup->group_output_node();
  if (!node_out) {
    return empty();
  }

  NodeGraph group_graph(graph_, ngroup->id.name + 2);

  NodeItem out = GroupOutputNodeParser(
                     group_graph, node_out, socket_out_, to_type_, this, use_group_default_)
                     .compute_full();

#ifdef USE_MATERIALX_NODEGRAPH
  /* We have to be in NodeParser's graph_, therefore copying output */
  NodeItem res = empty();
  res.output = out.output;
  return res;
#else
  return out;
#endif
}

NodeItem GroupNodeParser::compute_full()
{
  NodeItem res = compute();
  if (NodeItem::is_arithmetic(to_type_)) {
    res = res.convert(to_type_);
  }
  return res;
}

NodeItem GroupOutputNodeParser::compute()
{
#ifdef USE_MATERIALX_NODEGRAPH
  Vector<NodeItem> values;
  for (const auto *socket_in : node_->input_sockets()) {
    NodeItem value = get_input_value(
        socket_in->index(), NodeItem::is_arithmetic(to_type_) ? NodeItem::Type::Any : to_type_);
    if (value.value) {
      value = create_node("constant", value.type(), {{"value", value}});
    }
    values.append(value);
  }
  Vector<NodeItem> outputs;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i]) {
      outputs.append(create_output(out_name(node_->input_sockets()[i]), values[i]));
    }
  }
  return outputs[socket_out_->index()];
#else
  if (use_group_default_) {
    return get_input_value(socket_out_->index(), to_type_);
  }
  return get_input_link(socket_out_->index(), to_type_);
#endif
}

NodeItem GroupOutputNodeParser::compute_full()
{
  CLOG_DEBUG(LOG_IO_MATERIALX,
             "%s [%d] => %s",
             node_->name,
             node_->typeinfo->type_legacy,
             NodeItem::type(to_type_).c_str());

#ifdef USE_MATERIALX_NODEGRAPH
  /* Checking if output was already computed */
  NodeItem res = graph_.get_output(out_name(socket_out_));
  if (res.output) {
    return res;
  }

  res = compute();
  return res;
#else
  return compute();
#endif
}

std::string GroupOutputNodeParser::out_name(const bNodeSocket *out_socket)
{
  return MaterialX::createValidName(std::string("out_") + out_socket->identifier);
}

NodeItem GroupInputNodeParser::compute()
{
#ifdef USE_MATERIALX_NODEGRAPH
  NodeItem value = group_parser_->get_input_link(socket_out_->index(), to_type_);
  if (!value) {
    return empty();
  }

  if (value.value) {
    value = group_parser_->create_node("constant", value.type(), {{"value", value}});
  }
  return create_input(in_name(), value);
#else
  if (use_group_default_) {
    return group_parser_->get_input_value(socket_out_->index(), to_type_);
  }
  return group_parser_->get_input_link(socket_out_->index(), to_type_);
#endif
}

NodeItem GroupInputNodeParser::compute_full()
{
  CLOG_INFO(LOG_IO_MATERIALX,
            "%s [%d] => %s",
            node_->name,
            node_->typeinfo->type_legacy,
            NodeItem::type(to_type_).c_str());

#ifdef USE_MATERIALX_NODEGRAPH
  /* Checking if input was already computed */
  NodeItem res = graph_.get_input(in_name());
  if (res.input) {
    return res;
  }

  res = compute();
  return res;
#else
  return compute();
#endif
}

std::string GroupInputNodeParser::in_name() const
{
  return MaterialX::createValidName(std::string("in_") + socket_out_->identifier);
}

}  // namespace blender::nodes::materialx
