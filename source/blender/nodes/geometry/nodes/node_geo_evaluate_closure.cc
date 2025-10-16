/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_geo_closure.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_sync_sockets.hh"

#include "BKE_idprop.hh"

#include "BLO_read_write.hh"

#include "node_geometry_util.hh"
#include "shader/node_shader_util.hh"

namespace blender::nodes::node_geo_evaluate_closure_cc {

NODE_STORAGE_FUNCS(NodeEvaluateClosure)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Closure>("Closure");

  const bNode *node = b.node_or_null();
  auto &panel = b.add_panel("Interface");
  if (node) {
    const auto &storage = node_storage(*node);
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const std::string identifier =
          EvaluateClosureOutputItemsAccessor::socket_identifier_for_item(item);
      auto &decl = panel.add_output(socket_type, item.name, identifier);
      if (item.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
    }
    panel.add_output<decl::Extend>("", "__extend__");
    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeEvaluateClosureInputItem &item = storage.input_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const std::string identifier = EvaluateClosureInputItemsAccessor::socket_identifier_for_item(
          item);
      auto &decl = panel.add_input(socket_type, item.name, identifier);
      if (item.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
    }
    panel.add_input<decl::Extend>("", "__extend__");
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_callocN<NodeEvaluateClosure>(__func__);
  node->storage = storage;
}

static void node_copy_storage(bNodeTree * /*tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeEvaluateClosure &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeEvaluateClosure>(__func__, src_storage);
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

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  if (params.C && params.link.tosock == params.node.inputs.first &&
      params.link.fromsock->type == SOCK_CLOSURE)
  {
    const NodeEvaluateClosure &storage = node_storage(params.node);
    if (storage.input_items.items_num == 0 && storage.output_items.items_num == 0) {
      SpaceNode *snode = CTX_wm_space_node(params.C);
      if (snode && snode->edittree == &params.ntree) {
        sync_sockets_evaluate_closure(*snode, params.node, nullptr, params.link.fromsock);
      }
    }
    return true;
  }
  if (params.link.tonode == &params.node) {
    return socket_items::try_add_item_via_any_extend_socket<EvaluateClosureInputItemsAccessor>(
        params.ntree, params.node, params.node, params.link);
  }
  return socket_items::try_add_item_via_any_extend_socket<EvaluateClosureOutputItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->op("node.sockets_sync", IFACE_("Sync"), ICON_FILE_REFRESH);
  layout->prop(ptr, "define_signature", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (uiLayout *panel = layout->panel(C, "input_items", false, IFACE_("Input Items"))) {
    socket_items::ui::draw_items_list_with_operators<EvaluateClosureInputItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<EvaluateClosureInputItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          const auto &item = *item_ptr->data_as<NodeEvaluateClosureInputItem>();
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          if (!socket_type_always_single(eNodeSocketDatatype(item.socket_type))) {
            panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
          }
        });
  }
  if (uiLayout *panel = layout->panel(C, "output_items", false, IFACE_("Output Items"))) {
    socket_items::ui::draw_items_list_with_operators<EvaluateClosureOutputItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<EvaluateClosureOutputItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          const auto &item = *item_ptr->data_as<NodeEvaluateClosureOutputItem>();
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          if (!socket_type_always_single(eNodeSocketDatatype(item.socket_type))) {
            panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
          }
        });
  }
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode & /*node*/,
                                                       const bNodeSocket &output_socket)
{
  return evaluate_closure_node_internally_linked_input(output_socket);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.in_out == SOCK_IN) {
    params.add_item(IFACE_("Item"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeEvaluateClosure");
      const auto *item =
          socket_items::add_item_with_socket_type_and_name<EvaluateClosureOutputItemsAccessor>(
              params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
      params.update_and_connect_available_socket(node, item->name);
    });
    return;
  }
  if (other_socket.type == SOCK_CLOSURE) {
    params.add_item(IFACE_("Closure"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeEvaluateClosure");
      params.connect_available_socket(node, "Closure");

      SpaceNode &snode = *CTX_wm_space_node(&params.C);
      sync_sockets_evaluate_closure(snode, node, nullptr);
    });
  }
  if (EvaluateClosureInputItemsAccessor::supports_socket_type(other_socket.typeinfo->type,
                                                              params.node_tree().type))
  {
    params.add_item(
        IFACE_("Item"),
        [](LinkSearchOpParams &params) {
          bNode &node = params.add_node("NodeEvaluateClosure");
          const auto *item =
              socket_items::add_item_with_socket_type_and_name<EvaluateClosureInputItemsAccessor>(
                  params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
          nodes::update_node_declaration_and_sockets(params.node_tree, node);
          params.connect_available_socket_by_identifier(
              node, EvaluateClosureInputItemsAccessor::socket_identifier_for_item(*item));
        },
        other_socket.type == SOCK_CLOSURE ? -1 : 0);
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<EvaluateClosureInputItemsAccessor>();
  socket_items::ops::make_common_operators<EvaluateClosureOutputItemsAccessor>();
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<EvaluateClosureInputItemsAccessor>(&writer, node);
  socket_items::blend_write<EvaluateClosureOutputItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<EvaluateClosureInputItemsAccessor>(&reader, node);
  socket_items::blend_read_data<EvaluateClosureOutputItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  sh_geo_node_type_base(&ntype, "NodeEvaluateClosure", NODE_EVALUATE_CLOSURE);
  ntype.ui_name = "Evaluate Closure";
  ntype.ui_description = "Execute a given closure";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.register_operators = node_operators;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_type_storage(ntype, "NodeEvaluateClosure", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_evaluate_closure_cc

namespace blender::nodes {

StructRNA *EvaluateClosureInputItemsAccessor::item_srna = &RNA_NodeEvaluateClosureInputItem;

void EvaluateClosureInputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void EvaluateClosureInputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA *EvaluateClosureOutputItemsAccessor::item_srna = &RNA_NodeEvaluateClosureOutputItem;

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
  BLI_assert(node.is_type("NodeEvaluateClosure"));
  const auto &storage = *static_cast<const NodeEvaluateClosure *>(node.storage);
  if (output_socket.index() >= storage.output_items.items_num) {
    return nullptr;
  }
  const NodeEvaluateClosureOutputItem &output_item =
      storage.output_items.items[output_socket.index()];
  const StringRef output_key = output_item.name;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeEvaluateClosureInputItem &input_item = storage.input_items.items[i];
    const StringRef input_key = input_item.name;
    if (output_key == input_key) {
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
