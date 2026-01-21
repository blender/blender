/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLO_read_write.hh"

#include "NOD_geo_field_to_list.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_prototypes.hh"

#include "list_function_eval.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_field_to_list_cc {

NODE_STORAGE_FUNCS(GeometryNodeFieldToList)
using ItemsAccessor = FieldToListItemsAccessor;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Int>("Count").default_value(1).min(1).description(
      "The number of elements in the list");

  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const GeometryNodeFieldToList &storage = node_storage(*node);
  const Span<GeometryNodeFieldToListItem> items(storage.items, storage.items_num);

  for (const int i : items.index_range()) {
    const GeometryNodeFieldToListItem &item = items[i];
    const auto type = eNodeSocketDatatype(item.socket_type);
    const std::string input_identifier = ItemsAccessor::input_socket_identifier_for_item(item);
    const std::string output_identifier = ItemsAccessor::output_socket_identifier_for_item(item);

    b.add_input(type, item.name, input_identifier).supports_field();
    b.add_output(type, item.name, output_identifier)
        .structure_type(StructureType::List)
        .align_with_previous()
        .description("Output list with evaluated field values");
  }

  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Field);
  b.add_output<decl::Extend>("", "__extend__")
      .structure_type(StructureType::List)
      .align_with_previous();
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);
  if (ui::Layout *panel = layout.panel(C, "field_to_list_items", false, IFACE_("Items"))) {
    socket_items::ui::draw_items_list_with_operators<ItemsAccessor>(C, panel, tree, node);
    socket_items::ui::draw_active_item_props<ItemsAccessor>(tree, node, [&](PointerRNA *item_ptr) {
      panel->use_property_split_set(true);
      panel->use_property_decorate_set(false);
      panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    });
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  if (!U.experimental.use_geometry_nodes_lists) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(params.other_socket().type);
  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(data_type, SOCK_INT)) {
      params.add_item(IFACE_("Count"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeFieldToList");
        params.update_and_connect_available_socket(node, "Count");
      });
    }
    params.add_item(IFACE_("Field"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToList");
      socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, data_type, params.socket.name);
      params.update_and_connect_available_socket(node, params.socket.name);
    });
  }
  else {
    params.add_item(IFACE_("List"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToList");
      socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, data_type, params.socket.name);
      params.update_and_connect_available_socket(node, params.socket.name);
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int count = params.extract_input<int>("Count");
  if (count < 0) {
    params.error_message_add(NodeWarningType::Error, "Count must not be negative");
    params.set_default_remaining_outputs();
    return;
  }
  const GeometryNodeFieldToList &storage = node_storage(params.node());
  const Span<GeometryNodeFieldToListItem> items(storage.items, storage.items_num);

  Vector<int> required_items;
  for (const int i : items.index_range()) {
    if (params.output_is_required(ItemsAccessor::output_socket_identifier_for_item(items[i]))) {
      required_items.append(i);
    }
  }

  Vector<fn::GField> fields(required_items.size());
  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const std::string identifier = ItemsAccessor::input_socket_identifier_for_item(items[item_i]);
    fields[i] = params.extract_input<fn::GField>(identifier);
  }

  Vector<ListPtr> lists(required_items.size());
  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const auto type = eNodeSocketDatatype(items[item_i].socket_type);
    const CPPType &cpp_type = *bke::socket_type_to_geo_nodes_base_cpp_type(type);
    lists[i] = List::create(cpp_type, List::ArrayData::ForUninitialized(cpp_type, count), count);
  }

  Array<GMutableSpan> list_values(lists.size());
  for (const int i : lists.index_range()) {
    list_values[i] = {
        lists[i]->cpp_type(), std::get<List::ArrayData>(lists[i]->data()).data, count};
  }

  ListFieldContext context;
  fn::FieldEvaluator evaluator{context, count};
  for (const int i : fields.index_range()) {
    GMutableSpan values(
        lists[i]->cpp_type(), std::get<List::ArrayData>(lists[i]->data()).data, count);
    evaluator.add_with_destination(std::move(fields[i]), values);
  }

  evaluator.evaluate();

  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const std::string identifier = ItemsAccessor::output_socket_identifier_for_item(items[item_i]);
    params.set_output(identifier, std::move(lists[i]));
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->storage = MEM_new_for_free<GeometryNodeFieldToList>(__func__);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const GeometryNodeFieldToList &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<GeometryNodeFieldToList>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<ItemsAccessor>(*src_node, *dst_node);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ItemsAccessor>();
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<ItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<ItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<ItemsAccessor>(&reader, node);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  return node.input_by_identifier(output_socket.identifier);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeFieldToList");
  ntype.ui_name = "Field to List";
  ntype.ui_description = "Create a list of values";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "GeometryNodeFieldToList", node_free_storage, node_copy_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.insert_link = node_insert_link;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_field_to_list_cc

namespace blender::nodes {

StructRNA **FieldToListItemsAccessor::item_srna = &RNA_GeometryNodeFieldToListItem;

void FieldToListItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void FieldToListItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
