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
 * Contributor(s): none yet.
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
			}		
			break;
			
		case B_NODE_USESCENE:
			node_composit_default(G.scene);
			snode_set_context(snode);
			allqueue(REDRAWNODE, 0);
			break;
	}
}


void node_buttons(ScrArea *sa)
{
	SpaceNode *snode= sa->spacedata.first;
	uiBlock *block;
	short xco;
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
	
//		xmax= GetButStringLength("View");
//		uiDefPulldownBut(block, time_viewmenu, NULL, 
//					  "View", xco, -2, xmax-3, 24, "");
//		xco+= xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* main type choosing */
	uiBlockBeginAlign(block);
	uiDefIconButI(block, ROW, B_REDR, ICON_MATERIAL_DEHLT, xco,2,XIC,YIC-2,
				  &(snode->treetype), 2, 0, 0, 0, "Material Nodes");
	xco+= XIC;
	uiDefIconButI(block, ROW, B_REDR, ICON_IMAGE_DEHLT, xco,2,XIC,YIC-2,
				  &(snode->treetype), 2, 1, 0, 0, "Composit Nodes");
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
		uiDefButS(block, TOG, B_NODE_USESCENE, "Use Nodes", xco+5,0,70,19, &G.scene->use_nodes, 0.0f, 0.0f, 0, 0, "");
	}
	
	/* always as last  */
	sa->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
