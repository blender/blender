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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_header.c
 *  \ingroup spnode
 */


#include <string.h>
#include <stdio.h>

#include "DNA_space_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "node_intern.h"

/* ************************ add menu *********************** */

static void do_node_add(bContext *C, bNodeTemplate *ntemp)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar;
	bNode *node;
	
	/* get location to add node at mouse */
	for (ar=sa->regionbase.first; ar; ar=ar->next) {
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			wmWindow *win= CTX_wm_window(C);
			int x= win->eventstate->x - ar->winrct.xmin;
			int y= win->eventstate->y - ar->winrct.ymin;
			
			if (y < 60) y+= 60;
			UI_view2d_region_to_view(&ar->v2d, x, y, &snode->mx, &snode->my);
		}
	}
	
	/* store selection in temp test flag */
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & NODE_SELECT) node->flag |= NODE_TEST;
		else node->flag &= ~NODE_TEST;
	}
	
	/* node= */ node_add_node(snode, bmain, scene, ntemp, snode->mx, snode->my);
	
	/* select previous selection before autoconnect */
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & NODE_TEST) node->flag |= NODE_SELECT;
	}
	
	/* deselect after autoconnection */
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		if (node->flag & NODE_TEST) node->flag &= ~NODE_SELECT;
	}
		
	snode_notify(C, snode);
	snode_dag_update(C, snode);
}

static void do_node_add_static(bContext *C, void *UNUSED(arg), int event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	bNodeTemplate ntemp;
	
	ntemp.type = event;
	ntemp.main = bmain;
	ntemp.scene = scene;
	
	do_node_add(C, &ntemp);
}

static void do_node_add_group(bContext *C, void *UNUSED(arg), int event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	bNodeTemplate ntemp;
	
	if (event>=0) {
		ntemp.ngroup= BLI_findlink(&G.main->nodetree, event);
		ntemp.type = ntemp.ngroup->nodetype;
	}
	else {
		ntemp.type = -event;
		switch (ntemp.type) {
		case NODE_GROUP:
			ntemp.ngroup = ntreeAddTree("Group", snode->treetype, ntemp.type);
			break;
		case NODE_FORLOOP:
			ntemp.ngroup = ntreeAddTree("For Loop", snode->treetype, ntemp.type);
			break;
		case NODE_WHILELOOP:
			ntemp.ngroup = ntreeAddTree("While Loop", snode->treetype, ntemp.type);
			break;
		default:
			ntemp.ngroup = NULL;
		}
	}
	if (!ntemp.ngroup)
		return;
	
	ntemp.main = bmain;
	ntemp.scene = scene;
	
	do_node_add(C, &ntemp);
}

#if 0 /* disabled */
static void do_node_add_dynamic(bContext *C, void *UNUSED(arg), int event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	bNodeTemplate ntemp;
	
	ntemp.type = NODE_DYNAMIC;
	
	ntemp.main = bmain;
	ntemp.scene = scene;
	
	do_node_add(C, &ntemp);
}
#endif

static int node_tree_has_type(int treetype, int nodetype)
{
	bNodeTreeType *ttype= ntreeGetType(treetype);
	bNodeType *ntype;
	for (ntype=ttype->node_types.first; ntype; ntype=ntype->next) {
		if (ntype->type==nodetype)
			return 1;
	}
	return 0;
}

static void node_add_menu(bContext *C, uiLayout *layout, void *arg_nodeclass)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree;
	int nodeclass= GET_INT_FROM_POINTER(arg_nodeclass);
	int event, compatibility= 0;
	
	ntree = snode->nodetree;
	
	if (!ntree) {
		uiItemS(layout);
		return;
	}

	if (ntree->type == NTREE_SHADER) {
		if (scene_use_new_shading_nodes(scene))
			compatibility= NODE_NEW_SHADING;
		else
			compatibility= NODE_OLD_SHADING;
	}
	
	if (nodeclass==NODE_CLASS_GROUP) {
		bNodeTree *ngroup;
		
		uiLayoutSetFunc(layout, do_node_add_group, NULL);
		
		/* XXX hack: negative numbers used for empty group types */
		if (node_tree_has_type(ntree->type, NODE_GROUP))
			uiItemV(layout, IFACE_("New Group"), 0, -NODE_GROUP);
		if (node_tree_has_type(ntree->type, NODE_FORLOOP))
			uiItemV(layout, IFACE_("New For Loop"), 0, -NODE_FORLOOP);
		if (node_tree_has_type(ntree->type, NODE_WHILELOOP))
			uiItemV(layout, IFACE_("New While Loop"), 0, -NODE_WHILELOOP);
		uiItemS(layout);
		
		for (ngroup=bmain->nodetree.first, event=0; ngroup; ngroup= ngroup->id.next, ++event) {
			/* only use group trees */
			if (ngroup->type==ntree->type && ELEM3(ngroup->nodetype, NODE_GROUP, NODE_FORLOOP, NODE_WHILELOOP)) {
				uiItemV(layout, ngroup->id.name+2, 0, event);
			}
		}
	}
	else if (nodeclass==NODE_DYNAMIC) {
		/* disabled */
	}
	else {
		bNodeType *ntype;
		
		uiLayoutSetFunc(layout, do_node_add_static, NULL);
		
		for (ntype=ntreeGetType(ntree->type)->node_types.first; ntype; ntype=ntype->next) {
			if (ntype->nclass==nodeclass && ntype->name)
				if (!compatibility || (ntype->compatibility & compatibility))
					uiItemV(layout, IFACE_(ntype->name), 0, ntype->type);
		}
	}
}

static void node_menu_add_foreach_cb(void *calldata, int nclass, const char *name)
{
	uiLayout *layout= calldata;
	uiItemMenuF(layout, IFACE_(name), 0, node_add_menu, SET_INT_IN_POINTER(nclass));
}

static void node_menu_add(const bContext *C, Menu *menu)
{
	Scene *scene= CTX_data_scene(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	uiLayout *layout= menu->layout;
	bNodeTreeType *ntreetype= ntreeGetType(snode->treetype);

	if (!snode->nodetree)
		uiLayoutSetActive(layout, 0);
	
	if (ntreetype && ntreetype->foreach_nodeclass)
		ntreetype->foreach_nodeclass(scene, layout, node_menu_add_foreach_cb);
}

void node_menus_register(void)
{
	MenuType *mt;

	mt= MEM_callocN(sizeof(MenuType), "spacetype node menu add");
	strcpy(mt->idname, "NODE_MT_add");
	strcpy(mt->label, "Add");
	mt->draw= node_menu_add;
	WM_menutype_add(mt);
}

