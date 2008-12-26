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

#include "DNA_ID.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "outliner_intern.h"


/* ************************ header area region *********************** */

static void do_viewmenu(bContext *C, void *arg, int event)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceOops *soops= curarea->spacedata.first;
	
	switch(event) {
		case 0: /* Shuffle Selected Blocks */
			//shuffle_oops();
			break;
		case 1: /* Shrink Selected Blocks */
			//shrink_oops();
			break;
		case 2: /* View All */
			//do_oops_buttons(B_OOPSHOME);
			break;
		case 3: /* View All */
			//do_oops_buttons(B_OOPSVIEWSEL);
			break;
		case 4: /* Maximize Window */
			/* using event B_FULL */
			break;
		case 5: /* show rna viewer */
			soops->type= SO_RNA;
			break;
		case 14: /* show outliner viewer */
			soops->type= SO_OUTLINER;
			break;
		case 6:
			//outliner_toggle_visible(curarea);
			break;
		case 7:
			//outliner_show_hierarchy(curarea);
			break;
		case 8:
			//outliner_show_active(curarea);
			break;
		case 9:
			//outliner_one_level(curarea, 1);
			break;
		case 10:
			//outliner_one_level(curarea, -1);
			break;
		case 12:
			if (soops->flag & SO_HIDE_RESTRICTCOLS) soops->flag &= ~SO_HIDE_RESTRICTCOLS;
			else soops->flag |= SO_HIDE_RESTRICTCOLS;
			break;
	}
	ED_area_tag_redraw(curarea);
}

static uiBlock *outliner_viewmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceOops *soops= curarea->spacedata.first;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "outliner_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	if(soops->type==SO_RNA) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Outliner", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	}
	if(soops->type==SO_OUTLINER) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show RNA Viewer", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
		
		if (soops->flag & SO_HIDE_RESTRICTCOLS)
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Show Restriction Columns", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		else
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Show Restriction Columns", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Expand One Level|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Collapse One Level|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show/Hide All", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hierarchy|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Active|NumPad .", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	}
	
//	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
//	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
//	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
	
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

#define B_REDR	1
static void do_outliner_buttons(bContext *C, void *arg, int event)
{
	ScrArea *sa= CTX_wm_area(C);
	
	switch(event) {
		case B_REDR:
			ED_area_tag_redraw(sa);
			break;
	}
}


void outliner_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceOops *soutliner= (SpaceOops*)CTX_wm_space_data(C);
	uiBlock *block;
	int xco, yco= 3, xmax;
	char *path;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_outliner_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, outliner_viewmenu, CTX_wm_area(C), 
						 "View", xco, yco-2, xmax-3, 24, ""); 
		xco += xmax;
		
		/* header text */
		xco += XIC*2;
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	if(soutliner->type==SO_RNA) {
		path= (soutliner->rnapath)? soutliner->rnapath: "Main";
		xmax= GetButStringLength(path);
		uiDefBut(block, LABEL, 0, path, xco, yco-2, xmax-3, 24, 0, 0, 0, 0, 0, "Current RNA Path");
		xco += xmax;
	}		
	
	if(soutliner->type==SO_OUTLINER) {
		if(G.main->library.first) 
			uiDefButS(block, MENU, B_REDR, "Outliner Display%t|Libraries %x7|All Scenes %x0|Current Scene %x1|Visible Layers %x2|Groups %x6|Same Types %x5|Selected %x3|Active %x4|Sequence %x10",	 xco, yco, 100, 20,  &soutliner->outlinevis, 0, 0, 0, 0, "");
		else
			uiDefButS(block, MENU, B_REDR, "Outliner Display%t|All Scenes %x0|Current Scene %x1|Visible Layers %x2|Groups %x6|Same Types %x5|Selected %x3|Active %x4|Sequence %x10",	 xco, yco, 100, 20,  &soutliner->outlinevis, 0, 0, 0, 0, "");
	}
	
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


