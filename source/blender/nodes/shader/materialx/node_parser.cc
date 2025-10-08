/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_parser.h"

#include "group_nodes.h"

#include "BKE_node_runtime.hh"

namespace blender::nodes::materialx {

constexpr StringRef TEXCOORD_NODE_NAME = "node_texcoord";

CLG_LOGREF_DECLARE_GLOBAL(LOG_IO_MATERIALX, "io.materialx");

NodeParser::NodeParser(NodeGraph &graph,
                       const bNode *node,
                       const bNodeSocket *socket_out,
                       NodeItem::Type to_type,
                       GroupNodeParser *group_parser)
    : graph_(graph),
      node_(node),
      socket_out_(socket_out),
      to_type_(to_type),
      group_parser_(group_parser)
{
}

NodeItem NodeParser::compute_full()
{
  NodeItem res = empty();

  if (socket_out_ && !NodeItem::is_convertible(eNodeSocketDatatype(socket_out_->type), to_type_)) {
    return res;
  }

  /* Checking if node was already computed */
  const std::string res_node_name = node_name();
  res = graph_.get_node(res_node_name);
  if (!res.node) {
    CLOG_DEBUG(LOG_IO_MATERIALX,
               "%s [%d] => %s",
               node_->name,
               node_->typeinfo->type_legacy,
               NodeItem::type(to_type_).c_str());

    res = compute();
    if (res.node) {
      res.node->setName(res_node_name);
    }
  }
  return res.convert(to_type_);
}

std::string NodeParser::node_name(const char *override_output_name) const
{
  const NodeItem::Type to_type =
      ELEM(to_type_, NodeItem::Type::BSDF, NodeItem::Type::EDF, NodeItem::Type::SurfaceOpacity) ?
          to_type_ :
          NodeItem::Type::Empty;
  const StringRef socket_out_name = (override_output_name) ? override_output_name :
                                    (socket_out_)          ? socket_out_->identifier :
                                                             "";
  return graph_.unique_node_name(node_, socket_out_name, to_type);
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
  return get_default(*node_->input_by_identifier(name), to_type);
}

NodeItem NodeParser::get_input_default(int index, NodeItem::Type to_type)
{
  return get_default(node_->input_socket(index), to_type);
}

NodeItem NodeParser::get_input_link(const std::string &name, NodeItem::Type to_type)
{
  return get_input_link(*node_->input_by_identifier(name), to_type, false);
}

NodeItem NodeParser::get_input_link(int index, NodeItem::Type to_type)
{
  return get_input_link(node_->input_socket(index), to_type, false);
}

NodeItem NodeParser::get_input_value(const std::string &name, NodeItem::Type to_type)
{
  return get_input_value(*node_->input_by_identifier(name), to_type);
}

NodeItem NodeParser::get_input_value(int index, NodeItem::Type to_type)
{
  return get_input_value(node_->input_socket(index), to_type);
}

NodeItem NodeParser::get_output_default(const std::string &name, NodeItem::Type to_type)
{
  return get_default(*node_->output_by_identifier(name), to_type);
}

NodeItem NodeParser::get_output_default(int index, NodeItem::Type to_type)
{
  return get_default(node_->output_socket(index), to_type);
}

NodeItem NodeParser::empty() const
{
  return graph_.empty_node();
}

NodeItem NodeParser::texcoord_node(NodeItem::Type type, const std::string &attribute_name)
{
  BLI_assert(ELEM(type, NodeItem::Type::Vector2, NodeItem::Type::Vector3));
  std::string name = TEXCOORD_NODE_NAME;
  if (type == NodeItem::Type::Vector3) {
    name += "_vector3";
  }
  NodeItem res = graph_.get_node(name);
  if (!res.node) {
    /* TODO: Use "Pref" generated texture coordinates for 3D, but needs
     * work in USD and Hydra mesh export. */
    const bool is_active_uvmap = attribute_name == "" ||
                                 attribute_name == graph_.export_params.original_active_uvmap_name;
    if (graph_.export_params.new_active_uvmap_name == "st" && is_active_uvmap) {
      res = create_node("texcoord", type);
    }
    else {
      const std::string &geomprop = (is_active_uvmap) ?
                                        graph_.export_params.new_active_uvmap_name :
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
      CLOG_WARN(LOG_IO_MATERIALX, "Unsupported socket type: %d", socket.type);
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

  /* Passing reroute nodes. */
  while (from_node->is_reroute()) {
    link = from_node->input_socket(0).link;
    if (!(link && link->is_used())) {
      return empty();
    }
    from_node = link->fromnode;
  }

  if (from_node->is_group()) {
    return GroupNodeParser(
               graph_, from_node, link->fromsock, to_type, group_parser_, use_group_default)
        .compute_full();
  }
  if (from_node->is_group_input()) {
    return GroupInputNodeParser(
               graph_, from_node, link->fromsock, to_type, group_parser_, use_group_default)
        .compute_full();
  }

  if (!from_node->typeinfo->materialx_fn) {
    CLOG_WARN(LOG_IO_MATERIALX,
              "Unsupported node: %s [%d]",
              from_node->name,
              from_node->typeinfo->type_legacy);
    return empty();
  }

  NodeParserData data = {graph_, to_type, group_parser_, empty()};
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
