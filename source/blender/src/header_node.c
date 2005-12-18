/**
 *
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_butspace.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BSE_headerbuttons.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

void do_node_buttons(ScrArea *sa, unsigned short event)
{
//	SpaceNode *snode= sa->spacedata.first;
	
	switch(event) {
		;
	}
}


void node_buttons(ScrArea *sa)
{
//	SpaceNode *snode= sa->spacedata.first;
	uiBlock *block;
	short xco;
	char name[256];
	
	sprintf(name, "header %d", sa->headwin);
	block= uiNewBlock(&sa->uiblocks, name, UI_EMBOSS, UI_HELV, sa->headwin);

	if(area_is_active_area(sa)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);
	
	sa->butspacetype= SPACE_NODE;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, 
					  windowtype_pup(), xco, 0, XIC+10, YIC, 
					  &(sa->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
					  "Displays Current Window Type. "
					  "Click for menu of available types.");

	xco += XIC + 14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if (sa->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  xco,2,XIC,YIC-2,
					  &(sa->flag), 0, 0, 0, 0, 
					  "Show pulldown menus");
	}
	else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  xco,2,XIC,YIC-2,
					  &(sa->flag), 0, 0, 0, 0, 
					  "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
//		xmax= GetButStringLength("View");
//		uiDefPulldownBut(block, time_viewmenu, NULL, 
//					  "View", xco, -2, xmax-3, 24, "");
//		xco+= xmax;
	}
	
	/* always as last  */
	sa->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
