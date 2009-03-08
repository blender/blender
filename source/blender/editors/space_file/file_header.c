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
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"
#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"
#include "UI_text.h"

#include "file_intern.h"
#include "filelist.h"

#define B_SORTIMASELLIST 1
#define B_RELOADIMASELDIR 2


/* ************************ header area region *********************** */

static void do_viewmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *dummy_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "dummy_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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

static void do_file_header_buttons(bContext *C, void *arg, int event)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	switch(event) {
		case B_SORTIMASELLIST:
			filelist_sort(sfile->files, sfile->params->sort);
			WM_event_add_notifier(C, NC_WINDOW, NULL);
			break;
		case B_RELOADIMASELDIR:
			WM_event_add_notifier(C, NC_WINDOW, NULL);
			break;
	}
}


void file_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);

	uiBlock *block;
	int xco, yco= 3;
	int xcotitle;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_file_header_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, dummy_viewmenu, CTX_wm_area(C), 
						 "View", xco, yco-2, xmax-3, 24, "");
		xco+=XIC+xmax;
	}
	
	/* SORT TYPE */
	uiBlockSetEmboss(block, UI_EMBOSSX);
	xco+=XIC;
	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTALPHA,	xco+=XIC,0,XIC,YIC, &params->sort, 1.0, 0.0, 0, 0, "Sorts files alphabetically");
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTBYEXT,	xco+=XIC,0,XIC,YIC, &params->sort, 1.0, 3.0, 0, 0, "Sorts files by extension");	
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTTIME,	xco+=XIC,0,XIC,YIC, &params->sort, 1.0, 1.0, 0, 0, "Sorts files by time");
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTSIZE,	xco+=XIC,0,XIC,YIC, &params->sort, 1.0, 2.0, 0, 0, "Sorts files by size");	
	uiBlockEndAlign(block);
	xco+=XIC+10;

	if (sfile->params->type != FILE_MAIN) {
		uiDefIconButBitS(block, TOG, FILE_BOOKMARKS, B_RELOADIMASELDIR, ICON_BOOKMARKS,xco+=XIC,0,XIC,YIC, &params->flag, 0, 0, 0, 0, "Toggles Bookmarks on/off");
		xco+=XIC+10;
	} 

	if (sfile->params->type != FILE_MAIN) {
		uiBlockBeginAlign(block);
		uiDefIconButS(block, ROW, B_RELOADIMASELDIR, ICON_SHORTDISPLAY,	xco+=XIC,0,XIC,YIC, &params->display, 1.0, 1.0, 0, 0, "Displays short file description");
		uiDefIconButS(block, ROW, B_RELOADIMASELDIR, ICON_LONGDISPLAY,	xco+=XIC,0,XIC,YIC, &params->display, 1.0, 2.0, 0, 0, "Displays long file description");
		uiDefIconButS(block, ROW, B_RELOADIMASELDIR, ICON_IMAGE_COL /* ICON_IMGDISPLAY */,	xco+=XIC,0,XIC,YIC, &params->display, 1.0, 3.0, 0, 0, "Displays files as thumbnails");
		uiBlockEndAlign(block);
		xco+=XIC+10;
	} 
	xcotitle= xco;
	xco+= UI_GetStringWidth(G.font, params->title, 0);

	uiBlockSetEmboss(block, UI_EMBOSS);

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}



