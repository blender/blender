/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_listbase.h"
#include "BLI_string_search.h"

#include "DNA_space_types.h"

#include "BKE_context.h"

#include "NOD_socket_search_link.hh"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "WM_api.h"

#include "ED_node.h"

#include "node_intern.hh"

using blender::nodes::SocketLinkOperation;

namespace blender::ed::space_node {

struct LinkDragSearchStorage {
  bNode &from_node;
  bNodeSocket &from_socket;
  float2 cursor;
  Vector<SocketLinkOperation> search_link_ops;
  char search[256];

  eNodeSocketInOut in_out() const
  {
    return static_cast<eNodeSocketInOut>(from_socket.in_out);
  }
};

static void add_reroute_node_fn(nodes::LinkSearchOpParams &params)
{
  bNode &reroute = params.add_node("NodeReroute");
  if (params.socket.in_out == SOCK_IN) {
    nodeAddLink(&params.node_tree,
                &reroute,
                static_cast<bNodeSocket *>(reroute.outputs.first),
                &params.node,
                &params.socket);
  }
  else {
    nodeAddLink(&params.node_tree,
                &params.node,
                &params.socket,
                &reroute,
                static_cast<bNodeSocket *>(reroute.inputs.first));
  }
}

static void add_group_input_node_fn(nodes::LinkSearchOpParams &params)
{
  /* Add a group input based on the connected socket, and add a new group input node. */
  bNodeSocket *interface_socket = ntreeAddSocketInterfaceFromSocket(
      &params.node_tree, &params.node, &params.socket);
  const int group_input_index = BLI_findindex(&params.node_tree.inputs, interface_socket);

  bNode &group_input = params.add_node("NodeGroupInput");

  /* This is necessary to create the new sockets in the other input nodes. */
  ED_node_tree_propagate_change(&params.C, CTX_data_main(&params.C), &params.node_tree);

  /* Hide the new input in all other group input nodes, to avoid making them taller. */
  LISTBASE_FOREACH (bNode *, node, &params.node_tree.nodes) {
    if (node->type == NODE_GROUP_INPUT) {
      bNodeSocket *new_group_input_socket = (bNodeSocket *)BLI_findlink(&node->outputs,
                                                                        group_input_index);
      new_group_input_socket->flag |= SOCK_HIDDEN;
    }
  }

  /* Hide all existing inputs in the new group input node, to only display the new one. */
  LISTBASE_FOREACH (bNodeSocket *, socket, &group_input.outputs) {
    socket->flag |= SOCK_HIDDEN;
  }

  bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&group_input.outputs, group_input_index);
  if (socket == nullptr) {
    /* Adding sockets can fail in some cases. There's no good reason not to be safe here. */
    return;
  }
  /* Unhide the socket for the new input in the new node and make a connection to it. */
  socket->flag &= ~SOCK_HIDDEN;
  nodeAddLink(&params.node_tree, &group_input, socket, &params.node, &params.socket);
}

static void add_existing_group_input_fn(nodes::LinkSearchOpParams &params,
                                        const bNodeSocket &interface_socket)
{
  const int group_input_index = BLI_findindex(&params.node_tree.inputs, &interface_socket);
  bNode &group_input = params.add_node("NodeGroupInput");

  LISTBASE_FOREACH (bNodeSocket *, socket, &group_input.outputs) {
    socket->flag |= SOCK_HIDDEN;
  }

  bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&group_input.outputs, group_input_index);
  if (socket == nullptr) {
    /* Adding sockets can fail in some cases. There's no good reason not to be safe here. */
    return;
  }

  socket->flag &= ~SOCK_HIDDEN;
  nodeAddLink(&params.node_tree, &group_input, socket, &params.node, &params.socket);
}

/**
 * Call the callback to gather compatible socket connections for all node types, and the operations
 * that will actually make the connections. Also add some custom operations like connecting a group
 * output node.
 */
static void gather_socket_link_operations(bNodeTree &node_tree,
                                          const bNodeSocket &socket,
                                          Vector<SocketLinkOperation> &search_link_ops)
{
  NODE_TYPES_BEGIN (node_type) {
    if (StringRef(node_type->idname).find("Legacy") != StringRef::not_found) {
      continue;
    }
    const char *disabled_hint;
    if (!(node_type->poll && node_type->poll(node_type, &node_tree, &disabled_hint))) {
      continue;
    }

    if (node_type->gather_link_search_ops) {
      nodes::GatherLinkSearchOpParams params{*node_type, node_tree, socket, search_link_ops};
      node_type->gather_link_search_ops(params);
    }
  }
  NODE_TYPES_END;

  search_link_ops.append({IFACE_("Reroute"), add_reroute_node_fn});

  const bool is_node_group = !(node_tree.id.flag & LIB_EMBEDDED_DATA);

  if (is_node_group && socket.in_out == SOCK_IN) {
    search_link_ops.append({IFACE_("Group Input"), add_group_input_node_fn});

    int weight = -1;
    LISTBASE_FOREACH (const bNodeSocket *, interface_socket, &node_tree.inputs) {
      eNodeSocketDatatype from = (eNodeSocketDatatype)interface_socket->type;
      eNodeSocketDatatype to = (eNodeSocketDatatype)socket.type;
      if (node_tree.typeinfo->validate_link && !node_tree.typeinfo->validate_link(from, to)) {
        continue;
      }
      search_link_ops.append(
          {std::string(IFACE_("Group Input ")) + UI_MENU_ARROW_SEP + interface_socket->name,
           [interface_socket](nodes::LinkSearchOpParams &params) {
             add_existing_group_input_fn(params, *interface_socket);
           },
           weight});
      weight--;
    }
  }
}

static void link_drag_search_update_fn(const bContext *UNUSED(C),
                                       void *arg,
                                       const char *str,
                                       uiSearchItems *items,
                                       const bool is_first)
{
  LinkDragSearchStorage &storage = *static_cast<LinkDragSearchStorage *>(arg);

  StringSearch *search = BLI_string_search_new();

  for (SocketLinkOperation &op : storage.search_link_ops) {
    BLI_string_search_add(search, op.name.c_str(), &op, op.weight);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str;
  SocketLinkOperation **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, string, (void ***)&filtered_items);

  for (const int i : IndexRange(filtered_amount)) {
    SocketLinkOperation &item = *filtered_items[i];
    if (!UI_search_item_add(items, item.name.c_str(), &item, ICON_NONE, 0, 0)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

static void link_drag_search_exec_fn(bContext *C, void *arg1, void *arg2)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  LinkDragSearchStorage &storage = *static_cast<LinkDragSearchStorage *>(arg1);
  SocketLinkOperation *item = static_cast<SocketLinkOperation *>(arg2);
  if (item == nullptr) {
    return;
  }

  node_deselect_all(snode);

  Vector<bNode *> new_nodes;
  nodes::LinkSearchOpParams params{
      *C, *snode.edittree, storage.from_node, storage.from_socket, new_nodes};
  item->fn(params);
  if (new_nodes.is_empty()) {
    return;
  }

  /* For now, assume that only one node is created by the callback. */
  BLI_assert(new_nodes.size() == 1);
  bNode *new_node = new_nodes.first();

  new_node->locx = storage.cursor.x / UI_DPI_FAC;
  new_node->locy = storage.cursor.y / UI_DPI_FAC + 20 * UI_DPI_FAC;
  if (storage.in_out() == SOCK_IN) {
    new_node->locx -= new_node->width;
  }

  nodeSetSelected(new_node, true);
  nodeSetActive(snode.edittree, new_node);

  /* Ideally it would be possible to tag the node tree in some way so it updates only after the
   * translate operation is finished, but normally moving nodes around doesn't cause updates. */
  ED_node_tree_propagate_change(C, &bmain, snode.edittree);

  /* Start translation operator with the new node. */
  wmOperatorType *ot = WM_operatortype_find("TRANSFORM_OT_translate", true);
  BLI_assert(ot);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_boolean_set(&ptr, "view2d_edge_pan", true);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr);
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

  bNodeTree *node_tree = CTX_wm_space_node(C)->nodetree;
  gather_socket_link_operations(*node_tree, storage.from_socket, storage.search_link_ops);

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiBut *but = uiDefSearchBut(block,
                              storage.search,
                              0,
                              ICON_VIEWZOOM,
                              sizeof(storage.search),
                              storage.in_out() == SOCK_OUT ? 10 : 10 - UI_searchbox_size_x(),
                              10,
                              UI_searchbox_size_x(),
                              UI_UNIT_Y,
                              0,
                              0,
                              "");
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
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
           UI_BTYPE_LABEL,
           0,
           "",
           storage.in_out() == SOCK_OUT ? 10 : 10 - UI_searchbox_size_x(),
           10 - UI_searchbox_size_y(),
           UI_searchbox_size_x(),
           UI_searchbox_size_y(),
           nullptr,
           0,
           0,
           0,
           0,
           nullptr);

  const int offset[2] = {0, -UI_UNIT_Y};
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
