/**
 * header_nla.c oct-2003
 *
 * Functions to draw the "NLA Editor" window header
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "DNA_nla_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"

//#include "nla.h"

#include "blendef.h"
#include "interface.h"
#include "mydevice.h"

void do_nla_buttons(unsigned short event)
{
  View2D  *v2d;

  switch(event){
  case B_NLAHOME:
    //  Find X extents
    v2d= &(G.snla->v2d);

    v2d->cur.xmin = G.scene->r.sfra;
    v2d->cur.ymin=-SCROLLB;
    
//    if (!G.saction->action){
      v2d->cur.xmax=G.scene->r.efra;
//    }
//    else
//    {
//      v2d->cur.xmax=calc_action_length(G.saction->action)+1;
//    }
    
    test_view2d(G.v2d, curarea->winx, curarea->winy);
    addqueue (curarea->win, REDRAW, 1);
    break;
  }
}

void nla_buttons(void)
{
	SpaceNla *snla;
	short xco;
	char naam[20];
	uiBlock *block;
	
	snla= curarea->spacedata.first;
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTCHOKE);

	curarea->butspacetype= SPACE_NLA;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_NLAHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");	
	xco+= XIC;
	
	/* IMAGE */
//	uiDefIconButS(block, TOG, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &sseq->mainb, 0, 0, 0, 0, "Toggles image display");

	/* ZOOM en BORDER */
//	xco+= XIC;
//	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");
//	uiDefIconBut(block, BUT, B_NLABORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zoom view to area");

	/* draw LOCK */
	xco+= XIC/2;

	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(snla->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");

	uiDrawBlock(block);
}
