/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_library.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "NOD_socket.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_node.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

static bool node_tree_interface_panel_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode) {
    return false;
  }
  bNodeTree *ntree = snode->edittree;
  if (!ntree) {
    return false;
  }
  if (ntree->flag & ID_FLAG_EMBEDDED_DATA) {
    return false;
  }
  if (ntree->typeinfo->no_group_interface) {
    return false;
  }
  return true;
}

void node_tree_interface_draw(bContext &C, uiLayout &layout, bNodeTree &tree)
{
  PointerRNA tree_ptr = RNA_pointer_create_discrete(&tree.id, &RNA_NodeTree, &tree);
  PointerRNA interface_ptr = RNA_pointer_get(&tree_ptr, "interface");

  {
    uiLayout &row = layout.row(false);
    uiTemplateNodeTreeInterface(&row, &C, &interface_ptr);

    uiLayout &col = row.column(true);
    col.enabled_set(ID_IS_EDITABLE(&tree.id));
    col.menu("NODE_MT_node_tree_interface_new_item", "", ICON_ADD);
    col.op("node.interface_item_remove", "", ICON_REMOVE);
    col.separator();
    col.menu("NODE_MT_node_tree_interface_context_menu", "", ICON_DOWNARROW_HLT);
  }

  bNodeTreeInterfaceItem *active_item = tree.tree_interface.active_item();
  if (!active_item) {
    return;
  }
  PointerRNA active_item_ptr = RNA_pointer_get(&interface_ptr, "active");

  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);

  if (active_item->item_type == NODE_INTERFACE_SOCKET) {
    bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(active_item);
    const bke::bNodeSocketType *stype = socket->socket_typeinfo();
    layout.prop(&active_item_ptr, "socket_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);
    layout.prop(&active_item_ptr, "description", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (tree.type == NTREE_GEOMETRY) {
      if (nodes::socket_type_supports_fields(stype->type) && stype->type != SOCK_MENU) {
        if (socket->flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          layout.prop(&active_item_ptr, "attribute_domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        }
        layout.prop(
            &active_item_ptr, "default_attribute_name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
    }
    if (stype->interface_draw) {
      stype->interface_draw(&tree.id, socket, &C, &layout);
    }
  }
  if (active_item->item_type == NODE_INTERFACE_PANEL) {
    bNodeTreeInterfacePanel *panel_item = reinterpret_cast<bNodeTreeInterfacePanel *>(active_item);
    layout.prop(&active_item_ptr, "description", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout.prop(
        &active_item_ptr, "default_closed", UI_ITEM_NONE, IFACE_("Closed by Default"), ICON_NONE);

    if (bNodeTreeInterfaceSocket *panel_toggle_socket = panel_item->header_toggle_socket()) {
      if (uiLayout *panel = layout.panel(&C, "panel_toggle", false, IFACE_("Panel Toggle"))) {
        PointerRNA panel_toggle_socket_ptr = RNA_pointer_create_discrete(
            &tree.id, &RNA_NodeTreeInterfaceSocket, panel_toggle_socket);
        panel->prop(
            &panel_toggle_socket_ptr, "default_value", UI_ITEM_NONE, IFACE_("Default"), ICON_NONE);
        uiLayout &col = panel->column(false);
        col.prop(
            &panel_toggle_socket_ptr, "hide_in_modifier", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        col.prop(
            &panel_toggle_socket_ptr, "force_non_field", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
    }
  }
}

static void node_tree_interface_panel_draw(const bContext *C, Panel *panel)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &tree = *snode.edittree;
  uiLayout &layout = *panel->layout;

  node_tree_interface_draw(const_cast<bContext &>(*C), layout, tree);
}

void node_tree_interface_panel_register(ARegionType *art)
{
  PanelType *pt = MEM_callocN<PanelType>("NODE_PT_node_tree_interface");
  STRNCPY_UTF8(pt->idname, "NODE_PT_node_tree_interface");
  STRNCPY_UTF8(pt->label, N_("Group Sockets"));
  STRNCPY_UTF8(pt->category, "Group");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = node_tree_interface_panel_draw;
  pt->poll = node_tree_interface_panel_poll;
  pt->order = 10;
  BLI_addtail(&art->paneltypes, pt);
}

}  // namespace blender::ed::space_node
