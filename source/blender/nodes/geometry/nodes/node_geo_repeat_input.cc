/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_compute_contexts.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_geometry.hh"
#include "NOD_socket.hh"
#include "NOD_zone_socket_items.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_repeat_input_cc {

NODE_STORAGE_FUNCS(NodeGeometryRepeatInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Iterations").min(0).default_value(1);

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (ELEM(nullptr, node, tree)) {
    return;
  }

  const NodeGeometryRepeatInput &storage = node_storage(*node);
  const bNode *output_node = tree->node_by_id(storage.output_node_id);
  if (output_node == nullptr) {
    return;
  }
  const auto &output_storage = *static_cast<const NodeGeometryRepeatOutput *>(
      output_node->storage);
  for (const int i : IndexRange(output_storage.items_num)) {
    const NodeRepeatItem &item = output_storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const StringRef name = item.name ? item.name : "";
    const std::string identifier = RepeatItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier);
    auto &output_decl = b.add_output(socket_type, name, identifier);
    if (socket_type_supports_fields(socket_type)) {
      input_decl.supports_field();
      output_decl.dependent_field({input_decl.input_index()});
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryRepeatInput *data = MEM_cnew<NodeGeometryRepeatInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  bNode *output_node = ntree->node_by_id(node_storage(*node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<RepeatItemsAccessor>(
      *ntree, *node, *output_node, *link);
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REPEAT_INPUT, "Repeat Input", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = nullptr;
  ntype.insert_link = node_insert_link;
  node_type_storage(
      &ntype, "NodeGeometryRepeatInput", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_repeat_input_cc
