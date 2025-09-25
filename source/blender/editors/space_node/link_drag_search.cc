/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_listbase.h"

#include "DNA_space_types.h"

#include "BKE_asset.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_screen.hh"

#include "UI_string_search.hh"

#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"

#include "DEG_depsgraph_build.hh"

#include "ED_asset.hh"
#include "ED_node.hh"

#include "node_intern.hh"

using blender::nodes::SocketLinkOperation;

namespace blender::ed::space_node {

struct LinkDragSearchStorage {
  bNode &from_node;
  bNodeSocket &from_socket;
  float2 cursor;
  Vector<SocketLinkOperation> search_link_ops;
  char search[256];
  bool update_items_tag = true;

  eNodeSocketInOut in_out() const
  {
    return static_cast<eNodeSocketInOut>(from_socket.in_out);
  }
};

static void link_drag_search_listen_fn(const wmRegionListenerParams *params, void *arg)
{
  LinkDragSearchStorage &storage = *static_cast<LinkDragSearchStorage *>(arg);
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_ASSET:
      if (wmn->data == ND_ASSET_LIST_READING) {
        storage.update_items_tag = true;
      }
      break;
  }
}

static void add_reroute_node_fn(nodes::LinkSearchOpParams &params)
{
  bNode &reroute = params.add_node("NodeReroute");
  if (params.socket.in_out == SOCK_IN) {
    bke::node_add_link(params.node_tree,
                       reroute,
                       *static_cast<bNodeSocket *>(reroute.outputs.first),
                       params.node,
                       params.socket);
  }
  else {
    bke::node_add_link(params.node_tree,
                       params.node,
                       params.socket,
                       reroute,
                       *static_cast<bNodeSocket *>(reroute.inputs.first));
  }
}

static void add_group_input_node_fn(nodes::LinkSearchOpParams &params)
{
  /* Add a group input based on the connected socket, and add a new group input node. */
  bNodeTreeInterfaceSocket *socket_iface = bke::node_interface::add_interface_socket_from_node(
      params.node_tree,
      params.node,
      params.socket,
      params.socket.typeinfo->idname,
      params.socket.name);
  params.node_tree.tree_interface.active_item_set(&socket_iface->item);

  bNode &group_input = params.add_node("NodeGroupInput");

  /* This is necessary to create the new sockets in the other input nodes. */
  BKE_main_ensure_invariants(*CTX_data_main(&params.C), params.node_tree.id);

  /* Hide the new input in all other group input nodes, to avoid making them taller. */
  for (bNode *node : params.node_tree.all_nodes()) {
    if (node->is_group_input()) {
      bNodeSocket *new_group_input_socket = bke::node_find_socket(
          *node, SOCK_OUT, socket_iface->identifier);
      if (new_group_input_socket) {
        new_group_input_socket->flag |= SOCK_HIDDEN;
      }
    }
  }

  /* Hide all existing inputs in the new group input node, to only display the new one. */
  LISTBASE_FOREACH (bNodeSocket *, socket, &group_input.outputs) {
    socket->flag |= SOCK_HIDDEN;
  }

  bNodeSocket *socket = bke::node_find_socket(group_input, SOCK_OUT, socket_iface->identifier);
  if (socket) {
    /* Unhide the socket for the new input in the new node and make a connection to it. */
    socket->flag &= ~SOCK_HIDDEN;
    bke::node_add_link(params.node_tree, group_input, *socket, params.node, params.socket);

    bke::node_socket_move_default_value(
        *CTX_data_main(&params.C), params.node_tree, params.socket, *socket);
  }
}

static void add_existing_group_input_fn(nodes::LinkSearchOpParams &params,
                                        const bNodeTreeInterfaceSocket &interface_socket)
{
  const eNodeSocketInOut in_out = eNodeSocketInOut(params.socket.in_out);
  NodeTreeInterfaceSocketFlag flag = NodeTreeInterfaceSocketFlag(0);
  SET_FLAG_FROM_TEST(flag, in_out & SOCK_IN, NODE_INTERFACE_SOCKET_INPUT);
  SET_FLAG_FROM_TEST(flag, in_out & SOCK_OUT, NODE_INTERFACE_SOCKET_OUTPUT);

  bNode &group_input = params.add_node("NodeGroupInput");

  LISTBASE_FOREACH (bNodeSocket *, socket, &group_input.outputs) {
    socket->flag |= SOCK_HIDDEN;
  }

  bNodeSocket *socket = bke::node_find_socket(group_input, SOCK_OUT, interface_socket.identifier);
  if (socket != nullptr) {
    socket->flag &= ~SOCK_HIDDEN;
    bke::node_add_link(params.node_tree, group_input, *socket, params.node, params.socket);
  }
}

/**
 * \note This could use #search_link_ops_for_socket_templates, but we have to store the inputs and
 * outputs as IDProperties for assets because of asset indexing, so that's all we have without
 * loading the file.
 */
static void search_link_ops_for_asset_metadata(const bNodeTree &node_tree,
                                               const bNodeSocket &socket,
                                               const asset_system::AssetRepresentation &asset,
                                               Vector<SocketLinkOperation> &search_link_ops)
{
  const AssetMetaData &asset_data = asset.get_metadata();
  const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&asset_data, "type");
  if (tree_type == nullptr || IDP_int_get(tree_type) != node_tree.type) {
    return;
  }

  const bke::bNodeTreeType &node_tree_type = *node_tree.typeinfo;
  const eNodeSocketInOut in_out = socket.in_out == SOCK_OUT ? SOCK_IN : SOCK_OUT;

  const IDProperty *sockets = BKE_asset_metadata_idprop_find(
      &asset_data, in_out == SOCK_IN ? "inputs" : "outputs");

  int weight = -1;
  Set<StringRef> socket_names;
  LISTBASE_FOREACH (IDProperty *, socket_property, &sockets->data.group) {
    if (socket_property->type != IDP_STRING) {
      continue;
    }
    const char *socket_idname = IDP_string_get(socket_property);
    const bke::bNodeSocketType *socket_type = bke::node_socket_type_find(socket_idname);
    if (socket_type == nullptr) {
      continue;
    }
    eNodeSocketDatatype from = eNodeSocketDatatype(socket.type);
    eNodeSocketDatatype to = socket_type->type;
    if (socket.in_out == SOCK_OUT) {
      std::swap(from, to);
    }
    if (node_tree_type.validate_link && !node_tree_type.validate_link(from, to)) {
      continue;
    }
    if (!socket_names.add(socket_property->name)) {
      /* See comment in #search_link_ops_for_declarations. */
      continue;
    }

    const StringRef asset_name = asset.get_name();
    const StringRef socket_name = socket_property->name;

    search_link_ops.append(
        {asset_name + " " + UI_MENU_ARROW_SEP + socket_name,
         [&asset, socket_property, in_out](nodes::LinkSearchOpParams &params) {
           Main &bmain = *CTX_data_main(&params.C);

           bNode &node = params.add_node(params.node_tree.typeinfo->group_idname);

           bNodeTree *group = reinterpret_cast<bNodeTree *>(
               asset::asset_local_id_ensure_imported(bmain, asset));
           node.id = &group->id;
           id_us_plus(node.id);
           BKE_ntree_update_tag_node_property(&params.node_tree, &node);
           DEG_relations_tag_update(&bmain);

           node.flag &= ~NODE_OPTIONS;
           node.width = group->default_group_node_width;

           /* Create the inputs and outputs on the new node. */
           nodes::update_node_declaration_and_sockets(params.node_tree, node);

           bNodeSocket *new_node_socket = bke::node_find_enabled_socket(
               node, in_out, socket_property->name);
           if (new_node_socket != nullptr) {
             /* Rely on the way #node_add_link switches in/out if necessary. */
             bke::node_add_link(
                 params.node_tree, params.node, params.socket, node, *new_node_socket);
           }
         },
         weight});

    weight--;
  }
}

static void gather_search_link_ops_for_all_assets(const bContext &C,
                                                  const bNodeTree &node_tree,
                                                  const bNodeSocket &socket,
                                                  Vector<SocketLinkOperation> &search_link_ops)
{
  const AssetLibraryReference library_ref = asset_system::all_library_reference();
  asset::AssetFilterSettings filter_settings{};
  filter_settings.id_types = FILTER_ID_NT;

  asset::list::storage_fetch(&library_ref, &C);
  asset::list::iterate(library_ref, [&](asset_system::AssetRepresentation &asset) {
    if (!asset::filter_matches_asset(&filter_settings, asset)) {
      return true;
    }
    search_link_ops_for_asset_metadata(node_tree, socket, asset, search_link_ops);
    return true;
  });
}

/**
 * Call the callback to gather compatible socket connections for all node types, and the operations
 * that will actually make the connections. Also add some custom operations like connecting a group
 * output node.
 */
static void gather_socket_link_operations(const bContext &C,
                                          bNodeTree &node_tree,
                                          const bNodeSocket &socket,
                                          Vector<SocketLinkOperation> &search_link_ops)
{
  const SpaceNode &snode = *CTX_wm_space_node(&C);
  for (const bke::bNodeType *node_type : bke::node_types_get()) {
    const char *disabled_hint;
    if (node_type->poll && !node_type->poll(node_type, &node_tree, &disabled_hint)) {
      continue;
    }
    if (node_type->add_ui_poll && !node_type->add_ui_poll(&C)) {
      continue;
    }
    if (StringRefNull(node_type->ui_name).endswith("(Legacy)")) {
      continue;
    }
    if (node_type->gather_link_search_ops) {
      nodes::GatherLinkSearchOpParams params{
          *node_type, snode, node_tree, socket, search_link_ops};
      node_type->gather_link_search_ops(params);
    }
  }

  search_link_ops.append({IFACE_("Reroute"), add_reroute_node_fn});

  const bool is_node_group = !(node_tree.id.flag & ID_FLAG_EMBEDDED_DATA);

  if (is_node_group && socket.in_out == SOCK_IN) {
    search_link_ops.append({IFACE_("Group Input"), add_group_input_node_fn});

    int weight = -1;
    node_tree.tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
      if (item.item_type != NODE_INTERFACE_SOCKET) {
        return true;
      }
      const bNodeTreeInterfaceSocket &interface_socket =
          reinterpret_cast<const bNodeTreeInterfaceSocket &>(item);
      if (!(interface_socket.flag & NODE_INTERFACE_SOCKET_INPUT)) {
        return true;
      }
      {
        const bke::bNodeSocketType *from_typeinfo = bke::node_socket_type_find(
            interface_socket.socket_type);
        const eNodeSocketDatatype from = from_typeinfo ? from_typeinfo->type : SOCK_CUSTOM;
        const eNodeSocketDatatype to = socket.typeinfo->type;
        if (node_tree.typeinfo->validate_link && !node_tree.typeinfo->validate_link(from, to)) {
          return true;
        }
      }
      search_link_ops.append({std::string(IFACE_("Group Input")) + " " + UI_MENU_ARROW_SEP +
                                  (interface_socket.name ? interface_socket.name : ""),
                              [interface_socket](nodes::LinkSearchOpParams &params) {
                                add_existing_group_input_fn(params, interface_socket);
                              },
                              weight});
      weight--;
      return true;
    });
  }

  gather_search_link_ops_for_all_assets(C, node_tree, socket, search_link_ops);
}

static void link_drag_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  LinkDragSearchStorage &storage = *static_cast<LinkDragSearchStorage *>(arg);
  if (storage.update_items_tag) {
    bNodeTree *node_tree = CTX_wm_space_node(C)->edittree;
    storage.search_link_ops.clear();
    gather_socket_link_operations(*C, *node_tree, storage.from_socket, storage.search_link_ops);
    storage.update_items_tag = false;
  }

  ui::string_search::StringSearch<SocketLinkOperation> search{
      string_search::MainWordsHeuristic::All};

  for (SocketLinkOperation &op : storage.search_link_ops) {
    search.add(op.name, &op, op.weight);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str;
  const Vector<SocketLinkOperation *> filtered_items = search.query(string);

  for (SocketLinkOperation *item : filtered_items) {
    if (!UI_search_item_add(items, item->name, item, ICON_NONE, 0, 0)) {
      break;
    }
  }
}

static bNode *get_new_linked_node(bNodeSocket &socket, const Span<bNode *> new_nodes)
{
  for (const bNodeLink *link : socket.directly_linked_links()) {
    if (new_nodes.contains(link->fromnode)) {
      return link->fromnode;
    }
    if (new_nodes.contains(link->tonode)) {
      return link->tonode;
    }
  }
  return nullptr;
}

static void link_drag_search_exec_fn(bContext *C, void *arg1, void *arg2)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  LinkDragSearchStorage &storage = *static_cast<LinkDragSearchStorage *>(arg1);
  SocketLinkOperation *item = static_cast<SocketLinkOperation *>(arg2);
  if (item == nullptr) {
    return;
  }

  node_deselect_all(node_tree);

  Vector<bNode *> new_nodes;
  nodes::LinkSearchOpParams params{
      *C, node_tree, storage.from_node, storage.from_socket, new_nodes};
  item->fn(params);
  if (new_nodes.is_empty()) {
    return;
  }

  /* Used to position the new nodes where the cursor is. */
  const float2 cursor_offset = (storage.cursor / UI_SCALE_FAC) + float2(0.0f, 20.0f);

  /* Used to position the new nodes so that the newly linked socket is aligned to the cursor. */
  float2 link_offset{};
  node_tree.ensure_topology_cache();
  if (bNode *new_directly_linked_node = get_new_linked_node(storage.from_socket, new_nodes)) {
    link_offset -= new_directly_linked_node->location;
    if (storage.in_out() == SOCK_IN) {
      link_offset.x -= new_directly_linked_node->width;
    }
  }

  const float2 offset_in_tree = cursor_offset + link_offset;
  for (bNode *new_node : new_nodes) {
    /* The node may have an initial offset already, so use +=. */
    new_node->location[0] += offset_in_tree.x;
    new_node->location[1] += offset_in_tree.y;
    bke::node_set_selected(*new_node, true);
  }
  bke::node_set_active(node_tree, *new_nodes[0]);

  /* Ideally it would be possible to tag the node tree in some way so it updates only after the
   * translate operation is finished, but normally moving nodes around doesn't cause updates. */
  BKE_main_ensure_invariants(bmain, node_tree.id);

  /* Start translation operator with the new node. */
  wmOperatorType *ot = WM_operatortype_find("NODE_OT_translate_attach_remove_on_cancel", true);
  BLI_assert(ot);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

static void link_drag_search_free_fn(void *arg)
{
  LinkDragSearchStorage *storage = static_cast<LinkDragSearchStorage *>(arg);
  delete storage;
}

static uiBlock *create_search_popup_block(bContext *C, ARegion *region, void *arg_op)
{
  LinkDragSearchStorage &storage = *(LinkDragSearchStorage *)arg_op;

  uiBlock *block = UI_block_begin(C, region, "_popup", ui::EmbossType::Emboss);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiBut *but = uiDefSearchBut(block,
                              storage.search,
                              0,
                              ICON_VIEWZOOM,
                              sizeof(storage.search),
                              storage.in_out() == SOCK_OUT ? 10 : 10 - UI_searchbox_size_x(),
                              0,
                              UI_searchbox_size_x(),
                              UI_UNIT_Y,
                              "");
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set_listen(but, link_drag_search_listen_fn);
  UI_but_func_search_set(but,
                         nullptr,
                         link_drag_search_update_fn,
                         &storage,
                         false,
                         link_drag_search_free_fn,
                         link_drag_search_exec_fn,
                         nullptr);
  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* Fake button to hold space for the search items. */
  uiDefBut(block,
           ButType::Label,
           0,
           "",
           storage.in_out() == SOCK_OUT ? 10 : 10 - UI_searchbox_size_x(),
           10 - UI_searchbox_size_y(),
           UI_searchbox_size_x(),
           UI_searchbox_size_y(),
           nullptr,
           0,
           0,
           std::nullopt);

  const int2 offset = {0, -UI_UNIT_Y};
  UI_block_bounds_set_popup(block, 0.3f * U.widget_unit, offset);
  return block;
}

void invoke_node_link_drag_add_menu(bContext &C,
                                    bNode &node,
                                    bNodeSocket &socket,
                                    const float2 &cursor)
{
  LinkDragSearchStorage *storage = new LinkDragSearchStorage{node, socket, cursor};
  /* Use the "_ex" variant with `can_refresh` false to avoid a double free when closing Blender. */
  UI_popup_block_invoke_ex(&C, create_search_popup_block, storage, nullptr, false);
}

}  // namespace blender::ed::space_node
