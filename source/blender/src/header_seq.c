/**
 * header_seq.c oct-2003
 *
 * Functions to draw the "Video Sequence Editor" window header
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
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_sequence.h"

#include "blendef.h"
#include "mydevice.h"

static int viewmovetemp = 0;

void do_seq_buttons(short event)
{
	Editing *ed;

	ed= G.scene->ed;
	if(ed==0) return;

	switch(event) {
	case B_SEQHOME:
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	case B_SEQCLEAR:
		free_imbuf_seq();
		allqueue(REDRAWSEQ, 1);
		break;
	}
}

void seq_buttons()
{
	SpaceSeq *sseq;
	short xco;
	char naam[20];
	uiBlock *block;

	sseq= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_SEQ;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;
	
	/* FULL WINDOW */
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_SEQHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");	
	xco+= XIC;
	
	/* IMAGE */
	uiDefIconButS(block, TOG, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &sseq->mainb, 0, 0, 0, 0, "Toggles image display");

	/* ZOOM and BORDER */
	xco+= XIC;
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view in and out (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to fit area");

	/* CLEAR MEM */
	xco+= XIC;
	uiDefBut(block, BUT, B_SEQCLEAR, "Clear",	xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Forces a clear of all buffered images in memory");
	
	uiDrawBlock(block);
}
