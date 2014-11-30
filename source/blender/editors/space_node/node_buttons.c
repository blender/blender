/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_buttons.c
 *  \ingroup spnode
 */

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BLF_translation.h"

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

#include "node_intern.h"  /* own include */


/* ******************* node space & buttons ************** */

#if 0
/* poll for active nodetree */
static int active_nodetree_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	return (snode && snode->nodetree);
}
#endif

static int node_sockets_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	return (snode && snode->nodetree && G.debug_value == 777);
}

static void node_sockets_panel(const bContext *C, Panel *pa)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = (snode) ? snode->edittree : NULL;
	bNode *node = (ntree) ? nodeGetActive(ntree) : NULL;
	bNodeSocket *sock;
	uiLayout *layout = pa->layout, *split;
	char name[UI_MAX_NAME_STR];
	
	if (ELEM(NULL, ntree, node))
		return;
	
	for (sock = node->inputs.first; sock; sock = sock->next) {
		BLI_snprintf(name, sizeof(name), "%s:", sock->name);

		split = uiLayoutSplit(layout, 0.35f, false);
		uiItemL(split, name, ICON_NONE);
		uiTemplateNodeLink(split, ntree, node, sock);
	}
}

static int node_tree_interface_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	return (snode && snode->edittree && (snode->edittree->inputs.first || snode->edittree->outputs.first));
}

static bool node_tree_find_active_socket(bNodeTree *ntree, bNodeSocket **r_sock, int *r_in_out)
{
	bNodeSocket *sock;
	for (sock = ntree->inputs.first; sock; sock = sock->next) {
		if (sock->flag & SELECT) {
			*r_sock = sock;
			*r_in_out = SOCK_IN;
			return true;
		}
	}
	for (sock = ntree->outputs.first; sock; sock = sock->next) {
		if (sock->flag & SELECT) {
			*r_sock = sock;
			*r_in_out = SOCK_OUT;
			return true;
		}
	}
	
	*r_sock = NULL;
	*r_in_out = 0;
	return false;
}

static void node_tree_interface_panel(const bContext *C, Panel *pa)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = (snode) ? snode->edittree : NULL;
	bNodeSocket *sock;
	int in_out;
	uiLayout *layout = pa->layout, *row, *split, *col;
	PointerRNA ptr, sockptr, opptr;

	if (!ntree)
		return;
	
	RNA_id_pointer_create((ID *)ntree, &ptr);
	
	node_tree_find_active_socket(ntree, &sock, &in_out);
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, sock, &sockptr);
	
	row = uiLayoutRow(layout, false);
	
	split = uiLayoutRow(row, true);
	col = uiLayoutColumn(split, true);
	uiItemL(col, IFACE_("Inputs:"), ICON_NONE);
	uiTemplateList(col, (bContext *)C, "NODE_UL_interface_sockets", "inputs", &ptr, "inputs", &ptr, "active_input",
	               0, 0, 0, 0);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_add", "", ICON_PLUS, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "in_out", SOCK_IN);
	
	col = uiLayoutColumn(split, true);
	uiItemL(col, IFACE_("Outputs:"), ICON_NONE);
	uiTemplateList(col, (bContext *)C, "NODE_UL_interface_sockets", "outputs", &ptr, "outputs", &ptr, "active_output",
	               0, 0, 0, 0);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_add", "", ICON_PLUS, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "in_out", SOCK_OUT);
	
	col = uiLayoutColumn(row, true);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_move", "", ICON_TRIA_UP, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "direction", 1);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_move", "", ICON_TRIA_DOWN, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "direction", 2);
	
	if (sock) {
		row = uiLayoutRow(layout, true);
		uiItemR(row, &sockptr, "name", 0, NULL, ICON_NONE);
		uiItemO(row, "", ICON_X, "NODE_OT_tree_socket_remove");
		
		if (sock->typeinfo->interface_draw) {
			uiItemS(layout);
			sock->typeinfo->interface_draw((bContext *)C, layout, &sockptr);
		}
	}
}

/* ******************* node buttons registration ************** */

void node_buttons_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype node panel node sockets");
	strcpy(pt->idname, "NODE_PT_sockets");
	strcpy(pt->label, N_("Sockets"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = node_sockets_panel;
	pt->poll = node_sockets_poll;
	pt->flag |= PNL_DEFAULT_CLOSED;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype node panel tree interface");
	strcpy(pt->idname, "NODE_PT_node_tree_interface");
	strcpy(pt->label, N_("Interface"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = node_tree_interface_panel;
	pt->poll = node_tree_interface_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int node_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = node_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

/* non-standard poll operator which doesn't care if there are any nodes */
static int node_properties_poll(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	return (sa && (sa->spacetype == SPACE_NODE));
}

void NODE_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->description = "Toggles the properties panel display";
	ot->idname = "NODE_OT_properties";
	
	ot->exec = node_properties_toggle_exec;
	ot->poll = node_properties_poll;
	
	/* flags */
	ot->flag = 0;
}
