/**
 * header_oops.c oct-2003
 *
 * Functions to draw the "OOPS Schematic" window header
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
#include "DNA_oops_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_drawipo.h"
#include "BSE_drawoops.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"

static int viewmovetemp = 0;

void do_oops_buttons(short event)
{
	float dx, dy;
	
	if(curarea->win==0) return;

	switch(event) {
	case B_OOPSHOME:
		boundbox_oops();
		G.v2d->cur= G.v2d->tot;
		dx= 0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin);
		dy= 0.15*(G.v2d->cur.ymax-G.v2d->cur.ymin);
		G.v2d->cur.xmin-= dx;
		G.v2d->cur.xmax+= dx;
		G.v2d->cur.ymin-= dy;
		G.v2d->cur.ymax+= dy;		
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;

	case B_NEWOOPS:
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		G.soops->lockpoin= 0;
		break;
	}

}

void oops_buttons(void)
{
	SpaceOops *soops;
	Oops *oops;
	uiBlock *block;
	short xco;
	char naam[256];

	soops= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREEN);

	curarea->butspacetype= SPACE_OOPS;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;

	/* FULL WINDOW */
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_OOPSHOME, ICON_HOME,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");	
	xco+= XIC;
	
	/* ZOOM and BORDER */
	xco+= XIC;
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	(short)(xco+=XIC),0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");

	/* VISIBLE */
	xco+= XIC;
	uiDefButS(block, TOG|BIT|10,B_NEWOOPS, "lay",		(short)(xco+=XIC),0,XIC+10,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Objects based on layer");
	uiDefIconButS(block, TOG|BIT|0, B_NEWOOPS, ICON_SCENE_HLT,	(short)(xco+=XIC+10),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Scene data");
	uiDefIconButS(block, TOG|BIT|1, B_NEWOOPS, ICON_OBJECT_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Object data");
	uiDefIconButS(block, TOG|BIT|2, B_NEWOOPS, ICON_MESH_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Mesh data");
	uiDefIconButS(block, TOG|BIT|3, B_NEWOOPS, ICON_CURVE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Curve/Surface/Font data");
	uiDefIconButS(block, TOG|BIT|4, B_NEWOOPS, ICON_MBALL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Metaball data");
	uiDefIconButS(block, TOG|BIT|5, B_NEWOOPS, ICON_LATTICE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lattice data");
	uiDefIconButS(block, TOG|BIT|6, B_NEWOOPS, ICON_LAMP_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lamp data");
	uiDefIconButS(block, TOG|BIT|7, B_NEWOOPS, ICON_MATERIAL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Material data");
	uiDefIconButS(block, TOG|BIT|8, B_NEWOOPS, ICON_TEXTURE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Texture data");
	uiDefIconButS(block, TOG|BIT|9, B_NEWOOPS, ICON_IPO_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Ipo data");
	uiDefIconButS(block, TOG|BIT|12, B_NEWOOPS, ICON_IMAGE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Image data");
	uiDefIconButS(block, TOG|BIT|11, B_NEWOOPS, ICON_LIBRARY_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Library data");

	/* name */
	if(G.soops->lockpoin) {
		oops= G.soops->lockpoin;
		if(oops->type==ID_LI) strcpy(naam, ((Library *)oops->id)->name);
		else strcpy(naam, oops->id->name);
		
		cpack(0x0);
		glRasterPos2i(xco+=XIC+10,	5);
		BMF_DrawString(uiBlockGetCurFont(block), naam);

	}

	/* always do as last */
	curarea->headbutlen= xco+2*XIC;
	
	uiDrawBlock(block);
}
