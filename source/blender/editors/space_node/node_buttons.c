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

static bNodeSocket *node_tree_find_active_socket(bNodeTree *ntree, const eNodeSocketInOut in_out)
{
  ListBase *sockets = (in_out == SOCK_IN) ? &ntree->inputs : &ntree->outputs;
  LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
    if (socket->flag & SELECT) {
      return socket;
    }
  }
  return NULL;
}

static void draw_socket_list(const bContext *C,
                             uiLayout *layout,
                             bNodeTree *ntree,
                             const eNodeSocketInOut in_out)
{
  PointerRNA tree_ptr;
  RNA_id_pointer_create((ID *)ntree, &tree_ptr);

  uiLayout *split = uiLayoutRow(layout, false);
  uiLayout *list_col = uiLayoutColumn(split, true);
  uiTemplateList(list_col,
                 (bContext *)C,
                 "NODE_UL_interface_sockets",
                 (in_out == SOCK_IN) ? "inputs" : "outputs",
                 &tree_ptr,
                 (in_out == SOCK_IN) ? "inputs" : "outputs",
                 &tree_ptr,
                 (in_out == SOCK_IN) ? "active_input" : "active_output",
                 NULL,
                 0,
                 0,
                 0,
                 0,
                 false,
                 false);
  PointerRNA opptr;
  uiLayout *ops_col = uiLayoutColumn(split, false);
  uiLayout *add_remove_col = uiLayoutColumn(ops_col, true);
  wmOperatorType *ot = WM_operatortype_find("NODE_OT_tree_socket_add", false);
  uiItemFullO_ptr(add_remove_col, ot, "", ICON_ADD, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "in_out", in_out);
  ot = WM_operatortype_find("NODE_OT_tree_socket_remove", false);
  uiItemFullO_ptr(add_remove_col, ot, "", ICON_REMOVE, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "in_out", in_out);

  uiItemS(ops_col);

  uiLayout *up_down_col = uiLayoutColumn(ops_col, true);
  ot = WM_operatortype_find("NODE_OT_tree_socket_move", false);
  uiItemFullO_ptr(up_down_col, ot, "", ICON_TRIA_UP, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "direction", 1);
  RNA_enum_set(&opptr, "in_out", in_out);
  uiItemFullO_ptr(up_down_col, ot, "", ICON_TRIA_DOWN, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "direction", 2);
  RNA_enum_set(&opptr, "in_out", in_out);

  bNodeSocket *socket = node_tree_find_active_socket(ntree, in_out);
  if (socket != NULL) {
    uiLayoutSetPropSep(layout, true);
    uiLayoutSetPropDecorate(layout, false);
    PointerRNA socket_ptr;
    RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, socket, &socket_ptr);
    uiItemR(layout, &socket_ptr, "name", 0, NULL, ICON_NONE);

    /* Display descriptions only for Geometry Nodes, since it's only used in the modifier panel. */
    if (ntree->type == NTREE_GEOMETRY) {
      uiItemR(layout, &socket_ptr, "description", 0, NULL, ICON_NONE);
    }

    if (socket->typeinfo->interface_draw) {
      socket->typeinfo->interface_draw((bContext *)C, layout, &socket_ptr);
    }
  }
}

static void node_tree_interface_inputs_panel(const bContext *C, Panel *panel)
{
  SpaceNode *snode = CTX_wm_space_node(C); /* NULL checked in poll function. */
  bNodeTree *ntree = snode->edittree;      /* NULL checked in poll function. */

  draw_socket_list(C, panel->layout, ntree, SOCK_IN);
}

static void node_tree_interface_outputs_panel(const bContext *C, Panel *panel)
{
  SpaceNode *snode = CTX_wm_space_node(C); /* NULL checked in poll function. */
  bNodeTree *ntree = snode->edittree;      /* NULL checked in poll function. */

  draw_socket_list(C, panel->layout, ntree, SOCK_OUT);
}

/* ******************* node buttons registration ************** */

void node_buttons_register(ARegionType *art)
{
  {
    PanelType *pt = MEM_callocN(sizeof(PanelType), __func__);
    strcpy(pt->idname, "NODE_PT_sockets");
    strcpy(pt->category, N_("Node"));
    strcpy(pt->label, N_("Sockets"));
    strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    pt->draw = node_sockets_panel;
    pt->poll = node_sockets_poll;
    pt->flag |= PANEL_TYPE_DEFAULT_CLOSED;
    BLI_addtail(&art->paneltypes, pt);
  }

  {
    PanelType *pt = MEM_callocN(sizeof(PanelType), __func__);
    strcpy(pt->idname, "NODE_PT_node_tree_interface_inputs");
    strcpy(pt->category, N_("Group"));
    strcpy(pt->label, N_("Inputs"));
    strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    pt->draw = node_tree_interface_inputs_panel;
    pt->poll = node_tree_interface_poll;
    BLI_addtail(&art->paneltypes, pt);
  }
  {
    PanelType *pt = MEM_callocN(sizeof(PanelType), __func__);
    strcpy(pt->idname, "NODE_PT_node_tree_interface_outputs");
    strcpy(pt->category, N_("Group"));
    strcpy(pt->label, N_("Outputs"));
    strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    pt->draw = node_tree_interface_outputs_panel;
    pt->poll = node_tree_interface_poll;
    BLI_addtail(&art->paneltypes, pt);
  }
}
