/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "ED_screen.hh"

#include "NOD_geo_bundle.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_sync_sockets.hh"

#include "BKE_idprop.hh"

#include "BLO_read_write.hh"

#include "UI_interface_layout.hh"
#include "shader/node_shader_util.hh"

#include <fmt/format.h>

namespace blender::nodes::node_geo_separate_bundle_cc {

NODE_STORAGE_FUNCS(NodeSeparateBundle);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bundle>("Bundle");
  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (tree && node) {
    const NodeSeparateBundle &storage = node_storage(*node);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeSeparateBundleItem &item = storage.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      const std::string identifier = SeparateBundleItemsAccessor::socket_identifier_for_item(item);
      auto &decl = b.add_output(socket_type, name, identifier)
                       .socket_name_ptr(
                           &tree->id, SeparateBundleItemsAccessor::item_srna, &item, "name")
                       .propagate_all()
                       .reference_pass_all();
      if (item.structure_type != NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
    }
  }
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_callocN<NodeSeparateBundle>(__func__);
  node->storage = storage;
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeSeparateBundle &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeSeparateBundle>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<SeparateBundleItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<SeparateBundleItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  if (params.C && params.link.tonode == &params.node && params.link.fromsock->type == SOCK_BUNDLE)
  {
    const NodeSeparateBundle &storage = node_storage(params.node);
    if (storage.items_num == 0) {
      SpaceNode *snode = CTX_wm_space_node(params.C);
      if (snode && snode->edittree == &params.ntree) {
        sync_sockets_separate_bundle(*snode, params.node, nullptr, params.link.fromsock);
      }
    }
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<SeparateBundleItemsAccessor>(
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
    socket_items::ui::draw_items_list_with_operators<SeparateBundleItemsAccessor>(
        C, panel, ntree, node);
    socket_items::ui::draw_active_item_props<SeparateBundleItemsAccessor>(
        ntree, node, [&](PointerRNA *item_ptr) {
          const auto &item = *item_ptr->data_as<NodeSeparateBundleItem>();
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
  socket_items::ops::make_common_operators<SeparateBundleItemsAccessor>();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  nodes::BundlePtr bundle = params.extract_input<nodes::BundlePtr>("Bundle");
  if (!bundle) {
    params.set_default_remaining_outputs();
    return;
  }

  const bNode &node = params.node();
  const NodeSeparateBundle &storage = node_storage(node);

  lf::Params &lf_params = params.low_level_lazy_function_params();

  for (const int i : IndexRange(storage.items_num)) {
    const NodeSeparateBundleItem &item = storage.items[i];
    const StringRef name = item.name;
    if (name.is_empty()) {
      continue;
    }
    const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type);
    if (!stype || !stype->geometry_nodes_default_value) {
      continue;
    }
    const BundleItemValue *value = bundle->lookup(name);
    if (!value) {
      params.error_message_add(
          NodeWarningType::Error,
          fmt::format(fmt::runtime(TIP_("Value not found in bundle: \"{}\"")), name));
      continue;
    }
    const auto *socket_value = std::get_if<BundleItemSocketValue>(&value->value);
    if (!socket_value) {
      params.error_message_add(
          NodeWarningType::Error,
          fmt::format("{}: \"{}\"", TIP_("Cannot get internal value from bundle"), name));
      continue;
    }

    SocketValueVariant output_value = std::move(socket_value->value);
    if (socket_value->type->type != stype->type) {
      if (std::optional<SocketValueVariant> converted_value = implicitly_convert_socket_value(
              *socket_value->type, output_value, *stype))
      {
        output_value = std::move(*converted_value);
        params.error_message_add(
            NodeWarningType::Info,
            fmt::format("{}: \"{}\" ({} " BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE " {})",
                        TIP_("Implicit type conversion when separating bundle"),
                        name,
                        TIP_(socket_value->type->label),
                        TIP_(stype->label)));
      }
      else {
        params.error_message_add(
            NodeWarningType::Error,
            fmt::format("{}: \"{}\" ({} " BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE " {})",
                        TIP_("Conversion not supported when separating bundle"),
                        name,
                        TIP_(socket_value->type->label),
                        TIP_(stype->label)));
        output_value = *stype->geometry_nodes_default_value;
      }
    }
    lf_params.set_output(i, std::move(output_value));
  }

  params.set_default_remaining_outputs();
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.in_out == SOCK_IN) {
    if (!SeparateBundleItemsAccessor::supports_socket_type(other_socket.typeinfo->type,
                                                           params.node_tree().type))
    {
      return;
    }
    params.add_item(IFACE_("Item"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeSeparateBundle");
      const auto *item =
          socket_items::add_item_with_socket_type_and_name<SeparateBundleItemsAccessor>(
              params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
      params.update_and_connect_available_socket(node, item->name);
    });
  }
  else {
    if (other_socket.type != SOCK_BUNDLE) {
      return;
    }
    params.add_item(IFACE_("Bundle"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeSeparateBundle");
      params.connect_available_socket(node, "Bundle");

      SpaceNode &snode = *CTX_wm_space_node(&params.C);
      sync_sockets_separate_bundle(snode, node, nullptr);
    });
  }
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<SeparateBundleItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<SeparateBundleItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  sh_geo_node_type_base(&ntype, "NodeSeparateBundle", NODE_SEPARATE_BUNDLE);
  ntype.ui_name = "Separate Bundle";
  ntype.ui_description = "Split a bundle into multiple sockets.";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.register_operators = node_operators;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_type_storage(ntype, "NodeSeparateBundle", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_separate_bundle_cc

namespace blender::nodes {

StructRNA *SeparateBundleItemsAccessor::item_srna = &RNA_NodeSeparateBundleItem;

void SeparateBundleItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void SeparateBundleItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
