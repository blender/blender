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

/* poll for active nodetree */
static int active_nodetree_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	return (snode && ntreeIsValid(snode->nodetree));
}

/* poll callback for active node */
static int active_node_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	return (snode && ntreeIsValid(snode->edittree) && nodeGetActive(snode->edittree));
}

/* active node */
static void active_node_panel(const bContext *C, Panel *pa)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = (snode) ? snode->edittree : NULL;
	bNode *node = (ntree) ? nodeGetActive(ntree) : NULL; // xxx... for editing group nodes
	uiLayout *layout, *row, *col, *sub;
	PointerRNA ptr, opptr;
	
	/* verify pointers, and create RNA pointer for the node */
	if (ELEM(NULL, ntree, node))
		return;
	//if (node->id) /* for group nodes */
	//	RNA_pointer_create(node->id, &RNA_Node, node, &ptr);
	//else
	RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
	
	layout = uiLayoutColumn(pa->layout, FALSE);
	uiLayoutSetContextPointer(layout, "node", &ptr);
	
	/* draw this node's name, etc. */
	uiItemR(layout, &ptr, "label", 0, NULL, ICON_NODE);
	uiItemS(layout);
	uiItemR(layout, &ptr, "name", 0, NULL, ICON_NODE);
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "NODE_OT_hide_socket_toggle");
	uiItemS(layout);
	uiItemS(layout);

	row = uiLayoutRow(layout, FALSE);
	
	col = uiLayoutColumn(row, TRUE);
	uiItemM(col, (bContext *)C, "NODE_MT_node_color_presets", NULL, 0);
	uiItemR(col, &ptr, "use_custom_color", UI_ITEM_R_ICON_ONLY, NULL, ICON_NONE);
	sub = uiLayoutRow(col, FALSE);
	if (!(node->flag & NODE_CUSTOM_COLOR))
		uiLayoutSetEnabled(sub, FALSE);
	uiItemR(sub, &ptr, "color", 0, "", ICON_NONE);
	
	col = uiLayoutColumn(row, TRUE);
	uiItemO(col, "", ICON_ZOOMIN, "node.node_color_preset_add");
	opptr = uiItemFullO(col, "node.node_color_preset_add", "", ICON_ZOOMOUT, NULL, WM_OP_INVOKE_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_boolean_set(&opptr, "remove_active", 1);
	uiItemM(col, (bContext *)C, "NODE_MT_node_color_specials", "", ICON_DOWNARROW_HLT);
	
	/* draw this node's settings */
	if (node->typeinfo && node->typeinfo->uifuncbut) {
		uiItemS(layout);
		uiItemS(layout);
		node->typeinfo->uifuncbut(layout, (bContext *)C, &ptr);
	}
	else if (node->typeinfo && node->typeinfo->uifunc) {
		uiItemS(layout);
		uiItemS(layout);
		node->typeinfo->uifunc(layout, (bContext *)C, &ptr);
	}
}

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

		split = uiLayoutSplit(layout, 0.35f, FALSE);
		uiItemL(split, name, ICON_NONE);
		uiTemplateNodeLink(split, ntree, node, sock);
	}
}

static int node_tree_interface_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	return (snode && snode->edittree && (snode->edittree->inputs.first || snode->edittree->outputs.first));
}

static int node_tree_find_active_socket(bNodeTree *ntree, bNodeSocket **r_sock, int *r_in_out)
{
	bNodeSocket *sock;
	for (sock = ntree->inputs.first; sock; sock = sock->next) {
		if (sock->flag & SELECT) {
			*r_sock = sock;
			*r_in_out = SOCK_IN;
			return TRUE;
		}
	}
	for (sock = ntree->outputs.first; sock; sock = sock->next) {
		if (sock->flag & SELECT) {
			*r_sock = sock;
			*r_in_out = SOCK_OUT;
			return TRUE;
		}
	}
	
	*r_sock = NULL;
	*r_in_out = 0;
	return FALSE;
}

static void node_tree_interface_panel(const bContext *C, Panel *pa)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree= (snode) ? snode->edittree : NULL;
	bNodeSocket *sock;
	int in_out;
	uiLayout *layout= pa->layout, *row, *split, *col;
	PointerRNA ptr, sockptr, opptr;
	
	if(!ntree)
		return;
	
	RNA_id_pointer_create((ID *)ntree, &ptr);
	
	node_tree_find_active_socket(ntree, &sock, &in_out);
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, sock, &sockptr);
	
	row = uiLayoutRow(layout, FALSE);
	
	split = uiLayoutRow(row, TRUE);
	col = uiLayoutColumn(split, TRUE);
	uiItemL(col, "Inputs:", ICON_NONE);
	uiTemplateList(col, (bContext*)C, "NODE_UL_interface_sockets", "", &ptr, "inputs", &ptr, "active_input", 0, 0, 0);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_add", "", ICON_PLUS, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "in_out", SOCK_IN);
	
	col = uiLayoutColumn(split, TRUE);
	uiItemL(col, "Outputs:", ICON_NONE);
	uiTemplateList(col, (bContext*)C, "NODE_UL_interface_sockets", "", &ptr, "outputs", &ptr, "active_output", 0, 0, 0);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_add", "", ICON_PLUS, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "in_out", SOCK_OUT);
	
	col = uiLayoutColumn(row, TRUE);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_move", "", ICON_TRIA_UP, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "direction", 1);
	opptr = uiItemFullO(col, "NODE_OT_tree_socket_move", "", ICON_TRIA_DOWN, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "direction", 2);
	
	if (sock) {
		row = uiLayoutRow(layout, TRUE);
		uiItemR(row, &sockptr, "name", 0, NULL, ICON_NONE);
		uiItemO(row, "", ICON_X, "NODE_OT_tree_socket_remove");
		
		uiItemS(layout);
		
		if (sock->typeinfo->interface_draw)
			sock->typeinfo->interface_draw((bContext*)C, layout, &sockptr);
	}
}

/* ******************* node buttons registration ************** */

void node_buttons_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype node panel active node");
	strcpy(pt->idname, "NODE_PT_item");
	strcpy(pt->label, IFACE_("Active Node"));
	pt->draw = active_node_panel;
	pt->poll = active_node_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype node panel node sockets");
	strcpy(pt->idname, "NODE_PT_sockets");
	strcpy(pt->label, "Sockets");
	pt->draw = node_sockets_panel;
	pt->poll = node_sockets_poll;
	pt->flag |= PNL_DEFAULT_CLOSED;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype node panel tree interface");
	strcpy(pt->idname, "NODE_PT_node_tree_interface");
	strcpy(pt->label, "Interface");
	pt->draw= node_tree_interface_panel;
	pt->poll= node_tree_interface_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype node panel gpencil");
	strcpy(pt->idname, "NODE_PT_gpencil");
	strcpy(pt->label, "Grease Pencil");
	pt->draw_header = gpencil_panel_standard_header;
	pt->draw = gpencil_panel_standard;
	pt->poll = active_nodetree_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int node_properties(bContext *C, wmOperator *UNUSED(op))
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
	
	ot->exec = node_properties;
	ot->poll = node_properties_poll;
	
	/* flags */
	ot->flag = 0;
}
