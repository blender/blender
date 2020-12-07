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
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spnode
 */

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_gpencil.h"
#include "ED_screen.h"

#include "UI_resources.h"

#include "node_intern.h" /* own include */

/* ******************* node space & buttons ************** */

#if 0
/* poll for active nodetree */
static bool active_nodetree_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  return (snode && snode->nodetree);
}
#endif

static bool node_sockets_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  return (snode && snode->nodetree && G.debug_value == 777);
}

static void node_sockets_panel(const bContext *C, Panel *panel)
{
  SpaceNode *snode = CTX_wm_space_node(C); /* NULL checked in poll function. */
  bNodeTree *ntree = snode->edittree;      /* NULL checked in poll function. */
  bNode *node = nodeGetActive(ntree);
  if (node == NULL) {
    return;
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    char name[UI_MAX_NAME_STR];
    BLI_snprintf(name, sizeof(name), "%s:", socket->name);

    uiLayout *split = uiLayoutSplit(panel->layout, 0.35f, false);
    uiItemL(split, name, ICON_NONE);
    uiTemplateNodeLink(split, (bContext *)C, ntree, node, socket);
  }
}

static bool node_tree_interface_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceNode *snode = CTX_wm_space_node(C);

  return (snode && snode->edittree &&
          (snode->edittree->inputs.first || snode->edittree->outputs.first));
}

static bool node_tree_find_active_socket(bNodeTree *ntree,
                                         bNodeSocket **r_sock,
                                         eNodeSocketInOut *r_in_out)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->inputs) {
    if (socket->flag & SELECT) {
      *r_sock = socket;
      *r_in_out = SOCK_IN;
      return true;
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->outputs) {
    if (socket->flag & SELECT) {
      *r_sock = socket;
      *r_in_out = SOCK_OUT;
      return true;
    }
  }

  *r_sock = NULL;
  *r_in_out = 0;
  return false;
}

static void node_tree_interface_panel(const bContext *C, Panel *panel)
{
  SpaceNode *snode = CTX_wm_space_node(C); /* NULL checked in poll function. */
  bNodeTree *ntree = snode->edittree;      /* NULL checked in poll function. */
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  RNA_id_pointer_create((ID *)ntree, &ptr);

  bNodeSocket *socket;
  eNodeSocketInOut in_out;
  node_tree_find_active_socket(ntree, &socket, &in_out);
  PointerRNA sockptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, socket, &sockptr);

  uiLayout *row = uiLayoutRow(layout, false);

  uiLayout *split = uiLayoutRow(row, true);
  uiLayout *col = uiLayoutColumn(split, true);
  wmOperatorType *ot = WM_operatortype_find("NODE_OT_tree_socket_add", false);
  uiItemL(col, IFACE_("Inputs:"), ICON_NONE);
  uiTemplateList(col,
                 (bContext *)C,
                 "NODE_UL_interface_sockets",
                 "inputs",
                 &ptr,
                 "inputs",
                 &ptr,
                 "active_input",
                 NULL,
                 0,
                 0,
                 0,
                 0,
                 false,
                 false);
  PointerRNA opptr;
  uiItemFullO_ptr(col, ot, "", ICON_PLUS, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "in_out", SOCK_IN);

  col = uiLayoutColumn(split, true);
  uiItemL(col, IFACE_("Outputs:"), ICON_NONE);
  uiTemplateList(col,
                 (bContext *)C,
                 "NODE_UL_interface_sockets",
                 "outputs",
                 &ptr,
                 "outputs",
                 &ptr,
                 "active_output",
                 NULL,
                 0,
                 0,
                 0,
                 0,
                 false,
                 false);
  uiItemFullO_ptr(col, ot, "", ICON_PLUS, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "in_out", SOCK_OUT);

  ot = WM_operatortype_find("NODE_OT_tree_socket_move", false);
  col = uiLayoutColumn(row, true);
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_UP, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "direction", 1);
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_DOWN, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "direction", 2);

  if (socket) {
    row = uiLayoutRow(layout, true);
    uiItemR(row, &sockptr, "name", 0, NULL, ICON_NONE);
    uiItemO(row, "", ICON_X, "NODE_OT_tree_socket_remove");

    if (socket->typeinfo->interface_draw) {
      uiItemS(layout);
      socket->typeinfo->interface_draw((bContext *)C, layout, &sockptr);
    }
  }
}

/* ******************* node buttons registration ************** */

void node_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN(sizeof(PanelType), "spacetype node panel node sockets");
  strcpy(pt->idname, "NODE_PT_sockets");
  strcpy(pt->category, N_("Node"));
  strcpy(pt->label, N_("Sockets"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = node_sockets_panel;
  pt->poll = node_sockets_poll;
  pt->flag |= PANEL_TYPE_DEFAULT_CLOSED;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype node panel tree interface");
  strcpy(pt->idname, "NODE_PT_node_tree_interface");
  strcpy(pt->category, N_("Node"));
  strcpy(pt->label, N_("Interface"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = node_tree_interface_panel;
  pt->poll = node_tree_interface_poll;
  BLI_addtail(&art->paneltypes, pt);
}
