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
#include "BIF_editoops.h"
#include "BIF_oops.h"
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

static void do_oops_viewmenu(void *arg, int event)
{

	switch(event) {
	case 0: /* Shuffle Selected Blocks */
		shuffle_oops();
		break;
	case 1: /* Shrink Selected Blocks */
    	shrink_oops();
        break;
    case 2: /* View All */
		do_oops_buttons(B_OOPSHOME);
		break;
	case 3: /* Maximize Window */
		/* using event B_FULL */
		break;
	}
}			

static uiBlock *oops_viewmenu(void *arg_unused)
{
/*	static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "oops_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_oops_viewmenu, NULL);
		
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shuffle Selected Blocks|Shift S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
    uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink Selected Blocks|Alt S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	
	return block;
}


static void do_oops_selectmenu(void *arg, int event)
{	

	switch(event)
	{
	case 0: /* Border Select */
		borderselect_oops();
		break;
	case 1: /* Select/Deselect All */
		swap_select_all_oops();
		break;
	case 2: /* Linked to Selected */
		select_linked_oops();
		break;
	case 3: /* Users of Selected */
		select_backlinked_oops();
		break;
	}
}

static uiBlock *oops_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "oops_selectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_oops_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");    

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked to Selected|L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Users of Selected|Shift L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}


void oops_buttons(void)
{	
	SpaceOops *soops;
	Oops *oops;
	uiBlock *block;
	short xco, xmax;
	char naam[256];

	soops= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_OOPS;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Show pulldown menus");
	} else {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
	/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefBlockBut(block, oops_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefBlockBut(block, oops_selectmenu, NULL, "Select", xco, -2, xmax-3, 24, "");
		xco+= xmax;

	}

	uiBlockSetEmboss(block, UI_EMBOSSX);

	xco+= 8;
	
	/* ZOOM and BORDER */
    uiBlockBeginAlign(block);
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	(short)(xco),0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (Ctrl MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");
    uiBlockEndAlign(block);
    
	xco+= 8;
    
	/* VISIBLE */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|10,B_NEWOOPS, "Layer",		(short)(xco+=XIC),0,XIC+20,YIC, &soops->visiflag, 0, 0, 0, 0, "Only show object datablocks on visible layers");
    xco+= 20;
	uiDefIconButS(block, TOG|BIT|0, B_NEWOOPS, ICON_SCENE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Scene datablocks");
	uiDefIconButS(block, TOG|BIT|1, B_NEWOOPS, ICON_OBJECT_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Object datablocks");
	uiDefIconButS(block, TOG|BIT|2, B_NEWOOPS, ICON_MESH_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Mesh datablocks");
	uiDefIconButS(block, TOG|BIT|3, B_NEWOOPS, ICON_CURVE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Curve/Surface/Font datablocks");
	uiDefIconButS(block, TOG|BIT|4, B_NEWOOPS, ICON_MBALL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Metaball datablocks");
	uiDefIconButS(block, TOG|BIT|5, B_NEWOOPS, ICON_LATTICE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lattice datablocks");
	uiDefIconButS(block, TOG|BIT|6, B_NEWOOPS, ICON_LAMP_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lamp datablocks");
	uiDefIconButS(block, TOG|BIT|7, B_NEWOOPS, ICON_MATERIAL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Material datablocks");
	uiDefIconButS(block, TOG|BIT|8, B_NEWOOPS, ICON_TEXTURE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Texture datablocks");
	uiDefIconButS(block, TOG|BIT|9, B_NEWOOPS, ICON_IPO_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Ipo datablocks");
	uiDefIconButS(block, TOG|BIT|12, B_NEWOOPS, ICON_IMAGE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Image datablocks");
	uiDefIconButS(block, TOG|BIT|11, B_NEWOOPS, ICON_LIBRARY_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Library datablocks");

    uiBlockEndAlign(block);
  
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

