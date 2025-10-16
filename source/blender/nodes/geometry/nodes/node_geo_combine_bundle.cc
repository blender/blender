/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_geo_bundle.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_sync_sockets.hh"

#include "BKE_idprop.hh"

#include "BLO_read_write.hh"

#include "NOD_geometry_nodes_bundle.hh"

#include "UI_interface_layout.hh"
#include "shader/node_shader_util.hh"

namespace blender::nodes::node_geo_combine_bundle_cc {

NODE_STORAGE_FUNCS(NodeCombineBundle);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (tree && node) {
    const NodeCombineBundle &storage = node_storage(*node);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeCombineBundleItem &item = storage.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      const std::string identifier = CombineBundleItemsAccessor::socket_identifier_for_item(item);
      auto &decl = b.add_input(socket_type, name, identifier)
                       .socket_name_ptr(
                           &tree->id, CombineBundleItemsAccessor::item_srna, &item, "name")
                       .supports_field();
      if (item.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Bundle>("Bundle").propagate_all().reference_pass_all();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_callocN<NodeCombineBundle>(__func__);
  node->storage = storage;
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeCombineBundle &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeCombineBundle>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<CombineBundleItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<CombineBundleItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  if (params.C && params.link.fromnode == &params.node && params.link.tosock->type == SOCK_BUNDLE)
  {
    const NodeCombineBundle &storage = node_storage(params.node);
    if (storage.items_num == 0) {
      SpaceNode *snode = CTX_wm_space_node(params.C);
      if (snode && snode->edittree == &params.ntree) {
        sync_sockets_combine_bundle(*snode, params.node, nullptr, params.link.tosock);
      }
    }
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<CombineBundleItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(node_ptr->owner_id);
  bNode &node = *static_cast<bNode *>(node_ptr->data);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->op("node.sockets_sync", IFACE_("Sync"), ICON_FILE_REFRESH);
  layout->prop(node_ptr, "define_signature", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (uiLayout *panel = layout->panel(C, "bundle_items", false, IFACE_("Bundle Items"))) {
    socket_items::ui::draw_items_list_with_operators<CombineBundleItemsAccessor>(
        C, panel, ntree, node);
    socket_items::ui::draw_active_item_props<CombineBundleItemsAccessor>(
        ntree, node, [&](PointerRNA *item_ptr) {
          const auto &item = *item_ptr->data_as<NodeCombineBundleItem>();
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);
          if (!socket_type_always_single(eNodeSocketDatatype(item.socket_type))) {
            panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
          }
        });
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<CombineBundleItemsAccessor>();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const NodeCombineBundle &storage = node_storage(node);

  BundlePtr bundle_ptr = Bundle::create();
  BLI_assert(bundle_ptr->is_mutable());
  Bundle &bundle = const_cast<Bundle &>(*bundle_ptr);

  for (const int i : IndexRange(storage.items_num)) {
    const NodeCombineBundleItem &item = storage.items[i];
    const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type);
    if (!stype || !stype->geometry_nodes_default_value) {
      continue;
    }
    const StringRef name = item.name;
    if (name.is_empty()) {
      continue;
    }
    bke::SocketValueVariant value = params.extract_input<bke::SocketValueVariant>(
        node.input_socket(i).identifier);
    bundle.add(name, BundleItemSocketValue{stype, std::move(value)});
  }

  params.set_output("Bundle", std::move(bundle_ptr));
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.in_out == SOCK_OUT) {
    if (!CombineBundleItemsAccessor::supports_socket_type(other_socket.typeinfo->type,
                                                          params.node_tree().type))
    {
      return;
    }
    params.add_item(IFACE_("Item"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeCombineBundle");
      const auto *item =
          socket_items::add_item_with_socket_type_and_name<CombineBundleItemsAccessor>(
              params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
      params.update_and_connect_available_socket(node, item->name);
    });
  }
  else {
    if (other_socket.type != SOCK_BUNDLE) {
      return;
    }
    params.add_item(IFACE_("Bundle"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeCombineBundle");
      params.connect_available_socket(node, "Bundle");

      SpaceNode &snode = *CTX_wm_space_node(&params.C);
      sync_sockets_combine_bundle(snode, node, nullptr);
    });
  }
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

  sh_geo_node_type_base(&ntype, "NodeCombineBundle", NODE_COMBINE_BUNDLE);
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
  bke::node_type_storage(ntype, "NodeCombineBundle", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_combine_bundle_cc

namespace blender::nodes {

StructRNA *CombineBundleItemsAccessor::item_srna = &RNA_NodeCombineBundleItem;

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
  if (!result.empty()) {
    /* Disallow leading spaces. */
    if (std::isspace(result[0])) {
      result[0] = '_';
    }
    /* Disallow trailing spaces. */
    const int last_index = result.size() - 1;
    if (std::isspace(result[last_index])) {
      result[last_index] = '_';
    }
  }
  return result;
}

}  // namespace blender::nodes
