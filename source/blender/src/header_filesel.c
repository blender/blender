/**
 * header_filesel.c oct-2003
 *
 * Functions to draw the "File Browser" window header
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

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_language.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BLI_blenlib.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"
#include "mydevice.h"

void do_file_buttons(short event)
{
	SpaceFile *sfile;

	if(curarea->win==0) return;
	sfile= curarea->spacedata.first;

	switch(event) {
	case B_SORTFILELIST:
		sort_filelist(sfile);
		scrarea_queue_winredraw(curarea);
		break;
	case B_RELOADDIR:
		freefilelist(sfile);
		scrarea_queue_winredraw(curarea);
		break;
	}
	
}

void file_buttons(void)
{
	SpaceFile *sfile;
	uiBlock *block;
	float df, totlen, sellen;
	short xco, xcotitle;
	int totfile, selfile;
	char naam[256];

	sfile= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_FILE;
	
	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;
	
	/* FULL WINDOW */
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* SORT TYPE */
	xco+=XIC;
	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTALPHA,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 0.0, 0, 0, "Sorts files alphabetically");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTBYEXT,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 3.0, 0, 0, "Sorts files by extension");	
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTTIME,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 1.0, 0, 0, "Sorts files by time");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTSIZE,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 2.0, 0, 0, "Sorts files by size");	
	uiBlockEndAlign(block);

	cpack(0x0);
	xco+=XIC+10;

	xcotitle= xco;
	xco+= BIF_GetStringWidth(G.font, sfile->title, (U.transopts & USER_TR_BUTTONS));
	
	if(sfile->pupmenu && sfile->menup) {
		uiDefButS(block, MENU, B_NOP, sfile->pupmenu, xco+10,0,90,20, sfile->menup, 0, 0, 0, 0, "");
		xco+= 100;
	}
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, FILE_SHOWSHORT, B_SORTFILELIST, ICON_LONGDISPLAY,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Toggles long info");
	uiDefIconButBitS(block, TOG, FILE_HIDE_DOT, B_RELOADDIR, ICON_GHOST,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Hides dot files");
	uiBlockEndAlign(block);
	
	uiDefButBitS(block, TOG, FILE_STRINGCODE, 0, "Relative Paths", xco+=XIC+20,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Makes sure returned paths are relative to the current .blend file");

	xco+=90;

	if(sfile->type==FILE_LOADLIB) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOGN, FILE_LINK, B_REDR, "Append",		xco+=XIC,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Copies selected data into current project");
		uiDefButBitS(block, TOG, FILE_LINK, B_REDR, "Link",	xco+=100,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Creates a link to selected data from current project");
		uiBlockEndAlign(block);
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, FILE_AUTOSELECT, B_REDR, "Autosel", xco+=125,0,65,YIC, &sfile->flag, 0, 0, 0, 0, "Autoselect imported objects");
		uiDefButBitS(block, TOG, FILE_ACTIVELAY, B_REDR, "Active Layer", xco+=65,0,80,YIC, &sfile->flag, 0, 0, 0, 0, "Append object(s) in active layer");
		uiDefButBitS(block, TOG, FILE_ATCURSOR, B_REDR, "At Cursor", xco+=80,0,65,YIC, &sfile->flag, 0, 0, 0, 0, "Append object(s) at cursor, use centroid if more than one object is selected");
		uiBlockEndAlign(block);
		
		xco+= 100;	// scroll
		
	} else if(sfile->type==FILE_BLENDER) {
		uiDefButBitI(block, TOGN, G_FILE_NO_UI, B_REDR, "Load UI", xco+=XIC,0,80,YIC, &G.fileflags, 0, 0, 0, 0, "Load the UI setup as well as the scene data");
	
		xco+= 100;	// scroll
	}
	else if(sfile->type==FILE_LOADFONT) {
		uiDefIconButBitS(block, TOG, FILE_SHOWSHORT, B_SORTFILELIST, ICON_FONTPREVIEW, xco+= XIC, 0, XIC, YIC, &sfile->f_fp, 0, 0, 0, 0, "Activate font preview");
		if (sfile->f_fp)
			uiDefButC(block, FTPREVIEW, 0, "Font preview", xco+= XIC, 0, 100, YIC, sfile->fp_str, (float)0, (float)16, 0, 0, "Font preview");
	
		xco+= 100;	// scroll
	}

	uiDrawBlock(block);
	
	glRasterPos2f((float)xcotitle, 5.0);
	BIF_RasterPos((float)xcotitle, 5.0);	// stupid texture fonts
	BIF_ThemeColor(TH_TEXT);
	BIF_DrawString(uiBlockGetCurFont(block), sfile->title, (U.transopts & USER_TR_BUTTONS));
	
	if(sfile->type==FILE_UNIX) {
		df= BLI_diskfree(sfile->dir)/(1048576.0);

		filesel_statistics(sfile, &totfile, &selfile, &totlen, &sellen);
		
		sprintf(naam, "Free: %.3f MB   Files: (%d) %d    (%.3f) %.3f MB", df, selfile,totfile, sellen, totlen);

		cpack(0x0);
		glRasterPos2f((float)xco, 5.0);
		BIF_RasterPos((float)xco, 5.0);	// texture fonts
	
		BIF_DrawString(G.font, naam, 0);
	}
	
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;
}
