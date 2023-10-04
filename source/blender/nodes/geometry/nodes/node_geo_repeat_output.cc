/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "BKE_compute_contexts.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_geometry.hh"
#include "NOD_socket.hh"
#include "NOD_zone_socket_items.hh"

#include "BLI_string_utils.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static std::unique_ptr<SocketDeclaration> socket_declaration_for_repeat_item(
    const NodeRepeatItem &item, const eNodeSocketInOut in_out, const int corresponding_input = -1)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);

  std::unique_ptr<SocketDeclaration> decl;

  auto handle_field_decl = [&](SocketDeclaration &decl) {
    if (in_out == SOCK_IN) {
      decl.input_field_type = InputSocketFieldType::IsSupported;
    }
    else {
      decl.output_field_dependency = OutputFieldDependency::ForPartiallyDependentField(
          {corresponding_input});
    }
  };

  switch (socket_type) {
    case SOCK_FLOAT:
      decl = std::make_unique<decl::Float>();
      handle_field_decl(*decl);
      break;
    case SOCK_VECTOR:
      decl = std::make_unique<decl::Vector>();
      handle_field_decl(*decl);
      break;
    case SOCK_RGBA:
      decl = std::make_unique<decl::Color>();
      handle_field_decl(*decl);
      break;
    case SOCK_BOOLEAN:
      decl = std::make_unique<decl::Bool>();
      handle_field_decl(*decl);
      break;
    case SOCK_ROTATION:
      decl = std::make_unique<decl::Rotation>();
      handle_field_decl(*decl);
      break;
    case SOCK_INT:
      decl = std::make_unique<decl::Int>();
      handle_field_decl(*decl);
      break;
    case SOCK_STRING:
      decl = std::make_unique<decl::String>();
      break;
    case SOCK_GEOMETRY:
      decl = std::make_unique<decl::Geometry>();
      break;
    case SOCK_OBJECT:
      decl = std::make_unique<decl::Object>();
      break;
    case SOCK_IMAGE:
      decl = std::make_unique<decl::Image>();
      break;
    case SOCK_COLLECTION:
      decl = std::make_unique<decl::Collection>();
      break;
    case SOCK_MATERIAL:
      decl = std::make_unique<decl::Material>();
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  decl->name = item.name ? item.name : "";
  decl->identifier = RepeatItemsAccessor::socket_identifier_for_item(item);
  decl->in_out = in_out;
  return decl;
}

void socket_declarations_for_repeat_items(const Span<NodeRepeatItem> items,
                                          NodeDeclaration &r_declaration)
{
  for (const int i : items.index_range()) {
    const NodeRepeatItem &item = items[i];
    SocketDeclarationPtr input_decl = socket_declaration_for_repeat_item(item, SOCK_IN);
    r_declaration.inputs.append(input_decl.get());
    r_declaration.items.append(std::move(input_decl));

    SocketDeclarationPtr output_decl = socket_declaration_for_repeat_item(
        item, SOCK_OUT, r_declaration.inputs.size() - 1);
    r_declaration.outputs.append(output_decl.get());
    r_declaration.items.append(std::move(output_decl));
  }
  SocketDeclarationPtr input_extend_decl = decl::create_extend_declaration(SOCK_IN);
  r_declaration.inputs.append(input_extend_decl.get());
  r_declaration.items.append(std::move(input_extend_decl));

  SocketDeclarationPtr output_extend_decl = decl::create_extend_declaration(SOCK_OUT);
  r_declaration.outputs.append(output_extend_decl.get());
  r_declaration.items.append(std::move(output_extend_decl));
}
}  // namespace blender::nodes
namespace blender::nodes::node_geo_repeat_output_cc {

NODE_STORAGE_FUNCS(NodeGeometryRepeatOutput);

static void node_declare_dynamic(const bNodeTree & /*node_tree*/,
                                 const bNode &node,
                                 NodeDeclaration &r_declaration)
{
  const NodeGeometryRepeatOutput &storage = node_storage(node);
  socket_declarations_for_repeat_items(storage.items_span(), r_declaration);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryRepeatOutput *data = MEM_cnew<NodeGeometryRepeatOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_cnew_array<NodeRepeatItem>(1, __func__);
  data->items[0].name = BLI_strdup(DATA_("Geometry"));
  data->items[0].socket_type = SOCK_GEOMETRY;
  data->items[0].identifier = data->next_identifier++;
  data->items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  NodeGeometryRepeatOutput &storage = node_storage(*node);
  for (NodeRepeatItem &item : storage.items_span()) {
    MEM_SAFE_FREE(item.name);
  }
  MEM_SAFE_FREE(storage.items);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryRepeatOutput &src_storage = node_storage(*src_node);
  NodeGeometryRepeatOutput *dst_storage = MEM_cnew<NodeGeometryRepeatOutput>(__func__);

  dst_storage->items = MEM_cnew_array<NodeRepeatItem>(src_storage.items_num, __func__);
  dst_storage->items_num = src_storage.items_num;
  dst_storage->active_index = src_storage.active_index;
  dst_storage->next_identifier = src_storage.next_identifier;
  dst_storage->inspection_index = src_storage.inspection_index;
  for (const int i : IndexRange(src_storage.items_num)) {
    if (char *name = src_storage.items[i].name) {
      dst_storage->items[i].identifier = src_storage.items[i].identifier;
      dst_storage->items[i].name = BLI_strdup(name);
      dst_storage->items[i].socket_type = src_storage.items[i].socket_type;
    }
  }

  dst_node->storage = dst_storage;
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<RepeatItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REPEAT_OUTPUT, "Repeat Output", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare_dynamic = node_declare_dynamic;
  ntype.insert_link = node_insert_link;
  node_type_storage(&ntype, "NodeGeometryRepeatOutput", node_free_storage, node_copy_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_repeat_output_cc

blender::Span<NodeRepeatItem> NodeGeometryRepeatOutput::items_span() const
{
  return blender::Span<NodeRepeatItem>(items, items_num);
}

blender::MutableSpan<NodeRepeatItem> NodeGeometryRepeatOutput::items_span()
{
  return blender::MutableSpan<NodeRepeatItem>(items, items_num);
}
