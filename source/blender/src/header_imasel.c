/**
 * header_imasel.c oct-2003
 *
 * Functions to draw the "Image Browser" window header
 * and handle user events sent to it.
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BMF_Api.h"
#include "BIF_language.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BIF_filelist.h"
#include "BIF_gl.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"


void do_imasel_buttons(short event)
{
	SpaceImaSel *simasel;

	simasel= curarea->spacedata.first;
	
	if(curarea->win==0) return;

	switch(event) {
	case B_SORTIMASELLIST:
		BIF_filelist_sort(simasel->files, simasel->sort);
		scrarea_queue_winredraw(curarea);
		break;
		
	case B_RELOADIMASELDIR:
		BIF_filelist_free(simasel->files);
		scrarea_queue_winredraw(curarea);
		break;
	case B_FILTERIMASELDIR:
		if (simasel->flag & FILE_FILTER) {
			BIF_filelist_setfilter(simasel->files,simasel->filter);
			BIF_filelist_filter(simasel->files);
			scrarea_queue_winredraw(curarea);
		} else {
			BIF_filelist_setfilter(simasel->files,0);
			BIF_filelist_filter(simasel->files);
			scrarea_queue_winredraw(curarea);
		}
		break;
	}
}

void imasel_buttons(void)
{
	SpaceImaSel *simasel;
	uiBlock *block;
	short xco, xcotitle;
	char naam[256];

	simasel= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_IMASEL;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;	

	/* FULL WINDOW */
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "");
	
	/* SORT TYPE */
	xco+=XIC;
	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTALPHA,	xco+=XIC,0,XIC,YIC, &simasel->sort, 1.0, 0.0, 0, 0, "Sorts files alphabetically");
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTBYEXT,	xco+=XIC,0,XIC,YIC, &simasel->sort, 1.0, 3.0, 0, 0, "Sorts files by extension");	
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTTIME,	xco+=XIC,0,XIC,YIC, &simasel->sort, 1.0, 1.0, 0, 0, "Sorts files by time");
	uiDefIconButS(block, ROW, B_SORTIMASELLIST, ICON_SORTSIZE,	xco+=XIC,0,XIC,YIC, &simasel->sort, 1.0, 2.0, 0, 0, "Sorts files by size");	
	uiBlockEndAlign(block);

	cpack(0x0);
	xco+=XIC+10;
	uiDefIconButBitS(block, TOG, FILE_BOOKMARKS, B_RELOADIMASELDIR, ICON_BOOKMARKS,xco+=XIC,0,XIC,YIC, &simasel->flag, 0, 0, 0, 0, "Toggles Bookmarks on/off");
	xco+=XIC+10;

	xcotitle= xco;
	xco+= BIF_GetStringWidth(G.font, simasel->title, (U.transopts & USER_TR_BUTTONS));
	
	if(simasel->pupmenu && simasel->menup) {
		uiDefButS(block, MENU, B_NOP, simasel->pupmenu, xco+10,0,90,20, simasel->menup, 0, 0, 0, 0, "");
		xco+= 100;
	}
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, TOG, FILE_HIDE_DOT, B_RELOADIMASELDIR, ICON_GHOST,xco+=XIC,0,XIC,YIC, &simasel->flag, 0, 0, 0, 0, "Hides dot files");		
	uiBlockEndAlign(block);
	xco+=20;
		
	uiDefIconButBitS(block, TOG, FILE_FILTER, B_FILTERIMASELDIR, ICON_SORTBYEXT,xco+=XIC,0,XIC,YIC, &simasel->flag, 0, 0, 0, 0, "Filter files");
	if (simasel->flag & FILE_FILTER) {
		xco+=4;
		uiBlockBeginAlign(block);
		uiDefIconButBitS(block, TOG, IMAGEFILE, B_FILTERIMASELDIR, ICON_IMAGE_COL,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show images");
		uiDefIconButBitS(block, TOG, BLENDERFILE, B_FILTERIMASELDIR, ICON_BLENDER,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show .blend files");
		uiDefIconButBitS(block, TOG, MOVIEFILE, B_FILTERIMASELDIR, ICON_SEQUENCE,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show movies");
		uiDefIconButBitS(block, TOG, PYSCRIPTFILE, B_FILTERIMASELDIR, ICON_PYTHON,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show python scripts");
		uiDefIconButBitS(block, TOG, FTFONTFILE, B_FILTERIMASELDIR, ICON_SYNTAX,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show fonts");
		uiDefIconButBitS(block, TOG, SOUNDFILE, B_FILTERIMASELDIR, ICON_SOUND,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show sound files");
		uiDefIconButBitS(block, TOG, TEXTFILE, B_FILTERIMASELDIR, ICON_TEXT,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show text files");
		uiDefIconButBitS(block, TOG, FOLDERFILE, B_FILTERIMASELDIR, ICON_FILESEL,xco+=XIC,0,XIC,YIC, &simasel->filter, 0, 0, 0, 0, "Show folders");
		uiBlockEndAlign(block);
	}

	uiDefButBitS(block, TOG, FILE_STRINGCODE, 0, "Relative Paths", xco+=XIC+20,0,100,YIC, &simasel->flag, 0, 0, 0, 0, "Makes sure returned paths are relative to the current .blend file");
	xco+=90;

	if(simasel->type==FILE_LOADLIB) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOGN, FILE_LINK, B_REDR, "Append",		xco+=XIC,0,100,YIC, &simasel->flag, 0, 0, 0, 0, "Copies selected data into current project");
		uiDefButBitS(block, TOG, FILE_LINK, B_REDR, "Link",	xco+=100,0,100,YIC, &simasel->flag, 0, 0, 0, 0, "Creates a link to selected data from current project");
		uiBlockEndAlign(block);
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, FILE_AUTOSELECT, B_REDR, "Autosel", xco+=125,0,65,YIC, &simasel->flag, 0, 0, 0, 0, "Autoselect imported objects");
		uiDefButBitS(block, TOG, FILE_ACTIVELAY, B_REDR, "Active Layer", xco+=65,0,80,YIC, &simasel->flag, 0, 0, 0, 0, "Append object(s) in active layer");
		uiDefButBitS(block, TOG, FILE_ATCURSOR, B_REDR, "At Cursor", xco+=80,0,65,YIC, &simasel->flag, 0, 0, 0, 0, "Append object(s) at cursor, use centroid if more than one object is selected");
		uiBlockEndAlign(block);
		
		xco+= 100;	// scroll
		
	} else if(simasel->type==FILE_BLENDER) {
		uiDefButBitI(block, TOGN, G_FILE_NO_UI, B_REDR, "Load UI", xco+=XIC,0,80,YIC, &G.fileflags, 0, 0, 0, 0, "Load the UI setup as well as the scene data");
	
		xco+= 100;	// scroll
	}

	glRasterPos2f((float)xcotitle, 5.0);
	BIF_RasterPos((float)xcotitle, 5.0);	// stupid texture fonts
	BIF_ThemeColor(TH_TEXT);
	BIF_DrawString(uiBlockGetCurFont(block), simasel->title, (U.transopts & USER_TR_BUTTONS));

	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}