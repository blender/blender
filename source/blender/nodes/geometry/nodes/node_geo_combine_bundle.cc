/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_geo_bundle.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "BLO_read_write.hh"

#include "NOD_geometry_nodes_bundle.hh"

#include "UI_interface_layout.hh"

namespace blender::nodes::node_geo_combine_bundle_cc {

NODE_STORAGE_FUNCS(NodeGeometryCombineBundle);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (tree && node) {
    const NodeGeometryCombineBundle &storage = node_storage(*node);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeGeometryCombineBundleItem &item = storage.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      const std::string identifier = CombineBundleItemsAccessor::socket_identifier_for_item(item);
      b.add_input(socket_type, name, identifier)
          .socket_name_ptr(&tree->id, CombineBundleItemsAccessor::item_srna, &item, "name")
          .supports_field()
          .structure_type(StructureType::Dynamic);
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Bundle>("Bundle").propagate_all().reference_pass_all();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_callocN<NodeGeometryCombineBundle>(__func__);
  node->storage = storage;
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryCombineBundle &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeGeometryCombineBundle>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<CombineBundleItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<CombineBundleItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bNodeTree *tree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<CombineBundleItemsAccessor>(
      *tree, *node, *node, *link);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(node_ptr->owner_id);
  bNode &node = *static_cast<bNode *>(node_ptr->data);

  if (uiLayout *panel = layout->panel(C, "bundle_items", false, TIP_("Bundle Items"))) {
    panel->op("node.sockets_sync", "Sync", ICON_FILE_REFRESH);

    socket_items::ui::draw_items_list_with_operators<CombineBundleItemsAccessor>(
        C, panel, ntree, node);
    socket_items::ui::draw_active_item_props<CombineBundleItemsAccessor>(
        ntree, node, [&](PointerRNA *item_ptr) {
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, "Type", ICON_NONE);
        });
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<CombineBundleItemsAccessor>();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!U.experimental.use_bundle_and_closure_nodes) {
    params.set_default_remaining_outputs();
    return;
  }

  const bNode &node = params.node();
  const NodeGeometryCombineBundle &storage = node_storage(node);

  BundlePtr bundle_ptr = Bundle::create();
  BLI_assert(bundle_ptr->is_mutable());
  Bundle &bundle = const_cast<Bundle &>(*bundle_ptr);

  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryCombineBundleItem &item = storage.items[i];
    const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type);
    if (!stype || !stype->geometry_nodes_cpp_type) {
      continue;
    }
    const StringRef name = item.name;
    if (name.is_empty()) {
      continue;
    }
    void *input_ptr = params.low_level_lazy_function_params().try_get_input_data_ptr(i);
    BLI_assert(input_ptr);
    bundle.add(name, BundleItemSocketValue{stype, input_ptr});
  }

  params.set_output("Bundle", std::move(bundle_ptr));
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.type != SOCK_BUNDLE) {
    return;
  }
  if (other_socket.in_out == SOCK_OUT) {
    return;
  }

  params.add_item("Bundle", [](LinkSearchOpParams &params) {
    bNode &node = params.add_node("GeometryNodeCombineBundle");
    params.connect_available_socket(node, "Bundle");

    SpaceNode &snode = *CTX_wm_space_node(&params.C);
    ed::space_node::sync_sockets_combine_bundle(snode, node, nullptr);
  });
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<CombineBundleItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<CombineBundleItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCombineBundle", GEO_NODE_COMBINE_BUNDLE);
  ntype.ui_name = "Combine Bundle";
  ntype.ui_description = "Combine multiple socket values into one.";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.register_operators = node_operators;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_type_storage(ntype, "NodeGeometryCombineBundle", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_combine_bundle_cc

namespace blender::nodes {

StructRNA *CombineBundleItemsAccessor::item_srna = &RNA_NodeGeometryCombineBundleItem;

void CombineBundleItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void CombineBundleItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

std::string CombineBundleItemsAccessor::validate_name(const StringRef name)
{
  std::string result;
  if (name.is_empty()) {
    return result;
  }
  /* Disallow certain characters so that we can use them to e.g. build a bundle path or
   * expressions referencing multiple bundle items. We might not need all of them in the future,
   * but better reserve them now while we still can. */
  constexpr StringRefNull forbidden_chars_str = "/*&|\"^~!,{}()+$#@[];:?<>.-%\\=";
  const Span<char> forbidden_chars = forbidden_chars_str;
  for (const char c : name) {
    if (forbidden_chars.contains(c)) {
      result += '_';
    }
    else {
      result += c;
    }
  }
  return result;
}

}  // namespace blender::nodes
