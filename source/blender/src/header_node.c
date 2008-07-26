/**
 *
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "DNA_ID.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_butspace.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

void do_node_buttons(ScrArea *sa, unsigned short event)
{
	SpaceNode *snode= sa->spacedata.first;
	Material *ma;
	
	switch(event) {
		case B_NODE_USEMAT:
			ma= (Material *)snode->id;
			if(ma) {
				if(ma->use_nodes && ma->nodetree==NULL) {
					node_shader_default(ma);
					snode_set_context(snode);
				}
				BIF_preview_changed(ID_MA);
				allqueue(REDRAWNODE, 0);
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
			}		
			break;
			
		case B_NODE_USESCENE:
			if(G.scene->use_nodes) {
				if(G.scene->nodetree==NULL)
					node_composit_default(G.scene);
				addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
			}
			snode_set_context(snode);
			allqueue(REDRAWNODE, 0);
			break;
	}
}

static void do_node_viewmenu(void *arg, int event)
{
	SpaceNode *snode= curarea->spacedata.first; 
	
	switch(event) {
		case 1: /* Zoom in */
			snode_zoom_in(curarea);
			break;
		case 2: /* View all */
			snode_zoom_out(curarea);
			break;
		case 3: /* View all */
			snode_home(curarea, snode);
			break;
		case 4: /* Grease Pencil */
			add_blockhandler(curarea, NODES_HANDLER_GREASEPENCIL, UI_PNL_UNSTOW);
		break;
	}
	allqueue(REDRAWNODE, 0);
}

static uiBlock *node_viewmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first; 
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "node_viewmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_node_viewmenu, NULL);

	if (snode->nodetree) {
		uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Grease Pencil...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
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

static void do_node_selectmenu(void *arg, int event)
{
	SpaceNode *snode= curarea->spacedata.first; 
	
	/* functions in editnode.c assume there's a tree */
	if(snode->nodetree==NULL)
		return;
	
	switch(event) {
		case 1: /* border select */
			node_border_select(snode);
			break;
		case 2: /* select/deselect all */
			node_deselectall(snode, 1);
			break;
		case 3:	/* select linked in */
			node_select_linked(snode, 0);
			break;
		case 4:	/* select linked out */
			node_select_linked(snode, 1);
			break;
	}
	allqueue(REDRAWNODE, 0);
}

static uiBlock *node_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "node_selectmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
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
	
	return block;
}

void do_node_addmenu(void *arg, int event)
{
	SpaceNode *snode= curarea->spacedata.first;
	bNode *node;
	float locx, locy;
	short mval[2];
	
	/* store selection in temp test flag */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & NODE_SELECT) node->flag |= NODE_TEST;
		else node->flag &= ~NODE_TEST;
	}
	
	toolbox_mousepos(mval, 0 ); /* get initial mouse position */
	areamouseco_to_ipoco(G.v2d, mval, &locx, &locy);
	node= node_add_node(snode, event, locx, locy);
	
	/* uses test flag */
	snode_autoconnect(snode, node, NODE_TEST);
		
	addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
	
	BIF_undo_push("Add Node");
	
}

static void node_make_addmenu(SpaceNode *snode, int nodeclass, uiBlock *block)
{
	bNodeTree *ntree;
	int tot= 0, a;
	short yco= 0, menuwidth=120;
	
	ntree = snode->nodetree;
	if(ntree) {
		/* mostly taken from toolbox.c, node_add_sublevel() */
		if(ntree) {
			if(nodeclass==NODE_CLASS_GROUP) {
				bNodeTree *ngroup= G.main->nodetree.first;
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
			bNodeTree *ngroup= G.main->nodetree.first;
			for(tot=0, a=0; ngroup; ngroup= ngroup->id.next, tot++) {
				if(ngroup->type==ntree->type) {
					
					uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, (ngroup->id.name+2), 0, 
						yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, NODE_GROUP_MENU+tot, "");
					a++;
				}
			}
		}
		else {
			bNodeType *type;
			int script=0;
			for(a=0, type= ntree->alltypes.first; type; type=type->next) {
				if( type->nclass == nodeclass ) {
					if(type->type == NODE_DYNAMIC) {
						uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, type->name, 0, 
							yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, NODE_DYNAMIC_MENU+script, "");
						script++;
					} else {
					uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, type->name, 0, 
						yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, type->type, "");
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

static uiBlock *node_add_inputmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "node_add_inputmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_INPUT, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_outputmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_outputmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_OUTPUT, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_colormenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_colormenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_OP_COLOR, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_vectormenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_vectormenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_OP_VECTOR, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_filtermenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_filtermenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_OP_FILTER, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_convertermenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_convertermenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_CONVERTOR, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_mattemenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_mattemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_MATTE, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_distortmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_distortmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_DISTORT, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}
static uiBlock *node_add_groupmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_groupmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_GROUP, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

static uiBlock *node_add_dynamicmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "node_add_dynamicmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_node_addmenu, NULL);
	
	node_make_addmenu(snode, NODE_CLASS_OP_DYNAMIC, block);
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

static uiBlock *node_addmenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "node_addmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
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

	} else
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

static void do_node_nodemenu(void *arg, int event)
{
	SpaceNode *snode= curarea->spacedata.first; 
	int fromlib=0;
	
	/* functions in editnode.c assume there's a tree */
	if(snode->nodetree==NULL)
		return;
	fromlib= (snode->id && snode->id->lib);
	
	switch(event) {
		case 1: /* grab/move */
			node_transform_ext(0,0);
			break;
		case 2: /* duplicate */
			if(fromlib) fromlib= -1;
			else node_adduplicate(snode);
			break;
		case 3: /* delete */
			if(fromlib) fromlib= -1;
			else node_delete(snode);
			break;
		case 4: /* make group */
			node_make_group(snode);
			break;
		case 5: /* ungroup */
			node_ungroup(snode);
			break;
		case 6: /* edit group */
			if(fromlib) fromlib= -1;
			else snode_make_group_editable(snode, NULL);
			break;
		case 7: /* hide/unhide */
			node_hide(snode);
			break;
		case 8: /* read saved render layers */
			node_read_renderlayers(snode);
			break;
		case 9: /* show cyclic */
			ntreeSolveOrder(snode->edittree);
			break;
		case 10: /* execute */
			addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
			break;
		case 11: /* make link */
			node_make_link(snode);
			break;
		case 12: /* rename */
			node_rename(snode);
			break;
		case 13: /* read saved full sample layers */
			node_read_fullsamplelayers(snode);
			break;
		case 14: /* connect viewer */
			node_active_link_viewer(snode);
			break;
			
	}
	
	if(fromlib==-1) error_libdata();
	allqueue(REDRAWNODE, 0);
}

static uiBlock *node_nodemenu(void *arg_unused)
{
	SpaceNode *snode= curarea->spacedata.first; 
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "node_nodemenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
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
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Read Saved Full Sample Results|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connect Node to Viewer|Ctrl LMB", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		
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
	
	return block;
}

void node_buttons(ScrArea *sa)
{
	SpaceNode *snode= sa->spacedata.first;
	uiBlock *block;
	short xco, xmax;
	char name[256];
	
	sprintf(name, "header %d", sa->headwin);
	block= uiNewBlock(&sa->uiblocks, name, UI_EMBOSS, UI_HELV, sa->headwin);

	if(area_is_active_area(sa)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);
	
	sa->butspacetype= SPACE_NODE;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, 
					  windowtype_pup(), xco, 0, XIC+10, YIC, 
					  &(sa->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
					  "Displays Current Window Type");

	xco += XIC + 14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if (sa->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_RIGHT, xco,2,XIC,YIC-2,
					  &(sa->flag), 0, 0, 0, 0,  "Show pulldown menus");
	}
	else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_DOWN,  xco,2,XIC,YIC-2,
					  &(sa->flag), 0, 0, 0, 0,  "Hide pulldown menus");
	}
	xco+=XIC;

	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, node_viewmenu, NULL, 
					  "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefPulldownBut(block, node_selectmenu, NULL, 
						 "Select", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Add");
		uiDefPulldownBut(block, node_addmenu, NULL, 
						 "Add", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Node");
		uiDefPulldownBut(block, node_nodemenu, NULL, 
						 "Node", xco, -2, xmax-3, 24, "");
		xco+= xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* main type choosing */
	uiBlockBeginAlign(block);
	uiDefIconButI(block, ROW, B_REDR, ICON_MATERIAL_DEHLT, xco,2,XIC,YIC-2,
				  &(snode->treetype), 2, 0, 0, 0, "Material Nodes");
	xco+= XIC;
	uiDefIconButI(block, ROW, B_REDR, ICON_IMAGE_DEHLT, xco,2,XIC,YIC-2,
				  &(snode->treetype), 2, 1, 0, 0, "Composite Nodes");
	xco+= 2*XIC;
	uiBlockEndAlign(block);
	
	/* find and set the context */
	snode_set_context(snode);
	
	if(snode->treetype==NTREE_SHADER) {
		if(snode->from) {
										/* 0, NULL -> pin */
			xco= std_libbuttons(block, xco, 0, 0, NULL, B_MATBROWSE, ID_MA, 1, snode->id, snode->from, &(snode->menunr), 
					   B_MATALONE, B_MATLOCAL, B_MATDELETE, B_AUTOMATNAME, B_KEEPDATA);
			
			if(snode->id) {
				Material *ma= (Material *)snode->id;
				uiDefButC(block, TOG, B_NODE_USEMAT, "Use Nodes", xco+5,0,70,19, &ma->use_nodes, 0.0f, 0.0f, 0, 0, "");
				xco+=80;
			}
		}
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		uiDefButS(block, TOG, B_NODE_USESCENE, "Use Nodes", xco+5,0,70,19, &G.scene->use_nodes, 0.0f, 0.0f, 0, 0, "Indicate this Scene will use Nodes and execute them while editing");
		xco+= 80;
		uiDefButBitI(block, TOG, R_COMP_FREE, B_NOP, "Free Unused", xco+5,0,80,19, &G.scene->r.scemode, 0.0f, 0.0f, 0, 0, "Free Nodes that are not used while composite");
		xco+= 80;
		uiDefButBitS(block, TOG, SNODE_BACKDRAW, REDRAWNODE, "Backdrop", xco+5,0,80,19, &snode->flag, 0.0f, 0.0f, 0, 0, "Use active Viewer Node output as backdrop");
		xco+= 80;
	}
	
	/* always as last  */
	sa->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
