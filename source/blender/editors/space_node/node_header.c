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

/* ************************ header area region *********************** */

static void do_node_selectmenu(bContext *C, void *arg, int event)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C); 
	
	/* functions in editnode.c assume there's a tree */
	if(snode->nodetree==NULL)
		return;
	
	switch(event) {
		case 1: /* border select */
			WM_operator_name_call(C, "NODE_OT_select_border", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 2: /* select/deselect all */
			// XXX node_deselectall(snode, 1);
			break;
		case 3:	/* select linked in */
			// XXX node_select_linked(snode, 0);
			break;
		case 4:	/* select linked out */
			// XXX node_select_linked(snode, 1);
			break;
	}
	
	ED_area_tag_redraw(curarea);
}

static uiBlock *node_selectmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "node_selectmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_selectmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select Linked From|L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select Linked To|Shift L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
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

void do_node_addmenu(bContext *C, void *arg, int event)
{
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
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

static void node_make_addmenu(bContext *C, int nodeclass, uiBlock *block)
{
	Main *bmain= CTX_data_main(C);
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
	bNodeTree *ntree;
	int tot= 0, a;
	short yco= 0, menuwidth=120;
	
	ntree = snode->nodetree;
	if(ntree) {
		/* mostly taken from toolbox.c, node_add_sublevel() */
		if(ntree) {
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
		}	
		
		if(tot==0) {
			uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			return;
		}
		
		if(nodeclass==NODE_CLASS_GROUP) {
			bNodeTree *ngroup= bmain->nodetree.first;
			for(tot=0, a=0; ngroup; ngroup= ngroup->id.next, tot++) {
				if(ngroup->type==ntree->type) {
					
					uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, (ngroup->id.name+2), 0, 
						yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1.0f, (float)(NODE_GROUP_MENU+tot), "");
					a++;
				}
			}
		}
		else {
			bNodeType *type;
			int script=0;
			for(a=0, type= ntree->alltypes.first; type; type=type->next) {
				if( type->nclass == nodeclass && type->name) {
					if(type->type == NODE_DYNAMIC) {
						uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, type->name, 0, 
							yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1.0f, (float)(NODE_DYNAMIC_MENU+script), "");
						script++;
					} else {
					uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, type->name, 0, 
						yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1.0f, (float)(type->type), "");
					}
					a++;
				}
			}
		}
	} else {
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		return;
	}
}

static uiBlock *node_add_inputmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;

	block= uiBeginBlock(C, ar, "node_add_inputmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_INPUT, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_outputmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_outputmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_OUTPUT, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_colormenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_colormenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_OP_COLOR, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_vectormenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_vectormenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_OP_VECTOR, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_filtermenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_filtermenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_OP_FILTER, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_convertermenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_convertermenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_CONVERTOR, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_mattemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_mattemenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_MATTE, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_distortmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_distortmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_DISTORT, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_patternmenu(bContext *C, ARegion *ar,  void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_patternmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_PATTERN, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_texturemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_texturemenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_TEXTURE, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}
static uiBlock *node_add_groupmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_groupmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_GROUP, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}

static uiBlock *node_add_dynamicmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "node_add_dynamicmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(C, NODE_CLASS_OP_DYNAMIC, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}

static uiBlock *node_addmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "node_addmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	if(snode->treetype==NTREE_SHADER) {
		uiDefIconTextBlockBut(block, node_add_inputmenu, NULL, ICON_RIGHTARROW_THIN, "Input", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_outputmenu, NULL, ICON_RIGHTARROW_THIN, "Output", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_colormenu, NULL, ICON_RIGHTARROW_THIN, "Color", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_vectormenu, NULL, ICON_RIGHTARROW_THIN, "Vector", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_convertermenu, NULL, ICON_RIGHTARROW_THIN, "Convertor", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_groupmenu, NULL, ICON_RIGHTARROW_THIN, "Group", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_dynamicmenu, NULL, ICON_RIGHTARROW_THIN, "Dynamic", 0, yco-=20, 120, 19, "");
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		uiDefIconTextBlockBut(block, node_add_inputmenu, NULL, ICON_RIGHTARROW_THIN, "Input", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_outputmenu, NULL, ICON_RIGHTARROW_THIN, "Output", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_colormenu, NULL, ICON_RIGHTARROW_THIN, "Color", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_vectormenu, NULL, ICON_RIGHTARROW_THIN, "Vector", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_filtermenu, NULL, ICON_RIGHTARROW_THIN, "Filter", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_convertermenu, NULL, ICON_RIGHTARROW_THIN, "Convertor", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_mattemenu, NULL, ICON_RIGHTARROW_THIN, "Matte", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_distortmenu, NULL, ICON_RIGHTARROW_THIN, "Distort", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_groupmenu, NULL, ICON_RIGHTARROW_THIN, "Group", 0, yco-=20, 120, 19, "");

	} else if(snode->treetype==NTREE_TEXTURE) {
		uiDefIconTextBlockBut(block, node_add_inputmenu, NULL, ICON_RIGHTARROW_THIN, "Input", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_outputmenu, NULL, ICON_RIGHTARROW_THIN, "Output", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_colormenu, NULL, ICON_RIGHTARROW_THIN, "Color", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_patternmenu, NULL, ICON_RIGHTARROW_THIN, "Patterns", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_texturemenu, NULL, ICON_RIGHTARROW_THIN, "Textures", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_convertermenu, NULL, ICON_RIGHTARROW_THIN, "Convertor", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_distortmenu, NULL, ICON_RIGHTARROW_THIN, "Distort", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, node_add_groupmenu, NULL, ICON_RIGHTARROW_THIN, "Group", 0, yco-=20, 120, 19, "");
	}
	else
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	
	return block;
}

static void do_node_nodemenu(bContext *C, void *arg, int event)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C); 
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
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
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

static void do_node_viewmenu(bContext *C, void *arg, int event)
{
//	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
//	ARegion *ar= CTX_wm_region(C);
	ScrArea *sa= CTX_wm_area(C);
	
	switch(event) {
		case 1: /* Zoom in */
			WM_operator_name_call(C, "VIEW2D_OT_zoom_in", WM_OP_EXEC_REGION_WIN, NULL);
			break;
		case 2: /* View all */
			WM_operator_name_call(C, "VIEW2D_OT_zoom_out", WM_OP_EXEC_REGION_WIN, NULL);
			break;
		case 3: /* View all */
			WM_operator_name_call(C, "NODE_OT_view_all", WM_OP_EXEC_REGION_WIN, NULL);
			break;
		case 4: /* Grease Pencil */
			// XXX add_blockhandler(sa, NODES_HANDLER_GREASEPENCIL, UI_PNL_UNSTOW);
			break;
	}
	ED_area_tag_redraw(sa);
}

static uiBlock *node_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "node_viewmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_node_viewmenu, NULL);

	if (snode->nodetree) {
		uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Grease Pencil...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	/* XXX if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	*/
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

static void do_node_buttons(bContext *C, void *arg, int event)
{
	// NODE_FIX_ME : instead of using "current material/texture/scene", node editor can also pin context?
	// note: scene context better not gets overridden, that'll clash too much (ton)
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
	Scene *scene= CTX_data_scene(C);
	Material *ma;
	Tex *tx;
	
	switch(event) {
		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));			
			break;
		case B_NODE_USEMAT:
			ma= (Material *)snode->id;
			if(ma) {
				if(ma->use_nodes && ma->nodetree==NULL) {
					node_shader_default(ma);
					snode_set_context(snode, scene);
				}
			}		
			ED_area_tag_redraw(CTX_wm_area(C));			
			break;
			
		case B_NODE_USESCENE:
			if(scene->use_nodes) {
				if(scene->nodetree==NULL)
					node_composit_default(scene);
			}
			snode_set_context(snode, scene);
			ED_area_tag_redraw(CTX_wm_area(C));			
			break;
			
		case B_NODE_USETEX:
			tx = (Tex *)snode->id;
			if(tx) {
				tx->type = 0;
				if(tx->use_nodes && tx->nodetree==NULL) {
					node_texture_default(tx);
					snode_set_context(snode, scene);
				}
			}
			ED_area_tag_redraw(CTX_wm_area(C));			
			break;
	}
}

void node_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceNode *snode= (SpaceNode*)CTX_wm_space_data(C);
	Scene *scene= CTX_data_scene(C);
	uiBlock *block;
	short xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header node", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_node_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, node_viewmenu, NULL, 
					  "View", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefPulldownBut(block, node_selectmenu, NULL, 
						 "Select", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Add");
		uiDefPulldownBut(block, node_addmenu, NULL, 
						 "Add", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Node");
		uiDefPulldownBut(block, node_nodemenu, NULL, 
						 "Node", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* main type choosing */
	uiBlockBeginAlign(block);
	uiDefIconButI(block, ROW, B_REDR, ICON_MATERIAL_DATA, xco,yco,XIC,YIC-2,
				  &(snode->treetype), 2.0f, 0.0f, 0.0f, 0.0f, "Material Nodes");
	xco+= XIC;
	uiDefIconButI(block, ROW, B_REDR, ICON_IMAGE_DATA, xco,yco,XIC,YIC-2,
				  &(snode->treetype), 2.0f, 1.0f, 0.0f, 0.0f, "Composite Nodes");
	xco+= XIC;
	uiDefIconButI(block, ROW, B_REDR, ICON_TEXTURE_DATA, xco,yco,XIC,YIC-2,
				  &(snode->treetype), 2.0f, 2.0f, 0.0f, 0.0f, "Texture Nodes");
	xco+= 2*XIC;
	uiBlockEndAlign(block);

	/* find and set the context */
	snode_set_context(snode, scene);
	
	if(snode->treetype==NTREE_SHADER) {
		if(snode->from) {
										/* 0, NULL -> pin */
			// XXX xco= std_libbuttons(block, xco, 0, 0, NULL, B_MATBROWSE, ID_MA, 1, snode->id, snode->from, &(snode->menunr), 
			//   B_MATALONE, B_MATLOCAL, B_MATDELETE, B_AUTOMATNAME, B_KEEPDATA);
			
			if(snode->id) {
				Material *ma= (Material *)snode->id;
				uiDefButC(block, TOG, B_NODE_USEMAT, "Use Nodes", xco+5,yco,90,19, &ma->use_nodes, 0.0f, 0.0f, 0, 0, "");
				xco+=80;
			}
		}
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		int icon;
		
		if(WM_jobs_test(CTX_wm_manager(C), sa)) icon= ICON_REC; else icon= ICON_BLANK1;
		uiDefIconTextButS(block, TOG, B_NODE_USESCENE, icon, "Use Nodes", xco+5,yco,100,19, &scene->use_nodes, 0.0f, 0.0f, 0, 0, "Indicate this Scene will use Nodes and execute them while editing");
		xco+= 100;
		uiDefButBitI(block, TOG, R_COMP_FREE, B_NOP, "Free Unused", xco+5,yco,100,19, &scene->r.scemode, 0.0f, 0.0f, 0, 0, "Free Nodes that are not used while composite");
		xco+= 100;
		uiDefButBitS(block, TOG, SNODE_BACKDRAW, B_REDR, "Backdrop", xco+5,yco,90,19, &snode->flag, 0.0f, 0.0f, 0, 0, "Use active Viewer Node output as backdrop");
		xco+= 90;
	}
	else if(snode->treetype==NTREE_TEXTURE) {
		if(snode->from) {
			
			// XXX xco= std_libbuttons(block, xco, 0, 0, NULL, B_TEXBROWSE, ID_TE, 1, snode->id, snode->from, &(snode->menunr), 
			//		   B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
			
			if(snode->id) {
				Tex *tx= (Tex *)snode->id;
				uiDefButC(block, TOG, B_NODE_USETEX, "Use Nodes", xco+5,yco,90,19, &tx->use_nodes, 0.0f, 0.0f, 0, 0, "");
				xco+=80;
			}
		}
	}
	
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+100, (int)(ar->v2d.tot.ymax-ar->v2d.tot.ymin));
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


