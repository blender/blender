/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "NOD_geo_repeat.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_ops.hh"

#include "BLO_read_write.hh"

#include "BLI_string_utils.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "BKE_screen.hh"

#include "WM_api.hh"

#include "UI_interface.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_repeat_cc {

static void draw_repeat_state_item(uiList * /*ui_list*/,
                                   const bContext *C,
                                   uiLayout *layout,
                                   PointerRNA * /*idataptr*/,
                                   PointerRNA *itemptr,
                                   int /*icon*/,
                                   PointerRNA * /*active_dataptr*/,
                                   const char * /*active_propname*/,
                                   int /*index*/,
                                   int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, true);
  float4 color;
  RNA_float_get_array(itemptr, "color", color);
  uiTemplateNodeSocket(row, const_cast<bContext *>(C), color);
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  uiItemR(row, itemptr, "name", UI_ITEM_NONE, "", ICON_NONE);
}

/** Shared between repeat zone input and output node. */
static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *current_node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(current_node_ptr->owner_id);
  bNode *current_node = static_cast<bNode *>(current_node_ptr->data);

  const bke::bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(current_node->identifier);
  if (!zone) {
    return;
  }
  if (!zone->output_node) {
    return;
  }
  bNode &output_node = const_cast<bNode &>(*zone->output_node);
  PointerRNA output_node_ptr = RNA_pointer_create(
      current_node_ptr->owner_id, &RNA_Node, &output_node);

  static const uiListType *state_items_list = []() {
    uiListType *list = MEM_cnew<uiListType>(__func__);
    STRNCPY(list->idname, "DATA_UL_repeat_zone_state");
    list->draw_item = draw_repeat_state_item;
    WM_uilisttype_add(list);
    return list;
  }();

  if (uiLayout *panel = uiLayoutPanel(C, layout, "repeat_items", false, TIP_("Repeat Items"))) {
    uiLayout *row = uiLayoutRow(panel, false);
    uiTemplateList(row,
                   C,
                   state_items_list->idname,
                   "",
                   &output_node_ptr,
                   "repeat_items",
                   &output_node_ptr,
                   "active_index",
                   nullptr,
                   3,
                   5,
                   UILST_LAYOUT_DEFAULT,
                   0,
                   UI_TEMPLATE_LIST_FLAG_NONE);
    {
      uiLayout *ops_col = uiLayoutColumn(row, false);
      {
        uiLayout *add_remove_col = uiLayoutColumn(ops_col, true);
        uiItemO(add_remove_col, "", ICON_ADD, "node.repeat_zone_item_add");
        uiItemO(add_remove_col, "", ICON_REMOVE, "node.repeat_zone_item_remove");
      }
      {
        uiLayout *up_down_col = uiLayoutColumn(ops_col, true);
        uiItemEnumO(up_down_col, "node.repeat_zone_item_move", "", ICON_TRIA_UP, "direction", 0);
        uiItemEnumO(up_down_col, "node.repeat_zone_item_move", "", ICON_TRIA_DOWN, "direction", 1);
      }
    }
    auto &storage = *static_cast<NodeGeometryRepeatOutput *>(output_node.storage);
    if (storage.active_index >= 0 && storage.active_index < storage.items_num) {
      NodeRepeatItem &active_item = storage.items[storage.active_index];
      PointerRNA item_ptr = RNA_pointer_create(
          output_node_ptr.owner_id, RepeatItemsAccessor::item_srna, &active_item);
      uiLayoutSetPropSep(panel, true);
      uiLayoutSetPropDecorate(panel, false);
      uiItemR(panel, &item_ptr, "socket_type", UI_ITEM_NONE, nullptr, ICON_NONE);
    }
  }

  uiItemR(layout, &output_node_ptr, "inspection_index", UI_ITEM_NONE, nullptr, ICON_NONE);
}

namespace repeat_input_node {

NODE_STORAGE_FUNCS(NodeGeometryRepeatInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Int>("Iterations").min(0).default_value(1);

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (node && tree) {
    const NodeGeometryRepeatInput &storage = node_storage(*node);
    const bNode *output_node = tree->node_by_id(storage.output_node_id);
    if (output_node) {
      const auto &output_storage = *static_cast<const NodeGeometryRepeatOutput *>(
          output_node->storage);
      for (const int i : IndexRange(output_storage.items_num)) {
        const NodeRepeatItem &item = output_storage.items[i];
        const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
        const StringRef name = item.name ? item.name : "";
        const std::string identifier = RepeatItemsAccessor::socket_identifier_for_item(item);
        auto &input_decl = b.add_input(socket_type, name, identifier);
        auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
        if (socket_type_supports_fields(socket_type)) {
          input_decl.supports_field();
          output_decl.dependent_field({input_decl.index()});
        }
      }
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__").align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryRepeatInput *data = MEM_cnew<NodeGeometryRepeatInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(label, IFACE_("Repeat"), label_maxncpy);
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
  ntype.labelfunc = node_label;
  ntype.gather_link_search_ops = nullptr;
  ntype.insert_link = node_insert_link;
  ntype.no_muting = true;
  ntype.draw_buttons_ex = node_layout_ex;
  node_type_storage(
      &ntype, "NodeGeometryRepeatInput", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace repeat_input_node

namespace repeat_output_node {

NODE_STORAGE_FUNCS(NodeGeometryRepeatOutput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  const bNode *node = b.node_or_null();
  if (node) {
    const NodeGeometryRepeatOutput &storage = node_storage(*node);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeRepeatItem &item = storage.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      const std::string identifier = RepeatItemsAccessor::socket_identifier_for_item(item);
      auto &input_decl = b.add_input(socket_type, name, identifier);
      auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
      if (socket_type_supports_fields(socket_type)) {
        input_decl.supports_field();
        output_decl.dependent_field({input_decl.index()});
      }
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__").align_with_previous();
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
  socket_items::destruct_array<RepeatItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryRepeatOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeGeometryRepeatOutput>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<RepeatItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<RepeatItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void NODE_OT_repeat_zone_item_remove(wmOperatorType *ot)
{
  socket_items::ops::remove_item<RepeatItemsAccessor>(
      ot, "Remove Repeat Zone Item", __func__, "Remove active repeat zone item");
}

static void NODE_OT_repeat_zone_item_add(wmOperatorType *ot)
{
  socket_items::ops::add_item_with_name_and_type<RepeatItemsAccessor>(
      ot, "Add Repeat Zone Item", __func__, "Add repeat zone item");
}

static void NODE_OT_repeat_zone_item_move(wmOperatorType *ot)
{
  socket_items::ops::move_item<RepeatItemsAccessor>(
      ot, "Move Repeat Zone Item", __func__, "Move active repeat zone item");
}

static void node_operators()
{
  WM_operatortype_append(NODE_OT_repeat_zone_item_add);
  WM_operatortype_append(NODE_OT_repeat_zone_item_remove);
  WM_operatortype_append(NODE_OT_repeat_zone_item_move);
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REPEAT_OUTPUT, "Repeat Output", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = repeat_input_node::node_label;
  ntype.insert_link = node_insert_link;
  ntype.no_muting = true;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  node_type_storage(&ntype, "NodeGeometryRepeatOutput", node_free_storage, node_copy_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace repeat_output_node

}  // namespace blender::nodes::node_geo_repeat_cc

namespace blender::nodes {

StructRNA *RepeatItemsAccessor::item_srna = &RNA_RepeatItem;
int RepeatItemsAccessor::node_type = GEO_NODE_REPEAT_OUTPUT;

void RepeatItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeGeometryRepeatOutput *>(node.storage);
  BLO_write_struct_array(writer, NodeRepeatItem, storage.items_num, storage.items);
  for (const NodeRepeatItem &item : Span(storage.items, storage.items_num)) {
    BLO_write_string(writer, item.name);
  }
}

void RepeatItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeGeometryRepeatOutput *>(node.storage);
  BLO_read_struct_array(reader, NodeRepeatItem, storage.items_num, &storage.items);
  for (const NodeRepeatItem &item : Span(storage.items, storage.items_num)) {
    BLO_read_string(reader, &item.name);
  }
}

}  // namespace blender::nodes

blender::Span<NodeRepeatItem> NodeGeometryRepeatOutput::items_span() const
{
  return blender::Span<NodeRepeatItem>(items, items_num);
}

blender::MutableSpan<NodeRepeatItem> NodeGeometryRepeatOutput::items_span()
{
  return blender::MutableSpan<NodeRepeatItem>(items, items_num);
}
