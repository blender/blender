/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_space_types.h"
#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_node.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "node_intern.h"

/* ************************ add menu *********************** */

static void do_node_add(bContext *C, void *arg, int event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	bNode *node;
	
	/* store selection in temp test flag */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & NODE_SELECT) node->flag |= NODE_TEST;
		else node->flag &= ~NODE_TEST;
	}
	
	node= node_add_node(snode, CTX_data_scene(C), event, snode->mx, snode->my);
	
	/* uses test flag */
	snode_autoconnect(snode, node, NODE_TEST);
		
	snode_handle_recalc(C, snode);
}

static void node_auto_add_menu(bContext *C, uiLayout *layout, void *arg_nodeclass)
{
	Main *bmain= CTX_data_main(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	bNodeTree *ntree;
	int nodeclass= GET_INT_FROM_POINTER(arg_nodeclass);
	int tot= 0, a;
	
	ntree = snode->nodetree;

	if(!ntree) {
		uiItemS(layout);
		return;
	}

	/* mostly taken from toolbox.c, node_add_sublevel() */
	if(nodeclass==NODE_CLASS_GROUP) {
		bNodeTree *ngroup= bmain->nodetree.first;
		for(; ngroup; ngroup= ngroup->id.next)
			if(ngroup->type==ntree->type)
				tot++;
	}
	else {
		bNodeType *type = ntree->alltypes.first;
		while(type) {
			if(type->nclass == nodeclass)
				tot++;
			type= type->next;
		}
	}	
	
	if(tot==0) {
		uiItemS(layout);
		return;
	}

	uiLayoutSetFunc(layout, do_node_add, NULL);
	
	if(nodeclass==NODE_CLASS_GROUP) {
		bNodeTree *ngroup= bmain->nodetree.first;

		for(tot=0, a=0; ngroup; ngroup= ngroup->id.next, tot++) {
			if(ngroup->type==ntree->type) {
				uiItemV(layout, ngroup->id.name+2, 0, NODE_GROUP_MENU+tot);
				a++;
			}
		}
	}
	else {
		bNodeType *type;
		int script=0;

		for(a=0, type= ntree->alltypes.first; type; type=type->next) {
			if(type->nclass == nodeclass && type->name) {
				if(type->type == NODE_DYNAMIC) {
					uiItemV(layout, type->name, 0, NODE_DYNAMIC_MENU+script);
					script++;
				}
				else
					uiItemV(layout, type->name, 0, type->type);

				a++;
			}
		}
	}
}

static void node_menu_add(const bContext *C, Menu *menu)
{
	uiLayout *layout= menu->layout;
	SpaceNode *snode= CTX_wm_space_node(C);

	if(!snode->nodetree)
		uiLayoutSetActive(layout, 0);

	if(snode->treetype==NTREE_SHADER) {
		uiItemMenuF(layout, "Input", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_INPUT));
		uiItemMenuF(layout, "Output", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OUTPUT));
		uiItemMenuF(layout, "Color", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_COLOR));
		uiItemMenuF(layout, "Vector", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_VECTOR));
		uiItemMenuF(layout, "Convertor", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_CONVERTOR));
		uiItemMenuF(layout, "Group", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_GROUP));
		uiItemMenuF(layout, "Dynamic", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_DYNAMIC));
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		uiItemMenuF(layout, "Input", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_INPUT));
		uiItemMenuF(layout, "Output", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OUTPUT));
		uiItemMenuF(layout, "Color", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_COLOR));
		uiItemMenuF(layout, "Vector", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_VECTOR));
		uiItemMenuF(layout, "Filter", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_FILTER));
		uiItemMenuF(layout, "Convertor", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_CONVERTOR));
		uiItemMenuF(layout, "Matte", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_MATTE));
		uiItemMenuF(layout, "Distort", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_DISTORT));
		uiItemMenuF(layout, "Group", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_GROUP));
	}
	else if(snode->treetype==NTREE_TEXTURE) {
		uiItemMenuF(layout, "Input", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_INPUT));
		uiItemMenuF(layout, "Output", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OUTPUT));
		uiItemMenuF(layout, "Color", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_OP_COLOR));
		uiItemMenuF(layout, "Patterns", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_PATTERN));
		uiItemMenuF(layout, "Textures", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_TEXTURE));
		uiItemMenuF(layout, "Convertor", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_CONVERTOR));
		uiItemMenuF(layout, "Distort", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_DISTORT));
		uiItemMenuF(layout, "Group", 0, node_auto_add_menu, SET_INT_IN_POINTER(NODE_CLASS_GROUP));
	}
}

void node_menus_register(ARegionType *art)
{
	MenuType *mt;

	mt= MEM_callocN(sizeof(MenuType), "spacetype node menu add");
	strcpy(mt->idname, "NODE_MT_add");
	strcpy(mt->label, "Add");
	mt->draw= node_menu_add;
	BLI_addtail(&art->menutypes, mt);
}

#if 0
static void do_node_nodemenu(bContext *C, void *arg, int event)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceNode *snode= CTX_wm_space_node(C); 
	int fromlib=0;
	
	/* functions in editnode.c assume there's a tree */
	if(snode->nodetree==NULL)
		return;
	fromlib= (snode->id && snode->id->lib);
	
	switch(event) {
		case 1: /* grab/move */
			// XXX node_transform_ext(0,0);
			break;
		case 2: /* duplicate */
			if(fromlib) fromlib= -1;
			else ; // XXX node_adduplicate(snode);
			break;
		case 3: /* delete */
			if(fromlib) fromlib= -1;
			else ; // XXX node_delete(snode);
			break;
		case 4: /* make group */
			// XXX node_make_group(snode);
			break;
		case 5: /* ungroup */
			// XXX node_ungroup(snode);
			break;
		case 6: /* edit group */
			if(fromlib) fromlib= -1;
			else ; // XXX snode_make_group_editable(snode, NULL);
			break;
		case 7: /* hide/unhide */
			// XXX node_hide(snode);
			break;
		case 8: /* read saved render layers */
			// XXX node_read_renderlayers(snode);
			break;
		case 9: /* show cyclic */
			// XXX ntreeSolveOrder(snode->edittree);
			break;
		case 10: /* execute */
			// XXXX addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
			break;
		case 11: /* make link */
			// XXX node_make_link(snode);
			break;
		case 12: /* rename */
			// XXX node_rename(snode);
			break;
		case 13: /* read saved full sample layers */
			// XXX node_read_fullsamplelayers(snode);
			break;
		case 14: /* connect viewer */
			// XXX node_active_link_viewer(snode);
			break;
			
	}
	
	// XXX if(fromlib==-1) error_libdata();
	
	ED_area_tag_redraw(curarea);
}

static uiBlock *node_nodemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "node_nodemenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_nodemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Link|F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Group|Ctrl G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Ungroup|Alt G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Edit Group|Tab", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide/Unhide|H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rename|Ctrl R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(snode->treetype==NTREE_COMPOSIT) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Execute Composite|E", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Read Saved Render Results|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Read Saved Full Sample Results|Shift R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connect Node to Viewer|Ctrl RMB", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Cyclic Dependencies|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}
#endif

