/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_USD
#  include <pxr/base/tf/stringUtils.h>
#endif

#include "node_parser.h"

#include "group_nodes.h"

#include "BKE_node_runtime.hh"

namespace blender::nodes::materialx {

static const std::string TEXCOORD_NODE_NAME = "node_texcoord";

CLG_LOGREF_DECLARE_GLOBAL(LOG_MATERIALX_SHADER, "materialx.shader");

NodeParser::NodeParser(MaterialX::GraphElement *graph,
                       const Depsgraph *depsgraph,
                       const Material *material,
                       const bNode *node,
                       const bNodeSocket *socket_out,
                       NodeItem::Type to_type,
                       GroupNodeParser *group_parser,
                       const ExportParams &export_params)
    : graph_(graph),
      depsgraph_(depsgraph),
      material_(material),
      node_(node),
      socket_out_(socket_out),
      to_type_(to_type),
      group_parser_(group_parser),
      export_params_(export_params)
{
}

NodeItem NodeParser::compute_full()
{
  NodeItem res = empty();

  /* Checking if node was already computed */
  res.node = graph_->getNode(node_name());
  if (!res.node) {
    CLOG_INFO(LOG_MATERIALX_SHADER,
              1,
              "%s [%d] => %s",
              node_->name,
              node_->typeinfo->type,
              NodeItem::type(to_type_).c_str());

    res = compute();
    if (res.node) {
      res.node->setName(node_name());
    }
  }
  if (NodeItem::is_arithmetic(to_type_)) {
    res = res.convert(to_type_);
  }
  return res;
}

std::string NodeParser::node_name(bool with_out_socket) const
{
  auto valid_name = [](const std::string &name) {
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
  };

  std::string name = node_->name;
  if (with_out_socket) {
    if (node_->output_sockets().size() > 1) {
      name += std::string("_") + socket_out_->name;
    }
    if (ELEM(to_type_, NodeItem::Type::BSDF, NodeItem::Type::EDF, NodeItem::Type::SurfaceOpacity))
    {
      name += "_" + NodeItem::type(to_type_);
    }
  }
#ifdef USE_MATERIALX_NODEGRAPH
  return valid_name(name);
#else

  std::string prefix;
  GroupNodeParser *gr = group_parser_;
  while (gr) {
    const bNodeTree *ngroup = reinterpret_cast<const bNodeTree *>(gr->node_->id);
    prefix = valid_name(ngroup->id.name) + "_" + prefix;
    gr = gr->group_parser_;
  }
  return prefix + valid_name(name);
#endif
}

NodeItem NodeParser::create_node(const std::string &category, NodeItem::Type type)
{
  return empty().create_node(category, type);
}

NodeItem NodeParser::create_node(const std::string &category,
                                 NodeItem::Type type,
                                 const NodeItem::Inputs &inputs)
{
  return empty().create_node(category, type, inputs);
}

NodeItem NodeParser::create_input(const std::string &name, const NodeItem &item)
{
  return empty().create_input(name, item);
}

NodeItem NodeParser::create_output(const std::string &name, const NodeItem &item)
{
  return empty().create_output(name, item);
}

NodeItem NodeParser::get_input_default(const std::string &name, NodeItem::Type to_type)
{
  return get_default(node_->input_by_identifier(name), to_type);
}

NodeItem NodeParser::get_input_default(int index, NodeItem::Type to_type)
{
  return get_default(node_->input_socket(index), to_type);
}

NodeItem NodeParser::get_input_link(const std::string &name, NodeItem::Type to_type)
{
  return get_input_link(node_->input_by_identifier(name), to_type, false);
}

NodeItem NodeParser::get_input_link(int index, NodeItem::Type to_type)
{
  return get_input_link(node_->input_socket(index), to_type, false);
}

NodeItem NodeParser::get_input_value(const std::string &name, NodeItem::Type to_type)
{
  return get_input_value(node_->input_by_identifier(name), to_type);
}

NodeItem NodeParser::get_input_value(int index, NodeItem::Type to_type)
{
  return get_input_value(node_->input_socket(index), to_type);
}

NodeItem NodeParser::get_output_default(const std::string &name, NodeItem::Type to_type)
{
  return get_default(node_->output_by_identifier(name), to_type);
}

NodeItem NodeParser::get_output_default(int index, NodeItem::Type to_type)
{
  return get_default(node_->output_socket(index), to_type);
}

NodeItem NodeParser::empty() const
{
  return NodeItem(graph_);
}

NodeItem NodeParser::texcoord_node(NodeItem::Type type, const std::string &attribute_name)
{
  BLI_assert(ELEM(type, NodeItem::Type::Vector2, NodeItem::Type::Vector3));
  std::string name = TEXCOORD_NODE_NAME;
  if (type == NodeItem::Type::Vector3) {
    name += "_vector3";
  }
  NodeItem res = empty();
  res.node = graph_->getNode(name);
  if (!res.node) {
    /* TODO: Use "Pref" generated texture coordinates for 3D, but needs
     * work in USD and Hydra mesh export. */
    const bool is_active_uvmap = attribute_name == "" ||
                                 attribute_name == export_params_.original_active_uvmap_name;
    if (export_params_.new_active_uvmap_name == "st" && is_active_uvmap) {
      res = create_node("texcoord", type);
    }
    else {
      const std::string &geomprop = (is_active_uvmap) ? export_params_.new_active_uvmap_name :
                                                        attribute_name;
      res = create_node("geompropvalue", type, {{"geomprop", val(geomprop)}});
    }
    res.node->setName(name);
  }
  return res;
}

NodeItem NodeParser::get_default(const bNodeSocket &socket, NodeItem::Type to_type)
{
  NodeItem res = empty();
  if (!NodeItem::is_arithmetic(to_type) && to_type != NodeItem::Type::Any) {
    return res;
  }

  switch (socket.type) {
    case SOCK_CUSTOM:
      /* Return empty */
      break;
    case SOCK_FLOAT: {
      float v = socket.default_value_typed<bNodeSocketValueFloat>()->value;
      res.value = MaterialX::Value::createValue<float>(v);
      break;
    }
    case SOCK_VECTOR: {
      const float *v = socket.default_value_typed<bNodeSocketValueVector>()->value;
      res.value = MaterialX::Value::createValue<MaterialX::Vector3>(
          MaterialX::Vector3(v[0], v[1], v[2]));
      break;
    }
    case SOCK_RGBA: {
      const float *v = socket.default_value_typed<bNodeSocketValueRGBA>()->value;
      res.value = MaterialX::Value::createValue<MaterialX::Color4>(
          MaterialX::Color4(v[0], v[1], v[2], v[3]));
      break;
    }
    default: {
      CLOG_WARN(LOG_MATERIALX_SHADER, "Unsupported socket type: %d", socket.type);
    }
  }
  return res.convert(to_type);
}

NodeItem NodeParser::get_input_link(const bNodeSocket &socket,
                                    NodeItem::Type to_type,
                                    bool use_group_default)
{
  const bNodeLink *link = socket.link;
  if (!(link && link->is_used())) {
    return empty();
  }

  const bNode *from_node = link->fromnode;

  /* Passing NODE_REROUTE nodes */
  while (from_node->is_reroute()) {
    link = from_node->input_socket(0).link;
    if (!(link && link->is_used())) {
      return empty();
    }
    from_node = link->fromnode;
  }

  if (from_node->is_group()) {
    return GroupNodeParser(graph_,
                           depsgraph_,
                           material_,
                           from_node,
                           link->fromsock,
                           to_type,
                           group_parser_,
                           export_params_,
                           use_group_default)
        .compute_full();
  }
  if (from_node->is_group_input()) {
    return GroupInputNodeParser(graph_,
                                depsgraph_,
                                material_,
                                from_node,
                                link->fromsock,
                                to_type,
                                group_parser_,
                                export_params_,
                                use_group_default)
        .compute_full();
  }

  if (!from_node->typeinfo->materialx_fn) {
    CLOG_WARN(LOG_MATERIALX_SHADER,
              "Unsupported node: %s [%d]",
              from_node->name,
              from_node->typeinfo->type);
    return empty();
  }

  NodeParserData data = {
      graph_, depsgraph_, material_, to_type, group_parser_, empty(), export_params_};
  from_node->typeinfo->materialx_fn(&data, const_cast<bNode *>(from_node), link->fromsock);
  return data.result;
}

NodeItem NodeParser::get_input_value(const bNodeSocket &socket, NodeItem::Type to_type)
{
  NodeItem res = get_input_link(socket, to_type, true);
  if (!res) {
    res = get_default(socket, to_type);
  }
  return res;
}

}  // namespace blender::nodes::materialx
