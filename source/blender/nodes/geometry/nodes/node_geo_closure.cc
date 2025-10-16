/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_string_utf8.h"

#include "BKE_idprop.hh"

#include "NOD_geo_closure.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_sync_sockets.hh"

#include "BLO_read_write.hh"
#include "shader/node_shader_util.hh"

namespace blender::nodes::node_geo_closure_cc {

/** Shared between closure input and output node. */
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
  if (!zone->output_node_id) {
    return;
  }
  bNode &output_node = const_cast<bNode &>(*zone->output_node());

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  PointerRNA output_node_ptr = RNA_pointer_create_discrete(&ntree.id, &RNA_Node, &output_node);

  layout->op("node.sockets_sync", IFACE_("Sync"), ICON_FILE_REFRESH);
  layout->prop(&output_node_ptr, "define_signature", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (current_node->type_legacy == NODE_CLOSURE_INPUT) {
    if (uiLayout *panel = layout->panel(C, "input_items", false, IFACE_("Input Items"))) {
      socket_items::ui::draw_items_list_with_operators<ClosureInputItemsAccessor>(
          C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ClosureInputItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            const auto &item = *item_ptr->data_as<NodeClosureInputItem>();
            panel->use_property_split_set(true);
            panel->use_property_decorate_set(false);
            panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
            if (!socket_type_always_single(eNodeSocketDatatype(item.socket_type))) {
              panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
            }
          });
    }
  }
  else {
    if (uiLayout *panel = layout->panel(C, "output_items", false, IFACE_("Output Items"))) {
      socket_items::ui::draw_items_list_with_operators<ClosureOutputItemsAccessor>(
          C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ClosureOutputItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            const auto &item = *item_ptr->data_as<NodeClosureOutputItem>();
            panel->use_property_split_set(true);
            panel->use_property_decorate_set(false);
            panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
            if (!socket_type_always_single(eNodeSocketDatatype(item.socket_type))) {
              panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
            }
          });
    }
  }
}

namespace input_node {

NODE_STORAGE_FUNCS(NodeClosureInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (node && tree) {
    const NodeClosureInput &storage = node_storage(*node);
    const bNode *output_node = tree->node_by_id(storage.output_node_id);
    if (output_node) {
      const auto &output_storage = *static_cast<const NodeClosureOutput *>(output_node->storage);
      for (const int i : IndexRange(output_storage.input_items.items_num)) {
        const NodeClosureInputItem &item = output_storage.input_items.items[i];
        const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
        const std::string identifier = ClosureInputItemsAccessor::socket_identifier_for_item(item);
        auto &decl = b.add_output(socket_type, item.name, identifier);
        if (item.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
          decl.structure_type(StructureType(item.structure_type));
        }
        else {
          decl.structure_type(StructureType::Dynamic);
        }
      }
    }
  }
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(label, CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "Closure"), label_maxncpy);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeClosureInput *data = MEM_callocN<NodeClosureInput>(__func__);
  node->storage = data;
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  bNode *output_node = params.ntree.node_by_id(node_storage(params.node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<ClosureInputItemsAccessor>(
      params.ntree, params.node, *output_node, params.link);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  sh_geo_node_type_base(&ntype, "NodeClosureInput", NODE_CLOSURE_INPUT);
  ntype.ui_name = "Closure Input";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = nullptr;
  ntype.initfunc = node_init;
  ntype.labelfunc = node_label;
  ntype.no_muting = true;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  blender::bke::node_type_storage(
      ntype, "NodeClosureInput", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace input_node

namespace output_node {

NODE_STORAGE_FUNCS(NodeClosureOutput);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (node && tree) {
    const NodeClosureOutput &storage = node_storage(*node);
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeClosureOutputItem &item = storage.output_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const std::string identifier = ClosureOutputItemsAccessor::socket_identifier_for_item(item);
      auto &decl = b.add_input(socket_type, item.name, identifier).supports_field();
      if (item.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Closure>("Closure");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeClosureOutput *data = MEM_callocN<NodeClosureOutput>(__func__);
  node->storage = data;
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeClosureOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeClosureOutput>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<ClosureInputItemsAccessor>(*src_node, *dst_node);
  socket_items::copy_array<ClosureOutputItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ClosureInputItemsAccessor>(*node);
  socket_items::destruct_array<ClosureOutputItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  if (params.C && params.link.fromnode == &params.node && params.link.tosock->type == SOCK_CLOSURE)
  {
    const NodeClosureOutput &storage = node_storage(params.node);
    if (storage.input_items.items_num == 0 && storage.output_items.items_num == 0) {
      SpaceNode *snode = CTX_wm_space_node(params.C);
      if (snode && snode->edittree == &params.ntree) {
        bNode *input_node = bke::zone_type_by_node_type(NODE_CLOSURE_OUTPUT)
                                ->get_corresponding_input(params.ntree, params.node);
        if (input_node) {
          sync_sockets_closure(*snode, *input_node, params.node, nullptr, params.link.tosock);
        }
      }
    }
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<ClosureOutputItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ClosureInputItemsAccessor>();
  socket_items::ops::make_common_operators<ClosureOutputItemsAccessor>();
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.type != SOCK_CLOSURE) {
    return;
  }
  if (other_socket.in_out == SOCK_OUT) {
    return;
  }
  params.add_item_full_name(IFACE_("Closure"), [](LinkSearchOpParams &params) {
    bNode &input_node = params.add_node("NodeClosureInput");
    bNode &output_node = params.add_node("NodeClosureOutput");
    output_node.location[0] = 300;

    auto &input_storage = *static_cast<NodeClosureInput *>(input_node.storage);
    input_storage.output_node_id = output_node.identifier;

    params.connect_available_socket(output_node, "Closure");

    SpaceNode &snode = *CTX_wm_space_node(&params.C);
    sync_sockets_closure(snode, input_node, output_node, nullptr);
  });
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<ClosureInputItemsAccessor>(&writer, node);
  socket_items::blend_write<ClosureOutputItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<ClosureInputItemsAccessor>(&reader, node);
  socket_items::blend_read_data<ClosureOutputItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  sh_geo_node_type_base(&ntype, "NodeClosureOutput", NODE_CLOSURE_OUTPUT);
  ntype.ui_name = "Closure Output";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.labelfunc = input_node::node_label;
  ntype.no_muting = true;
  ntype.register_operators = node_operators;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_type_storage(ntype, "NodeClosureOutput", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace output_node

}  // namespace blender::nodes::node_geo_closure_cc

namespace blender::nodes {

StructRNA *ClosureInputItemsAccessor::item_srna = &RNA_NodeClosureInputItem;

void ClosureInputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ClosureInputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA *ClosureOutputItemsAccessor::item_srna = &RNA_NodeClosureOutputItem;

void ClosureOutputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ClosureOutputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
