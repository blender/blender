/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_geo_closure.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"

#include "BLO_read_write.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_evaluate_closure_cc {

NODE_STORAGE_FUNCS(NodeGeometryEvaluateClosure)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Closure>("Closure");

  const bNode *node = b.node_or_null();
  if (node) {
    const auto &storage = node_storage(*node);
    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeGeometryEvaluateClosureInputItem &item = storage.input_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const std::string identifier = EvaluateClosureInputItemsAccessor::socket_identifier_for_item(
          item);
      b.add_input(socket_type, item.name, identifier);
    }
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeGeometryEvaluateClosureOutputItem &item = storage.output_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const std::string identifier =
          EvaluateClosureOutputItemsAccessor::socket_identifier_for_item(item);
      b.add_output(socket_type, item.name, identifier).propagate_all().reference_pass_all();
    }
  }

  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_callocN<NodeGeometryEvaluateClosure>(__func__);
  node->storage = storage;
}

static void node_copy_storage(bNodeTree * /*tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryEvaluateClosure &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeGeometryEvaluateClosure>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<EvaluateClosureInputItemsAccessor>(*src_node, *dst_node);
  socket_items::copy_array<EvaluateClosureOutputItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<EvaluateClosureInputItemsAccessor>(*node);
  socket_items::destruct_array<EvaluateClosureOutputItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  if (link->tonode == node) {
    return socket_items::try_add_item_via_any_extend_socket<EvaluateClosureInputItemsAccessor>(
        *ntree, *node, *node, *link);
  }
  return socket_items::try_add_item_via_any_extend_socket<EvaluateClosureOutputItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  if (uiLayout *panel = uiLayoutPanel(C, layout, "input_items", false, IFACE_("Input Items"))) {
    socket_items::ui::draw_items_list_with_operators<EvaluateClosureInputItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<EvaluateClosureInputItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          uiItemR(panel, item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
  if (uiLayout *panel = uiLayoutPanel(C, layout, "output_items", false, IFACE_("Output Items"))) {
    socket_items::ui::draw_items_list_with_operators<EvaluateClosureOutputItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<EvaluateClosureOutputItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          uiItemR(panel, item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<EvaluateClosureInputItemsAccessor>();
  socket_items::ops::make_common_operators<EvaluateClosureOutputItemsAccessor>();
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeEvaluateClosure", GEO_NODE_EVALUATE_CLOSURE);
  ntype.ui_name = "Evaluate Closure";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  bke::node_type_storage(
      ntype, "NodeGeometryEvaluateClosure", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_evaluate_closure_cc

namespace blender::nodes {

StructRNA *EvaluateClosureInputItemsAccessor::item_srna =
    &RNA_NodeGeometryEvaluateClosureInputItem;
int EvaluateClosureInputItemsAccessor::node_type = GEO_NODE_EVALUATE_CLOSURE;
int EvaluateClosureInputItemsAccessor::item_dna_type = SDNA_TYPE_FROM_STRUCT(
    NodeGeometryEvaluateClosureInputItem);

void EvaluateClosureInputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void EvaluateClosureInputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA *EvaluateClosureOutputItemsAccessor::item_srna =
    &RNA_NodeGeometryEvaluateClosureOutputItem;
int EvaluateClosureOutputItemsAccessor::node_type = GEO_NODE_EVALUATE_CLOSURE;
int EvaluateClosureOutputItemsAccessor::item_dna_type = SDNA_TYPE_FROM_STRUCT(
    NodeGeometryEvaluateClosureOutputItem);

void EvaluateClosureOutputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void EvaluateClosureOutputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

const bNodeSocket *evaluate_closure_node_internally_linked_input(const bNodeSocket &output_socket)
{
  const bNode &node = output_socket.owner_node();
  const bNodeTree &tree = node.owner_tree();
  BLI_assert(node.is_type("GeometryNodeEvaluateClosure"));
  const auto &storage = *static_cast<const NodeGeometryEvaluateClosure *>(node.storage);
  if (output_socket.index() >= storage.output_items.items_num) {
    return nullptr;
  }
  const NodeGeometryEvaluateClosureOutputItem &output_item =
      storage.output_items.items[output_socket.index()];
  const SocketInterfaceKey output_key{output_item.name};
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeGeometryEvaluateClosureInputItem &input_item = storage.input_items.items[i];
    const SocketInterfaceKey input_key{input_item.name};
    if (output_key.matches(input_key)) {
      if (!tree.typeinfo->validate_link ||
          tree.typeinfo->validate_link(eNodeSocketDatatype(input_item.socket_type),
                                       eNodeSocketDatatype(output_item.socket_type)))
      {
        return &node.input_socket(i + 1);
      }
    }
  }
  return nullptr;
}

}  // namespace blender::nodes
