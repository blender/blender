/**
 * header_action.c oct-2003
 *
 * Functions to draw the "Action Editor" window header
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
#include "DNA_action_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BKE_action.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_drawipo.h"
#include "BSE_editaction.h"
#include "BSE_headerbuttons.h"

#include "nla.h"

#include "blendef.h"
#include "interface.h"
#include "mydevice.h"

void do_action_buttons(unsigned short event)
{

  switch(event){
#ifdef __NLA_BAKE
  case B_ACTBAKE:
    bake_action_with_client (G.saction->action, OBACT, 0.01);
    break;
#endif
  case B_ACTCONT:
    set_exprap_action(IPO_HORIZ);
    break;
//  case B_ACTEXTRAP:
//    set_exprap_ipo(IPO_DIR);
//    break;
  case B_ACTCYCLIC:
    set_exprap_action(IPO_CYCL);
    break;
//  case B_ACTCYCLICX:
//    set_exprap_ipo(IPO_CYCLX);
//    break;
  case B_ACTHOME:
    //  Find X extents
    //v2d= &(G.saction->v2d);

    G.v2d->cur.xmin = 0;
    G.v2d->cur.ymin=-SCROLLB;
    
    if (!G.saction->action){  // here the mesh rvk?
      G.v2d->cur.xmax=100;
    }
    else {
      float extra;
      G.v2d->cur.xmin= calc_action_start(G.saction->action);
      G.v2d->cur.xmax= calc_action_end(G.saction->action);
      extra= 0.05*(G.v2d->cur.xmax - G.v2d->cur.xmin);
      G.v2d->cur.xmin-= extra;
      G.v2d->cur.xmax+= extra;
    }

    G.v2d->tot= G.v2d->cur;
    test_view2d(G.v2d, curarea->winx, curarea->winy);


    addqueue (curarea->win, REDRAW, 1);

    break;
  case B_ACTCOPY:
    copy_posebuf();
    allqueue(REDRAWVIEW3D, 1);
    break;
  case B_ACTPASTE:
    paste_posebuf(0);
    allqueue(REDRAWVIEW3D, 1);
    break;
  case B_ACTPASTEFLIP:
    paste_posebuf(1);
    allqueue(REDRAWVIEW3D, 1);
    break;

  case B_ACTPIN:  /* __PINFAKE */
/*    if (G.saction->flag & SACTION_PIN){
      if (G.saction->action)
        G.saction->action->id.us ++;

    }
    else {
      if (G.saction->action)
        G.saction->action->id.us --;
    }
*/    /* end PINFAKE */
    allqueue(REDRAWACTION, 1);
    break;

  }
}

void action_buttons(void)
{
	uiBlock *block;
	short xco;
	char naam[256];
	Object *ob;
	ID *from;

	if (!G.saction)
		return;

	// copy from drawactionspace....
	if (!G.saction->pin) {
		if (OBACT)
			G.saction->action = OBACT->action;
		else
			G.saction->action=NULL;
	}

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTPINK);

	curarea->butspacetype= SPACE_ACTION;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_ACTHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");

	
	if (!get_action_mesh_key()) {
		/* NAME ETC */
		ob=OBACT;
		from = (ID*) ob;

		xco= std_libbuttons(block, xco+1.5*XIC, 0, B_ACTPIN, &G.saction->pin, 
							B_ACTIONBROWSE, (ID*)G.saction->action, 
							from, &(G.saction->actnr), B_ACTALONE, 
							B_ACTLOCAL, B_ACTIONDELETE, 0, 0);	

#ifdef __NLA_BAKE
		/* Draw action baker */
		uiDefBut(block, BUT, B_ACTBAKE, "Bake", 
				 xco+=XIC, 0, 64, YIC, 0, 0, 0, 0, 0, 
				 "Generate an action with the constraint "
				 "effects converted into ipo keys");
		xco+=64;
#endif
	}
	uiClearButLock();

	/* draw LOCK */
	xco+= XIC/2;
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.saction->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");


	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
