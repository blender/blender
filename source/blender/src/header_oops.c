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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: not all of this file anymore.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_oops_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_editoops.h"
#include "BIF_oops.h"
#include "BIF_outliner.h"
#include "BIF_space.h"

#include "BSE_drawipo.h"
#include "BSE_drawoops.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"

#include "BKE_depsgraph.h"

#include "BPY_extern.h"
#include "BPY_menus.h"

static int viewmovetemp = 0;

void do_oops_buttons(short event)
{
	float dx, dy;
	
	/* used for maximize hack */
	int win_width, win_height; 
	float aspect_win, aspect_oops, oops_width, oops_height, oops_x_mid, oops_y_mid;
	
	
	if(curarea->win==0) return;
	
	if (event == B_NEWOOPS) {
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		G.soops->lockpoin= 0;
	} else { /* must be either B_OOPSHOME or B_OOPSVIEWSEL */
		init_v2d_oops(curarea, curarea->spacedata.first);	// forces min/max to be reset
		if (event == B_OOPSHOME) {
			boundbox_oops(0); /* Test all oops blocks */
		} else {
			boundbox_oops(1); /* Test only selected oops blocks */
		}
		
		
		/* Hack to work with test_view2d in drawipo.c
		Modify the bounding box so it is maximized to the window aspect
		so viewing all oops blocks isnt limited to hoz/vert only.
		Cant modify drawipo.c because many other functions use this hos/vert operation - Campbell*/
		
		win_width= curarea->winrct.xmax - curarea->winrct.xmin;
		win_height= curarea->winrct.ymax - curarea->winrct.ymin;
		
		oops_width = G.v2d->tot.xmax - G.v2d->tot.xmin;
		oops_height = G.v2d->tot.ymax - G.v2d->tot.ymin;
		
		oops_x_mid = (G.v2d->tot.xmax + G.v2d->tot.xmin)*0.5;
		oops_y_mid = (G.v2d->tot.ymax + G.v2d->tot.ymin)*0.5;
		/* wide windows will be above 1, skinny below 1 */
		aspect_win= (float)win_width / (float)win_height; 
		aspect_oops = (float)oops_width / (float)oops_height; 
		if (aspect_win>aspect_oops) {/* the window is wider then the oops bounds, increase the oops width */
			G.v2d->tot.xmin = oops_x_mid - ((oops_x_mid-G.v2d->tot.xmin) * (aspect_win/aspect_oops) ); /* scale the min */
			G.v2d->tot.xmax = oops_x_mid + ((G.v2d->tot.xmax-oops_x_mid) * (aspect_win/aspect_oops) );/* scale the max */
		} else { /* the window is skinnier then the oops bounds, increase the oops height */
			G.v2d->tot.ymin = oops_y_mid - ((oops_y_mid-G.v2d->tot.ymin) * (aspect_oops/aspect_win) ); /* scale the min */
			G.v2d->tot.ymax = oops_y_mid + ((G.v2d->tot.ymax-oops_y_mid) * (aspect_oops/aspect_win) );/* scale the max */
		}
		
		/* maybe we should restore the correct values? - do next of its needed */
		/* end hack */
		
		
		
		G.v2d->cur= G.v2d->tot;
		dx= 0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin);
		dy= 0.15*(G.v2d->cur.ymax-G.v2d->cur.ymin);
		G.v2d->cur.xmin-= dx;
		G.v2d->cur.xmax+= dx;
		G.v2d->cur.ymin-= dy;
		G.v2d->cur.ymax+= dy;		
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
	}
}

static void do_oops_viewmenu(void *arg, int event)
{
	SpaceOops *soops= curarea->spacedata.first;

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
    case 3: /* View All */
		do_oops_buttons(B_OOPSVIEWSEL);
		break;
	case 4: /* Maximize Window */
		/* using event B_FULL */
		break;
	case 5: /* show outliner */
		if(soops->type==SO_OOPS || soops->type==SO_DEPSGRAPH) soops->type= SO_OUTLINER;
		else soops->type= SO_OOPS;
		init_v2d_oops(curarea, soops);
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	case 6:
		outliner_toggle_visible(curarea);
		break;
	case 7:
		outliner_show_hierarchy(curarea);
		break;
	case 8:
		outliner_show_active(curarea);
		break;
	case 9:
		outliner_one_level(curarea, 1);
		break;
	case 10:
		outliner_one_level(curarea, -1);
		break;
#ifdef SHOWDEPGRAPH
	case 11:
		// show deps
		{
			SpaceOops *soops= curarea->spacedata.first;
			if(soops->type==SO_OOPS) {
				soops->type= SO_DEPSGRAPH;
				soops->deps_flags = DAG_RL_ALL_BUT_DATA_MASK;
			} else 
				soops->type= SO_OOPS;
			init_v2d_oops(curarea, soops);
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			scrarea_queue_winredraw(curarea);
		}
		break;
#endif
	case 12:
		if (soops->flag & SO_HIDE_RESTRICTCOLS) soops->flag &= ~SO_HIDE_RESTRICTCOLS;
		else soops->flag |= SO_HIDE_RESTRICTCOLS;
		break;
	}
}			

static uiBlock *oops_viewmenu(void *arg_unused)
{
	SpaceOops *soops= curarea->spacedata.first;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "oops_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_oops_viewmenu, NULL);
	
	if(soops->type==SO_OOPS) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Outliner", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
#ifdef SHOWDEPGRAPH
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Dependancies", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
#endif
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shuffle Selected Blocks|Shift S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink Selected Blocks|Alt S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View Selected|NumPad .", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	}
#ifdef SHOWDEPGRAPH
	else if(soops->type==SO_DEPSGRAPH) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Outliner", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Oops Schematic", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	}
#endif
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Oops Schematic", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
		
		if (soops->flag & SO_HIDE_RESTRICTCOLS)
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Show Restriction Columns", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		else
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Show Restriction Columns", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");

		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Expand One Level|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Collapse One Level|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");

		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
			
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show/Hide All", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hierarchy|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Active|NumPad .", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	}
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
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

static void do_oops_blockmenu(void *arg, int event)
{	
	switch(event)
	{
	case 0: /* grab/move */
		transform_oops('g', 0);
		break;
	case 1: /* scale */
		transform_oops('s', 0);
		break;
	}
}

static uiBlock *oops_blockmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "oops_blockmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_oops_blockmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Scale|S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

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

static void do_oops_searchmenu(void *arg, int event)
{	
	int search_flags = OL_FIND, again = 0;
	
	switch(event)
	{
	case 0: /* plain new find */	
		search_flags = OL_FIND;
		break;
	case 1: /* case sensitive */
		search_flags = OL_FIND_CASE;
		break;
	case 2: /* full search */
		search_flags = OL_FIND_COMPLETE;
		break;
	case 3: /* full case sensitive */
		search_flags = OL_FIND_COMPLETE_CASE;
		break;
	case 4: /* again */
		again = 1;
		break;
	default: /* nothing valid */
		return;
	}
	
	/* run search */
	outliner_find_panel(curarea, again, search_flags);
}

static uiBlock *oops_searchmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "oops_searchmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_oops_searchmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find|F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find (Case Sensitive)|Ctrl F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find Complete|Alt F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find Complete (Case Sensitive)|Ctrl Alt F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");  
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find Again|Shift F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");

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
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Show pulldown menus");
	} else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, oops_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		if(soops->type==SO_OOPS) {
			xmax= GetButStringLength("Select");
			uiDefPulldownBut(block, oops_selectmenu, NULL, "Select", xco, -2, xmax-3, 24, "");
			xco+= xmax;
			
			xmax= GetButStringLength("Block");
			uiDefPulldownBut(block, oops_blockmenu, NULL, "Block", xco, -2, xmax-3, 24, "");
			xco+= xmax;
			
		}
		else {
			xmax= GetButStringLength("Search");
			uiDefPulldownBut(block, oops_searchmenu, NULL, "Search", xco, -2, xmax-3, 24, "");
			xco+= xmax;
		}
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	if(soops->type==SO_OOPS) {
		/* ZOOM and BORDER */
		uiBlockBeginAlign(block);
		uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	(short)(xco),0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (Ctrl MiddleMouse)");
		uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");
		uiBlockEndAlign(block);
		
		xco+= 8;
		
		/* VISIBLE */
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, OOPS_LAY, B_NEWOOPS, "Layer",		(short)(xco+=XIC),0,XIC+20,YIC, &soops->visiflag, 0, 0, 0, 0, "Only show object datablocks on visible layers");
		xco+= 20;
		uiDefIconButBitS(block, TOG, OOPS_SCE, B_NEWOOPS, ICON_SCENE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Scene datablocks");
		uiDefIconButBitS(block, TOG, OOPS_OB, B_NEWOOPS, ICON_OBJECT_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Object datablocks");
		uiDefIconButBitS(block, TOG, OOPS_ME, B_NEWOOPS, ICON_MESH_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Mesh datablocks");
		uiDefIconButBitS(block, TOG, OOPS_CU, B_NEWOOPS, ICON_CURVE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Curve/Surface/Font datablocks");
		uiDefIconButBitS(block, TOG, OOPS_MB, B_NEWOOPS, ICON_MBALL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Metaball datablocks");
		uiDefIconButBitS(block, TOG, OOPS_LT, B_NEWOOPS, ICON_LATTICE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lattice datablocks");
		uiDefIconButBitS(block, TOG, OOPS_LA, B_NEWOOPS, ICON_LAMP_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lamp datablocks");
		uiDefIconButBitS(block, TOG, OOPS_MA, B_NEWOOPS, ICON_MATERIAL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Material datablocks");
		uiDefIconButBitS(block, TOG, OOPS_TE, B_NEWOOPS, ICON_TEXTURE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Texture datablocks");
		uiDefIconButBitS(block, TOG, OOPS_IP, B_NEWOOPS, ICON_IPO_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Ipo datablocks");
		uiDefIconButBitS(block, TOG, OOPS_IM, B_NEWOOPS, ICON_IMAGE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Image datablocks");
		uiDefIconButBitS(block, TOG, OOPS_GR, B_NEWOOPS, ICON_CIRCLE_DEHLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Group datablocks");
		uiDefIconButBitS(block, TOG, OOPS_LI, B_NEWOOPS, ICON_LIBRARY_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Library datablocks");
		uiDefIconButBitS(block, TOG, OOPS_CA, B_NEWOOPS, ICON_CAMERA_DEHLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Camera datablocks");
		uiDefIconButBitS(block, TOG, OOPS_AR, B_NEWOOPS, ICON_ARMATURE,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Armature datablocks");
		
		uiBlockEndAlign(block);
	  
		/* name */
		if(G.soops->lockpoin) {
			oops= G.soops->lockpoin;
			if(oops->type==ID_LI) strcpy(naam, ((Library *)oops->id)->name);
			else strcpy(naam, oops->id->name);
			
			cpack(0x0);
            BIF_ThemeColor(TH_MENU_TEXT); /* makes text readable on dark theme */
            BIF_SetScale(1.0);
			glRasterPos2i(xco+=XIC+10,	5);
			BIF_RasterPos(xco+=XIC+10,	5);
			BIF_DrawString(G.font, naam, 0);

		}
	}
#ifdef SHOWDEPGRAPH
	else if(soops->type==SO_DEPSGRAPH) {
		// cpack colors : 0x00FF00 0xFF0000 0xFFFF00 0x000000 0x0000FF 0x00FFFF
		static unsigned char colr[21] ={0x00, 0xFF, 0x00, 
										0x00, 0x00, 0xFF,
										0x00, 0xFF, 0xFF,
										0x00, 0x00, 0x00,
										0xFF, 0x00, 0x00,
										0xFF, 0xFF, 0x00,
										0xFF, 0x00, 0x00};
		
		uiDefButC(	block, COL, 0, "", (short)(xco+=10),0, 5,YIC, colr, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|2, B_REDR, "parent", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "parent");
		uiDefButC(	block, COL, 0, "", (short)(xco+=60),0, 5,YIC, colr+3, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|1, B_REDR, "data", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "data");
		uiDefButC(	block, COL, 0, "", (short)(xco+=60),0, 5,YIC, colr+6, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|3, B_REDR, "track", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "track");
		uiDefButC(	block, COL, 0, "", (short)(xco+=60),0, 5,YIC, colr+9, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|4, B_REDR, "path", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "path");
		uiDefButC(	block, COL, 0, "", (short)(xco+=60),0, 5,YIC, colr+12, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|5, B_REDR, "cons.", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "constraint");
		uiDefButC(	block, COL, 0, "", (short)(xco+=60),0, 5,YIC, colr+15, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|6, B_REDR, "hook.", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "hook");
		uiDefButC(	block, COL, 0, "", (short)(xco+=60),0, 5,YIC, colr+18, 0, 1, 0, 0,  "");
		uiDefButS(	block, TOG|BIT|7, B_REDR, "d cons.", (short)(xco+=7),0, 50,YIC, &soops->deps_flags, 0, 1, 0, 0,  "d cons");
	} 
#endif
	else {
		if(G.main->library.first) 
#ifdef WITH_VERSE
			uiDefButS(block, MENU, B_REDR, "Outliner Display%t|Verse Servers %x9|Verse Sessions %x8|Libraries %x7|All Scenes %x0|Current Scene %x1|Visible Layers %x2|Groups %x6|Same Types %x5|Selected %x3|Active %x4|Sequence %x10",	 xco, 0, 100, 20,  &soops->outlinevis, 0, 0, 0, 0, "");
#else
			uiDefButS(block, MENU, B_REDR, "Outliner Display%t|Libraries %x7|All Scenes %x0|Current Scene %x1|Visible Layers %x2|Groups %x6|Same Types %x5|Selected %x3|Active %x4|Sequence %x10",	 xco, 0, 100, 20,  &soops->outlinevis, 0, 0, 0, 0, "");
#endif /* WITH_VERSE */
		else
#ifdef WITH_VERSE
			uiDefButS(block, MENU, B_REDR, "Outliner Display%t|Verse Servers %x9|Verse Sessions %x8|All Scenes %x0|Current Scene %x1|Visible Layers %x2|Groups %x6|Same Types %x5|Selected %x3|Active %x4|Sequence %x10",	 xco, 0, 100, 20,  &soops->outlinevis, 0, 0, 0, 0, "");
#else
			uiDefButS(block, MENU, B_REDR, "Outliner Display%t|All Scenes %x0|Current Scene %x1|Visible Layers %x2|Groups %x6|Same Types %x5|Selected %x3|Active %x4|Sequence %x10",	 xco, 0, 100, 20,  &soops->outlinevis, 0, 0, 0, 0, "");
#endif /* WITH_VERSE */
	}
	
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}


