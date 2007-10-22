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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2007, Joshua Leung (Action Editor recode) 
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_ID.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_editaction.h"
#include "BIF_interface.h"
#include "BIF_poseobject.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "BDR_drawaction.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"

#include "nla.h"

#include "blendef.h"
#include "mydevice.h"

/* enums declaring constants that are used as menu event codes  */

enum {
	ACTMENU_VIEW_CENTERVIEW= 0,
	ACTMENU_VIEW_AUTOUPDATE,
	ACTMENU_VIEW_PLAY3D,
	ACTMENU_VIEW_PLAYALL,
	ACTMENU_VIEW_ALL,
	ACTMENU_VIEW_MAXIMIZE,
	ACTMENU_VIEW_LOCK,
	ACTMENU_VIEW_SLIDERS,
	ACTMENU_VIEW_NEXTMARKER,
	ACTMENU_VIEW_PREVMARKER,
	ACTMENU_VIEW_TIME,
};

enum {
	ACTMENU_SEL_BORDER = 0,
	ACTMENU_SEL_BORDERM,
	ACTMENU_SEL_ALL_KEYS,
	ACTMENU_SEL_ALL_CHAN,
	ACTMENU_SEL_ALL_MARKERS,
	ACTMENU_SEL_INVERSE_KEYS,
	ACTMENU_SEL_INVERSE_MARKERS
};

enum {
	ACTMENU_SEL_COLUMN_KEYS	= 1,
	ACTMENU_SEL_COLUMN_MARKERSCOLUMN,
	ACTMENU_SEL_COLUMN_MARKERSBETWEEN
};

enum {
	ACTMENU_KEY_DUPLICATE = 0,
	ACTMENU_KEY_DELETE,
	ACTMENU_KEY_BAKE,
	ACTMENU_KEY_CLEAN
};

enum {
	ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_UP	= 0,
	ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_DOWN,
	ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_TOP,
	ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_BOTTOM
};

enum {
	ACTMENU_KEY_TRANSFORM_MOVE  = 0,
	ACTMENU_KEY_TRANSFORM_SCALE,
	ACTMENU_KEY_TRANSFORM_SLIDE
};

enum {
	ACTMENU_KEY_HANDLE_AUTO = 0,
	ACTMENU_KEY_HANDLE_ALIGN,
	ACTMENU_KEY_HANDLE_FREE,
	ACTMENU_KEY_HANDLE_VECTOR
};

enum {
	ACTMENU_KEY_INTERP_CONST = 0,
	ACTMENU_KEY_INTERP_LINEAR,
	ACTMENU_KEY_INTERP_BEZIER
};

enum {
	ACTMENU_KEY_EXTEND_CONST = 0,
	ACTMENU_KEY_EXTEND_EXTRAPOLATION,
	ACTMENU_KEY_EXTEND_CYCLIC,
	ACTMENU_KEY_EXTEND_CYCLICEXTRAPOLATION
};

enum {
	ACTMENU_KEY_SNAP_NEARFRAME = 1,
	ACTMENU_KEY_SNAP_CURFRAME,
	ACTMENU_KEY_SNAP_NEARMARK,
	ACTMENU_KEY_SNAP_NEARTIME,
};

enum {
	ACTMENU_KEY_MIRROR_CURFRAME = 1,
	ACTMENU_KEY_MIRROR_YAXIS,
	ACTMENU_KEY_MIRROR_XAXIS,
	ACTMENU_KEY_MIRROR_MARKER
};

enum {
	ACTMENU_MARKERS_ADD = 0,
	ACTMENU_MARKERS_DUPLICATE,
	ACTMENU_MARKERS_DELETE,
	ACTMENU_MARKERS_NAME,
	ACTMENU_MARKERS_MOVE
};

void do_action_buttons(unsigned short event)
{
	Object *ob= OBACT;

	switch(event) {
		case B_ACTHOME: /* HOMEKEY in Action Editor */
			/*	Find X extents */
			G.v2d->cur.xmin = 0;
			G.v2d->cur.ymin=-SCROLLB;
			
			if (G.saction->action) {
				float extra;
				
				calc_action_range(G.saction->action, &G.v2d->cur.xmin, &G.v2d->cur.xmax, 0);
				if (G.saction->pin==0 && ob) {
					G.v2d->cur.xmin= get_action_frame_inv(ob, G.v2d->cur.xmin);
					G.v2d->cur.xmax= get_action_frame_inv(ob, G.v2d->cur.xmax);
				}				
				extra= 0.05*(G.v2d->cur.xmax - G.v2d->cur.xmin);
				G.v2d->cur.xmin-= extra;
				G.v2d->cur.xmax+= extra;

				if (G.v2d->cur.xmin==G.v2d->cur.xmax) {
					G.v2d->cur.xmax= -5;
					G.v2d->cur.xmax= 100;
				}
			}
			else { /* shapekeys and/or no action */
				G.v2d->cur.xmin= -5.0;
				G.v2d->cur.xmax= 65.0;
			}
			
			G.v2d->cur.ymin= -75.0;
			G.v2d->cur.ymax= 5.0;
			
			G.v2d->tot= G.v2d->cur;
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			view2d_do_locks(curarea, V2D_LOCK_COPY);
			
			addqueue (curarea->win, REDRAW, 1);
			
			break;
			
		/* copy/paste/paste-flip buttons in 3d-view header in PoseMode */
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
			
		/* copy/paste buttons in Action Editor header */
		case B_ACTCOPYKEYS:
			copy_actdata();
			break;
		case B_ACTPASTEKEYS:
			paste_actdata();
			break;

		case B_ACTPIN:	/* __PINFAKE */
/*		if (G.saction->flag & SACTION_PIN) {
			if (G.saction->action)
				G.saction->action->id.us ++;
		}
		else {
			if (G.saction->action)
				G.saction->action->id.us --;
		}
*/		/* end PINFAKE */
			allqueue(REDRAWACTION, 1);
			break;
	}
}

static void do_action_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);

	switch(event) {
		case ACTMENU_VIEW_CENTERVIEW: /* Center View to Current Frame */
			center_currframe();
			break;
		case ACTMENU_VIEW_AUTOUPDATE: /* Update Automatically */
			if (BTST(G.saction->lock, 0)) 
				G.saction->lock = BCLR(G.saction->lock, 0);
			else 
				G.saction->lock = BSET(G.saction->lock, 0);
			break;
		case ACTMENU_VIEW_PLAY3D: /* Play Back Animation */
			play_anim(0);
			break;
		case ACTMENU_VIEW_PLAYALL: /* Play Back Animation in All */
			play_anim(1);
			break;	
		case ACTMENU_VIEW_ALL: /* View All */
			do_action_buttons(B_ACTHOME);
			break;
		case ACTMENU_VIEW_LOCK:
			G.v2d->flag ^= V2D_VIEWLOCK;
			if(G.v2d->flag & V2D_VIEWLOCK)
				view2d_do_locks(curarea, 0);
			break;
		case ACTMENU_VIEW_SLIDERS:	 /* Show sliders (when applicable) */
			G.saction->flag ^= SACTION_SLIDERS;
			break;
		case ACTMENU_VIEW_MAXIMIZE: /* Maximize Window */
			/* using event B_FULL */
			break;
		case ACTMENU_VIEW_NEXTMARKER: /* jump to next marker */
			nextprev_marker(1);
			break;
		case ACTMENU_VIEW_PREVMARKER: /* jump to previous marker */
			nextprev_marker(-1);
			break;
		case ACTMENU_VIEW_TIME: /* switch between frames and seconds display */
			G.saction->flag ^= SACTION_DRAWTIME;
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *action_viewmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "action_viewmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Center View to Current Frame|C", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_CENTERVIEW, "");
		
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	if (G.saction->flag & SACTION_DRAWTIME) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						"Show Frames|Ctrl T", 0, yco-=20, 
						menuwidth, 19, NULL, 0.0, 0.0, 1, 
						ACTMENU_VIEW_TIME, "");
	}
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						"Show Seconds|Ctrl T", 0, yco-=20, 
						menuwidth, 19, NULL, 0.0, 0.0, 1, 
						ACTMENU_VIEW_TIME, "");
	}
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, (G.saction->flag & SACTION_SLIDERS)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Show Sliders|", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_SLIDERS, "");
					 
	uiDefIconTextBut(block, BUTM, 1, (G.v2d->flag & V2D_VIEWLOCK)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Lock Time to Other Windows|", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_LOCK, "");
					 
	uiDefIconTextBut(block, BUTM, 1, BTST(G.saction->lock, 0)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Update Automatically|", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_AUTOUPDATE, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
					 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
				"Jump To Next Marker|PageUp", 0, yco-=20,
				menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_VIEW_NEXTMARKER, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
				"Jump To Prev Marker|PageDown", 0, yco-=20, 
				menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_VIEW_PREVMARKER, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Play Back Animation|Alt A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PLAY3D, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Play Back Animation in 3D View|Alt Shift A", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PLAYALL, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "View All|Home", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_ALL, "");
	
	if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, 
						 "Maximize Window|Ctrl UpArrow", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_VIEW_MAXIMIZE, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, 
						 "Tile Window|Ctrl DownArrow", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_VIEW_MAXIMIZE, "");
	
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

static void do_action_selectmenu_columnmenu(void *arg, int event)
{
	SpaceAction *saction;
	bAction	*act;
	Key *key;
	
	saction = curarea->spacedata.first;
	if (!saction) return;

	act = saction->action;
	key = get_action_mesh_key();
	
#if 0 // actionrewite
	if (event == ACTMENU_SEL_COLUMN_MARKERSBETWEEN) {
		markers_selectkeys_between();
	}
	else if (ELEM(event, ACTMENU_SEL_COLUMN_KEYS, ACTMENU_SEL_COLUMN_MARKERSCOLUMN)) {
		if (act)
			column_select_actionkeys(act, event);
		else if (key)
			column_select_shapekeys(key, event);
	}
	else
		return;
#endif // actionrewite
		
	allqueue(REDRAWTIME, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWSOUND, 0);
}

static uiBlock *action_selectmenu_columnmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_selectmenu_columnmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_selectmenu_columnmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "On Selected Keys|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,  
					 ACTMENU_SEL_COLUMN_KEYS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "On Selected Markers|Shift K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_COLUMN_MARKERSCOLUMN, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Between Selected Markers|Ctrl K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_COLUMN_MARKERSBETWEEN, "");
	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_selectmenu(void *arg, int event)
{	
	SpaceAction *saction;
	bAction	*act;
	Key *key;
	
	saction = curarea->spacedata.first;
	if (!saction) return;

	act = saction->action;
	key = get_action_mesh_key();
	
	switch(event)
	{
		case ACTMENU_SEL_BORDER: /* Border Select */
			borderselect_action();
			break;
			
		case ACTMENU_SEL_BORDERM: /* Border Select */
			borderselect_markers();
			break;

		case ACTMENU_SEL_ALL_KEYS: /* Select/Deselect All Keys */
			deselect_action_keys(1, 1);
			allqueue (REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue (REDRAWIPO, 0);
			break;

		case ACTMENU_SEL_ALL_CHAN: /* Select/Deselect All Channels */
			deselect_action_channels(1);
			allqueue (REDRAWVIEW3D, 0);
			allqueue (REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue (REDRAWIPO, 0);
			break;
			
		case ACTMENU_SEL_ALL_MARKERS: /* select/deselect all markers */
			deselect_markers(1, 0);
			allqueue(REDRAWMARKER, 0);
			break;
			
		case ACTMENU_SEL_INVERSE_KEYS: /* invert selection status of keys */
			deselect_action_keys(0, 2);
			allqueue (REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue (REDRAWIPO, 0);
			break;
			
		case ACTMENU_SEL_INVERSE_MARKERS: /* invert selection of markers */
			deselect_markers(0, 2);
			allqueue(REDRAWMARKER, 0);
			break;
	}
}

static uiBlock *action_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_selectmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Border Select Keys|B", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_BORDER, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Border Select Markers|Ctrl B", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_BORDERM, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Select/Deselect All Keys|A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_ALL_KEYS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Select/Deselect All Markers|Ctrl A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_ALL_MARKERS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Select/Deselect All Channels", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_ALL_CHAN, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Inverse Keys", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_INVERSE_KEYS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Inverse Markers", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_INVERSE_MARKERS, "");
		
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	uiDefIconTextBlockBut(block, action_selectmenu_columnmenu, 
						  NULL, ICON_RIGHTARROW_THIN, "Column Select Keys", 0, yco-=20, 120, 20, "");
	
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

static void do_action_keymenu_transformmenu(void *arg, int event)
{	
	switch (event)
	{
		case ACTMENU_KEY_TRANSFORM_MOVE:
			transform_action_keys('g', 0);
			break;
		case ACTMENU_KEY_TRANSFORM_SCALE:
			transform_action_keys('s', 0);
			break;
		case ACTMENU_KEY_TRANSFORM_SLIDE:
			transform_action_keys('t', 0);
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_transformmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_transformmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,  
					 ACTMENU_KEY_TRANSFORM_MOVE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Scale|S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_TRANSFORM_SCALE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Time Slide|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_TRANSFORM_SLIDE, "");
	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_handlemenu(void *arg, int event)
{
	switch (event) {
		case ACTMENU_KEY_HANDLE_AUTO:
			sethandles_action_keys(HD_AUTO);
			break;

		case ACTMENU_KEY_HANDLE_ALIGN:
		case ACTMENU_KEY_HANDLE_FREE:
			/* OK, this is kinda dumb, need to fix the
			 * toggle crap in sethandles_ipo_keys() 
			 */
			sethandles_action_keys(HD_ALIGN);
			break;

		case ACTMENU_KEY_HANDLE_VECTOR:
			sethandles_action_keys(HD_VECT);	
			break;
	}
}

static uiBlock *action_keymenu_handlemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_handlemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_handlemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Auto|Shift H", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_AUTO, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Aligned|H", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_ALIGN, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Free|H", 0, yco-=20, menuwidth, 
					 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_FREE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Vector|V", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_VECTOR, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_intpolmenu(void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_INTERP_CONST:
			action_set_ipo_flags(SET_IPO_MENU, SET_IPO_CONSTANT);
			break;
		case ACTMENU_KEY_INTERP_LINEAR:
			action_set_ipo_flags(SET_IPO_MENU, SET_IPO_LINEAR);
			break;
		case ACTMENU_KEY_INTERP_BEZIER:
			action_set_ipo_flags(SET_IPO_MENU, SET_IPO_BEZIER);
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_intpolmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_intpolmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_intpolmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Constant|Shift T, 1", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_INTERP_CONST, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Linear|Shift T, 2", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_INTERP_LINEAR, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Bezier|Shift T, 3", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_INTERP_BEZIER, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_extendmenu(void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_EXTEND_CONST:
			action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_CONSTANT);
			break;
		case ACTMENU_KEY_EXTEND_EXTRAPOLATION:
			action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_EXTRAPOLATION);
			break;
		case ACTMENU_KEY_EXTEND_CYCLIC:
			action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_CYCLIC);
			break;
		case ACTMENU_KEY_EXTEND_CYCLICEXTRAPOLATION:
			action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_CYCLICEXTRAPOLATION);
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_extendmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_extendmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_extendmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Constant", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_EXTEND_CONST, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Extrapolation", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_EXTEND_EXTRAPOLATION, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Cyclic", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_EXTEND_CYCLIC, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Cyclic Extrapolation", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_EXTEND_CYCLICEXTRAPOLATION, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_chanposmenu(void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_DOWN:
			down_sel_action();
			break;
		case ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_UP:
			up_sel_action();
			break;
		case ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_TOP:
			top_sel_action();
			break;
		case ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_BOTTOM:
			bottom_sel_action();
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_chanposmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_chanposmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_chanposmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move Up|Ctrl Page Up", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_UP, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move Down|Ctrl Page Down", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_DOWN, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
					
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move to Top|Shift Page Up", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_TOP, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move to Bottom|Shift Page Down", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_CHANPOS_MOVE_CHANNEL_BOTTOM, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_snapmenu(void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_SNAP_NEARFRAME:
		case ACTMENU_KEY_SNAP_CURFRAME:
		case ACTMENU_KEY_SNAP_NEARMARK:
		case ACTMENU_KEY_SNAP_NEARTIME:
			snap_action_keys(event);
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_snapmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_snapmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_snapmenu, NULL);

	if (G.saction->flag & SACTION_DRAWTIME) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Nearest Second|Shift S, 1", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_NEARTIME, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Current Time|Shift S, 2", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_CURFRAME, "");

	}
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Nearest Frame|Shift S, 1", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_NEARFRAME, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Current Frame|Shift S, 2", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_CURFRAME, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Nearest Marker|Shift S, 3", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_SNAP_NEARMARK, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_mirrormenu(void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_MIRROR_CURFRAME:
		case ACTMENU_KEY_MIRROR_YAXIS:
			mirror_action_keys(event);
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_mirrormenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_mirrormenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_mirrormenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Current Frame|Shift M, 1", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_MIRROR_CURFRAME, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Vertical Axis|Shift M, 2", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_MIRROR_YAXIS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Horizontal Axis|Shift M, 3", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_MIRROR_XAXIS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Selected Marker|Shift M, 4", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_MIRROR_MARKER, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu(void *arg, int event)
{	
	SpaceAction *saction;
	bAction	*act;
	Key *key;
	
	saction = curarea->spacedata.first;
	if (!saction) return;

	act = saction->action;
	key = get_action_mesh_key();

	switch(event)
	{
		case ACTMENU_KEY_DUPLICATE:
			duplicate_action_keys();
 			break;
		case ACTMENU_KEY_DELETE:
			delete_action_keys ();
			break;
		case ACTMENU_KEY_CLEAN:
			clean_action();
			break;
	}
}

static uiBlock *action_keymenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_keymenu, NULL);
	
	uiDefIconTextBlockBut(block, action_keymenu_transformmenu, 
						  NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 20, "");
	
	uiDefIconTextBlockBut(block, action_keymenu_snapmenu, 
						  NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 20, "");
	
	uiDefIconTextBlockBut(block, action_keymenu_mirrormenu, 
						  NULL, ICON_RIGHTARROW_THIN, "Mirror", 0, yco-=20, 120, 20, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					"Duplicate|Shift D", 0, yco-=20, 
					menuwidth, 19, NULL, 0.0, 0.0, 0, 
					ACTMENU_KEY_DUPLICATE, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					"Delete|X", 0, yco-=20, 
					menuwidth, 19, NULL, 0.0, 0.0, 0, 
					ACTMENU_KEY_DELETE, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Clean Action|O", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_CLEAN, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBlockBut(block, action_keymenu_handlemenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Handle Type", 0, yco-=20, 120, 20, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBlockBut(block, action_keymenu_extendmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Extend Mode", 0, yco-=20, 120, 20, "");
	uiDefIconTextBlockBut(block, action_keymenu_intpolmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Interpolation Mode", 0, yco-=20, 120, 20, "");
	uiDefIconTextBlockBut(block, action_keymenu_chanposmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Channel Ordering", 0, yco-=20, 120, 20, "");

	
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

static void do_action_markermenu(void *arg, int event)
{	
	switch(event)
	{
		case ACTMENU_MARKERS_ADD:
			add_marker(CFRA);
			break;
		case ACTMENU_MARKERS_DUPLICATE:
			duplicate_marker();
			break;
		case ACTMENU_MARKERS_DELETE:
			remove_marker();
			break;
		case ACTMENU_MARKERS_NAME:
			rename_marker();
			break;
		case ACTMENU_MARKERS_MOVE:
			transform_markers('g', 0);
			break;
	}
	
	allqueue(REDRAWMARKER, 0);
}

static uiBlock *action_markermenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_markermenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_markermenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|M", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_ADD, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Ctrl Shift D", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_DUPLICATE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker|X", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_DELETE, "");
					 
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "(Re)Name Marker|Ctrl M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_NAME, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Marker|Ctrl G", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_MOVE, "");
	
	
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

void action_buttons(void)
{
	uiBlock *block;
	short xco, xmax;
	char name[256];
	Object *ob;
	ID *from;

	if (G.saction == NULL)
		return;

	/* copied from drawactionspace.... */
	if (!G.saction->pin) {
		if (OBACT)
			G.saction->action = OBACT->action;
		else
			G.saction->action= NULL;
	}

	sprintf(name, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, name, 
					  UI_EMBOSS, UI_HELV, curarea->headwin);

	if (area_is_active_area(curarea)) 
		uiBlockSetCol(block, TH_HEADER);
	else 
		uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_ACTION;
	
	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, 
					  windowtype_pup(), xco, 0, XIC+10, YIC, 
					  &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
					  "Displays Current Window Type. "
					  "Click for menu of available types.");

	xco += XIC + 14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if (curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  xco,2,XIC,YIC-2,
					  &(curarea->flag), 0, 0, 0, 0, 
					  "Show pulldown menus");
	}
	else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  xco,2,XIC,YIC-2,
					  &(curarea->flag), 0, 0, 0, 0, 
					  "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if ((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, action_viewmenu, NULL, 
					  "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefPulldownBut(block, action_selectmenu, NULL, 
					  "Select", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Marker");
		uiDefPulldownBut(block, action_markermenu, NULL, 
					  "Marker", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Key");
		uiDefPulldownBut(block, action_keymenu, NULL, 
					  "Key", xco, -2, xmax-3, 24, "");
		xco+= xmax;
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* NAME ETC */
	ob= OBACT;
	from = (ID *)ob;

	xco= std_libbuttons(block, xco, 0, B_ACTPIN, &G.saction->pin, 
						B_ACTIONBROWSE, ID_AC, 0, (ID*)G.saction->action, 
						from, &(G.saction->actnr), B_ACTALONE, 
						B_ACTLOCAL, B_ACTIONDELETE, 0, 0);	

	uiClearButLock();

	/* draw AUTOSNAP */
	xco += 8;
	
	if (G.saction->flag & SACTION_DRAWTIME) {
		uiDefButS(block, MENU, B_REDR,
				"Auto-Snap Keyframes %t|Off %x0|Second Step %x1|Nearest Second %x2|Nearest Marker %x3", 
				xco,0,70,YIC, &(G.saction->autosnap), 0, 1, 0, 0, 
				"Auto-snapping mode for keyframes when transforming");
	}
	else {
		uiDefButS(block, MENU, B_REDR, 
				"Auto-Snap Keyframes %t|Off %x0|Frame Step %x1|Nearest Frame %x2|Nearest Marker %x3", 
				xco,0,70,YIC, &(G.saction->autosnap), 0, 1, 0, 0, 
				"Auto-snapping mode for keyframes when transforming");
	}
	
	xco += (70 + 8);
	
	/* COPY PASTE */
	uiBlockBeginAlign(block);
	if (curarea->headertype==HEADERTOP) {
		uiDefIconBut(block, BUT, B_ACTCOPYKEYS, ICON_COPYUP,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected keyframes from the selected channel(s) to the buffer");
		uiDefIconBut(block, BUT, B_ACTPASTEKEYS, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the keyframes from the buffer");
	}
	else {
		uiDefIconBut(block, BUT, B_ACTCOPYKEYS, ICON_COPYDOWN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected keyframes from the selected channel(s) to the buffer");
		uiDefIconBut(block, BUT, B_ACTPASTEKEYS, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the keyframes from the buffer");
	}
	uiBlockEndAlign(block);
	xco += (XIC + 8);
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco, 0, XIC, YIC, 
				  &(G.saction->lock), 0, 0, 0, 0, 
				  "Updates other affected window spaces automatically "
				  "to reflect changes in real time");
	
	/* always as last  */
	curarea->headbutlen = xco + 2*XIC;

	uiDrawBlock(block);
}
