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

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
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
	short xco;
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
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTALPHA,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 0.0, 0, 0, "Sorts files alphabetically");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTTIME,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 1.0, 0, 0, "Sorts files by time");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTSIZE,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 2.0, 0, 0, "Sorts files by size");	

	cpack(0x0);
	glRasterPos2i(xco+=XIC+10,	5);

	BIF_DrawString(uiBlockGetCurFont(block), sfile->title, (U.transopts & USER_TR_BUTTONS));
	xco+= BIF_GetStringWidth(G.font, sfile->title, (U.transopts & USER_TR_BUTTONS));
	
	uiDefIconButS(block, ICONTOG|BIT|0, B_SORTFILELIST, ICON_LONGDISPLAY,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Toggles long info");
	uiDefIconButS(block, TOG|BIT|3, B_RELOADDIR, ICON_GHOST,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Hides dot files");

	xco+=XIC+10;

	if(sfile->type==FILE_LOADLIB) {
		uiDefButS(block, TOGN|BIT|2, B_REDR, "Append",		xco+=XIC,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Copies selected data into current project");
		uiDefButS(block, TOG|BIT|2, B_REDR, "Link",	xco+=100,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Creates a link to selected data from current project");
	}

	if(sfile->type==FILE_UNIX) {
		df= BLI_diskfree(sfile->dir)/(1048576.0);

		filesel_statistics(sfile, &totfile, &selfile, &totlen, &sellen);
		
		sprintf(naam, "Free: %.3f Mb   Files: (%d) %d    (%.3f) %.3f Mb", df, selfile,totfile, sellen, totlen);

		cpack(0x0);
		glRasterPos2i(xco,	5);

		BIF_DrawString(uiBlockGetCurFont(block), naam, 0);
	}
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
