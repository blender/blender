/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLO_read_write.hh"

#include "BKE_node_socket_value.hh"

#include "NOD_geo_combine_list.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_combine_list_cc {

NODE_STORAGE_FUNCS(NodeCombineList)

using ItemsAccessor = CombineListItemsAccessor;

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const NodeCombineList &storage = node_storage(*node);
  const eNodeSocketDatatype data_type = storage.data_type;
  const Span<CombineListItem> items = storage.items_span();

  for (const int i : items.index_range()) {
    const std::string identifier = ItemsAccessor::socket_identifier_for_item(items[i]);
    auto &input = b.add_input(data_type, UString(std::to_string(i)), UString(identifier));
    input.structure_type(StructureType::Dynamic);
    /* Labels are ugly in combination with data-block pickers and are usually disabled. */
    input.optional_label(ELEM(data_type,
                              SOCK_OBJECT,
                              SOCK_IMAGE,
                              SOCK_COLLECTION,
                              SOCK_MATERIAL,
                              SOCK_FONT,
                              SOCK_SCENE,
                              SOCK_TEXT_ID,
                              SOCK_MASK,
                              SOCK_SOUND));
  }

  b.add_input<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Dynamic)
      .custom_draw(socket_items::ui::draw_extend_socket_fn<ItemsAccessor>());

  b.add_output(data_type, "List"_ustr)
      .structure_type(StructureType::List)
      .propagate_all()
      .description("List with one element per item");
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNode &node = *static_cast<bNode *>(ptr->data);
  NodeCombineList &storage = node_storage(node);
  if (ui::Layout *panel = layout.panel(C, "combine_list_items", false, IFACE_("Items"))) {
    panel->op("node.combine_list_item_add", IFACE_("Add Item"), ICON_ADD);
    ui::Layout *col = &panel->column(false);
    for (const int i : IndexRange(storage.items_num)) {
      ui::Layout *row = &col->row(false);
      row->label(node.input_socket(i).name, ICON_NONE);
      PointerRNA op_ptr = row->op("node.combine_list_item_remove", "", ICON_REMOVE);
      RNA_int_set(&op_ptr, "index", i);
    }
  }
}

static void node_operators()
{
  socket_items::ops::make_add_item_operator<ItemsAccessor>();
  socket_items::ops::make_remove_item_by_index_operator<ItemsAccessor>();
  socket_items::ops::make_move_item_operator<ItemsAccessor>();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeCombineList *data = MEM_new<NodeCombineList>(__func__);
  data->data_type = SOCK_FLOAT;
  data->next_identifier = 0;

  const int default_items_num = 1;
  data->items = MEM_new_array<CombineListItem>(default_items_num, __func__);
  for (const int i : IndexRange(default_items_num)) {
    data->items[i].identifier = data->next_identifier++;
  }
  data->items_num = default_items_num;

  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype other_type = params.other_socket().type;
  bke::bNodeTreeType &tree_type = *params.node_tree().typeinfo;
  bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(other_type);
  if (!socket_type ||
      (tree_type.valid_socket_type && !tree_type.valid_socket_type(&tree_type, socket_type)))
  {
    return;
  }

  if (params.in_out() == SOCK_OUT) {
    params.add_item(IFACE_("List"), [other_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeCombineList"_ustr);
      node_storage(node).data_type = other_type;
      params.update_and_connect_available_socket(node, "List"_ustr);
    });
  }
  else {
    params.add_item(IFACE_("Item"), [other_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeCombineList"_ustr);
      node_storage(node).data_type = other_type;
      params.update_and_connect_available_socket(node, "0"_ustr);
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeCombineList &storage = node_storage(params.node());
  const Span<CombineListItem> items = storage.items_span();
  const eNodeSocketDatatype data_type = storage.data_type;

  const CPPType *cpp_type = bke::socket_type_to_geo_nodes_base_cpp_type(data_type);
  if (!cpp_type) {
    params.set_default_remaining_outputs();
    return;
  }

  Array<bke::SocketValueVariant> values(items.size());
  for (const int i : items.index_range()) {
    const std::string identifier = ItemsAccessor::socket_identifier_for_item(items[i]);
    values[i] = params.extract_input<bke::SocketValueVariant>(UString(identifier));
  }

  const bool all_single = std::all_of(
      values.begin(), values.end(), [](const bke::SocketValueVariant &value) {
        return value.is_single();
      });

  if (all_single) {
    GArray<> array(*cpp_type, items.size(), NoInitialization());
    for (const int i : items.index_range()) {
      void *value_ptr = const_cast<void *>(values[i].get_single_ptr_raw());
      cpp_type->move_construct(value_ptr, array[i]);
    }
    params.set_output("List"_ustr, GList::from_garray(std::move(array)));
  }
  else {
    params.set_output("List"_ustr, GList::from_container(std::move(values)));
  }
}

static const EnumPropertyItem *data_type_items_callback(bContext * /*C*/,
                                                        PointerRNA *ptr,
                                                        PropertyRNA * /*prop*/,
                                                        bool *r_free)
{
  *r_free = true;
  const bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bke::bNodeTreeType *ntree_type = ntree.typeinfo;
  return enum_items_filter(
      rna_enum_node_socket_data_type_items, [&](const EnumPropertyItem &item) -> bool {
        bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(item.value);
        return ntree_type->valid_socket_type(ntree_type, socket_type);
      });
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "",
                    rna_enum_node_socket_data_type_items,
                    NOD_storage_enum_accessors(data_type),
                    SOCK_FLOAT,
                    data_type_items_callback);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ItemsAccessor>(*node);
  MEM_delete(static_cast<NodeCombineList *>(node->storage));
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeCombineList &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeCombineList>(__func__, dna::shallow_copy(src_storage));
  dst_node->storage = dst_storage;

  socket_items::copy_array<ItemsAccessor>(*src_node, *dst_node);
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

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCombineList"_ustr);
  ntype.ui_name = "Combine List";
  ntype.ui_description = "Combine an arbitrary number of values into a list";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  bke::node_type_storage(ntype, "NodeCombineList", node_free_storage, node_copy_storage);
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_combine_list_cc

namespace blender::nodes {

StructRNA **CombineListItemsAccessor::item_srna = &RNA_CombineListItem;

void CombineListItemsAccessor::blend_write_item(BlendWriter * /*writer*/, const ItemT & /*item*/)
{
}

void CombineListItemsAccessor::blend_read_data_item(BlendDataReader * /*reader*/, ItemT & /*item*/)
{
}

}  // namespace blender::nodes

namespace blender {

Span<CombineListItem> NodeCombineList::items_span() const
{
  return Span<CombineListItem>(items, items_num);
}

MutableSpan<CombineListItem> NodeCombineList::items_span()
{
  return MutableSpan<CombineListItem>(items, items_num);
}

}  // namespace blender
