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


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_gpencil.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_intern.h"	// own include


/* ******************* node space & buttons ************** */
#define B_NOP		1
#define B_REDR		2

static void do_node_region_buttons(bContext *C, void *UNUSED(arg), int event)
{
	//SpaceNode *snode= CTX_wm_space_node(C);
	
	switch(event) {
	case B_REDR:
		ED_area_tag_redraw(CTX_wm_area(C));
		return; /* no notifier! */
	}
}

/* poll callback for active node */
static int active_node_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	// TODO: include check for whether there is an active node...
	return (snode && snode->nodetree);
}

/* active node */
static void active_node_panel(const bContext *C, Panel *pa)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree= (snode) ? snode->edittree : NULL;
	bNode *node = (ntree) ? nodeGetActive(ntree) : NULL; // xxx... for editing group nodes
	uiLayout *layout= pa->layout;
	uiBlock *block;
	PointerRNA ptr;
	
	/* verify pointers, and create RNA pointer for the node */
	if ELEM(NULL, ntree, node)
		return;
	//if (node->id) /* for group nodes */
	//	RNA_pointer_create(node->id, &RNA_Node, node, &ptr);
	//else
		RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr); 
	
	/* set update callback */
	// xxx is this really needed
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_node_region_buttons, NULL);
	
	/* draw this node's name, etc. */
	uiItemR(layout, &ptr, "label", 0, NULL, ICON_NODE);
	uiItemS(layout);
	uiItemR(layout, &ptr, "name", 0, NULL, ICON_NODE);
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "NODE_OT_hide_socket_toggle");
	uiItemS(layout);

	/* draw this node's settings */
	if (node->typeinfo && node->typeinfo->uifuncbut)
		node->typeinfo->uifuncbut(layout, (bContext *)C, &ptr);
	else if (node->typeinfo && node->typeinfo->uifunc)
		node->typeinfo->uifunc(layout, (bContext *)C, &ptr);
}

static int node_sockets_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	return (snode && snode->nodetree && G.rt == 777);
}

static void node_sockets_panel(const bContext *C, Panel *pa)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree= (snode) ? snode->edittree : NULL;
	bNode *node = (ntree) ? nodeGetActive(ntree) : NULL;
	bNodeSocket *sock;
	uiLayout *layout= pa->layout, *split;
	char name[UI_MAX_NAME_STR];
	
	if (ELEM(NULL, ntree, node))
		return;
	
	for (sock=node->inputs.first; sock; sock=sock->next) {
		BLI_snprintf(name, sizeof(name), "%s:", sock->name);

		split = uiLayoutSplit(layout, 0.35f, 0);
		uiItemL(split, name, ICON_NONE);
		uiTemplateNodeLink(split, ntree, node, sock);
	}
}

/* ******************* node buttons registration ************** */

void node_buttons_register(ARegionType *art)
{
	PanelType *pt;
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype node panel active node");
	strcpy(pt->idname, "NODE_PT_item");
	strcpy(pt->label, "Active Node");
	pt->draw= active_node_panel;
	pt->poll= active_node_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype node panel node sockets");
	strcpy(pt->idname, "NODE_PT_sockets");
	strcpy(pt->label, "Sockets");
	pt->draw= node_sockets_panel;
	pt->poll= node_sockets_poll;
	pt->flag |= PNL_DEFAULT_CLOSED;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype node panel gpencil");
	strcpy(pt->idname, "NODE_PT_gpencil");
	strcpy(pt->label, "Grease Pencil");
	pt->draw= gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);
}

static int node_properties(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= node_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

/* non-standard poll operator which doesn't care if there are any nodes */
static int node_properties_poll(bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
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
