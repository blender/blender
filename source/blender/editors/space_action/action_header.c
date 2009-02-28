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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "action_intern.h"

/* ********************************************************* */
/* Menu Defines... */

/* button events */
enum {
	B_REDR 	= 0,
	B_ACTCOPYKEYS,
	B_ACTPASTEKEYS,
} eActHeader_ButEvents;

/* ------------------------------- */
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
	ACTMENU_VIEW_NEXTKEYFRAME,
	ACTMENU_VIEW_PREVKEYFRAME,
	ACTMENU_VIEW_TIME,
	ACTMENU_VIEW_NOHIDE,
	ACTMENU_VIEW_FRANUM,
	ACTMENU_VIEW_TRANSDELDUPS,
	ACTMENU_VIEW_HORIZOPTIMISE,
	ACTMENU_VIEW_GCOLORS,
	ACTMENU_VIEW_PREVRANGESET,
	ACTMENU_VIEW_PREVRANGECLEAR,
	ACTMENU_VIEW_PREVRANGEAUTO
};

enum {
	ACTMENU_SEL_BORDER = 0,
	ACTMENU_SEL_BORDERC,
	ACTMENU_SEL_BORDERM,
	ACTMENU_SEL_ALL_KEYS,
	ACTMENU_SEL_ALL_CHAN,
	ACTMENU_SEL_ALL_MARKERS,
	ACTMENU_SEL_INVERSE_KEYS,
	ACTMENU_SEL_INVERSE_MARKERS,
	ACTMENU_SEL_INVERSE_CHANNELS,
	ACTMENU_SEL_LEFTKEYS,
	ACTMENU_SEL_RIGHTKEYS
};

enum {
	ACTMENU_SEL_COLUMN_KEYS	= 1,
	ACTMENU_SEL_COLUMN_CFRA,
	ACTMENU_SEL_COLUMN_MARKERSCOLUMN,
	ACTMENU_SEL_COLUMN_MARKERSBETWEEN 
};

enum {
	ACTMENU_CHANNELS_OPENLEVELS = 0,
	ACTMENU_CHANNELS_CLOSELEVELS,
	ACTMENU_CHANNELS_EXPANDALL,
	ACTMENU_CHANNELS_SHOWACHANS,
	ACTMENU_CHANNELS_DELETE
};

enum {
	ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_UP	= 0,
	ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_DOWN,
	ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_TOP,
	ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_BOTTOM
};

enum {
	ACTMENU_CHANNELS_GROUP_ADD_TOACTIVE	= 0,
	ACTMENU_CHANNELS_GROUP_ADD_TONEW,
	ACTMENU_CHANNELS_GROUP_REMOVE,
	ACTMENU_CHANNELS_GROUP_SYNCPOSE
};

enum {
	ACTMENU_CHANNELS_SETTINGS_TOGGLE = 0,
	ACTMENU_CHANNELS_SETTINGS_ENABLE,
	ACTMENU_CHANNELS_SETTINGS_DISABLE,
};

enum {
	ACTMENU_KEY_DUPLICATE = 0,
	ACTMENU_KEY_DELETE,
	ACTMENU_KEY_CLEAN,
	ACTMENU_KEY_SAMPLEKEYS,
	ACTMENU_KEY_INSERTKEY
};

enum {
	ACTMENU_KEY_TRANSFORM_MOVE  = 0,
	ACTMENU_KEY_TRANSFORM_SCALE,
	ACTMENU_KEY_TRANSFORM_SLIDE,
	ACTMENU_KEY_TRANSFORM_EXTEND
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
	ACTMENU_KEY_SNAP_CFRA2KEY,
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
	ACTMENU_MARKERS_MOVE,
	ACTMENU_MARKERS_LOCALADD,
	ACTMENU_MARKERS_LOCALRENAME,
	ACTMENU_MARKERS_LOCALDELETE,
	ACTMENU_MARKERS_LOCALMOVE
};

/* ------------------------------- */
/* macros for easier state testing (only for use here) */

/* test if active action editor is showing any markers */
#if 0
	#define SACTION_HASMARKERS \
		((saction->action && saction->action->markers.first) \
		 || (scene->markers.first))
#endif

/* need to find out how to get scene from context */
#define SACTION_HASMARKERS (saction->action && saction->action->markers.first)

/* ------------------------------- */

/* *************************************************************** */
/* menus */

/* Key menu ---------------------------  */

static void do_keymenu_transformmenu(bContext *C, void *arg, int event)
{
	switch (event)
	{
		case ACTMENU_KEY_TRANSFORM_MOVE:
			//transform_action_keys('g', 0);
			break;
		case ACTMENU_KEY_TRANSFORM_SCALE:
			//transform_action_keys('s', 0);
			break;
		case ACTMENU_KEY_TRANSFORM_SLIDE:
			//transform_action_keys('t', 0);
			break;
		case ACTMENU_KEY_TRANSFORM_EXTEND:
			//transform_action_keys('e', 0);
			break;
	}
}

static uiBlock *action_keymenu_transformmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_keymenu_transformmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu_transformmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,  
					 ACTMENU_KEY_TRANSFORM_MOVE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Grab/Extend from Frame|E", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_TRANSFORM_EXTEND, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Scale|S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_TRANSFORM_SCALE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Time Slide|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_TRANSFORM_SLIDE, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}

static void do_keymenu_snapmenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_SNAP_NEARFRAME:
		case ACTMENU_KEY_SNAP_CURFRAME:
		case ACTMENU_KEY_SNAP_NEARMARK:
		case ACTMENU_KEY_SNAP_NEARTIME:
			//snap_action_keys(event);
			break;
			
		case ACTMENU_KEY_SNAP_CFRA2KEY:
			//snap_cfra_action();
			break;
	}
}

static uiBlock *action_keymenu_snapmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;

	
	block= uiBeginBlock(C, ar, "action_keymenu_snapmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu_snapmenu, NULL);

	if (saction->flag & SACTION_DRAWTIME) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Key -> Nearest Second|Shift S, 1", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_NEARTIME, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Key -> Current Time|Shift S, 2", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_CURFRAME, "");

	}
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Key -> Nearest Frame|Shift S, 1", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_NEARFRAME, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Key -> Current Frame|Shift S, 2", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_KEY_SNAP_CURFRAME, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Key -> Nearest Marker|Shift S, 3", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_SNAP_NEARMARK, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Current Frame -> Key|Ctrl Shift S", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_SNAP_NEARMARK, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	
	uiTextBoundsBlock(block, 60);
	uiEndBlock(C, block);
	
	return block;
}

static void do_keymenu_mirrormenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_MIRROR_CURFRAME:
		case ACTMENU_KEY_MIRROR_YAXIS:
			//mirror_action_keys(event);
			break;
	}

}

static uiBlock *action_keymenu_mirrormenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_keymenu_mirrormenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu_mirrormenu, NULL);

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
	uiEndBlock(C, block);
	
	return block;
}

static void do_keymenu_handlemenu(bContext *C, void *arg, int event)
{
	switch (event) {
		case ACTMENU_KEY_HANDLE_AUTO:
			//sethandles_action_keys(HD_AUTO);
			break;

		case ACTMENU_KEY_HANDLE_ALIGN:
		case ACTMENU_KEY_HANDLE_FREE:
			/* OK, this is kinda dumb, need to fix the
			 * toggle crap in sethandles_ipo_keys() 
			 */
			//sethandles_action_keys(HD_ALIGN);
			break;

		case ACTMENU_KEY_HANDLE_VECTOR:
			//sethandles_action_keys(HD_VECT);	
			break;
	}
}

static uiBlock *action_keymenu_handlemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_keymenu_handlemenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu_handlemenu, NULL);

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
	uiEndBlock(C, block);
	
	return block;
}

static void do_keymenu_extendmenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_EXTEND_CONST:
			//action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_CONSTANT);
			break;
		case ACTMENU_KEY_EXTEND_EXTRAPOLATION:
			//action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_EXTRAPOLATION);
			break;
		case ACTMENU_KEY_EXTEND_CYCLIC:
			//action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_CYCLIC);
			break;
		case ACTMENU_KEY_EXTEND_CYCLICEXTRAPOLATION:
			//action_set_ipo_flags(SET_EXTEND_MENU, SET_EXTEND_CYCLICEXTRAPOLATION);
			break;
	}
}

static uiBlock *action_keymenu_extendmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_keymenu_extendmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu_extendmenu, NULL);
	
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
	uiEndBlock(C, block);
	
	return block;
}

static void do_keymenu_intpolmenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_KEY_INTERP_CONST:
			//action_set_ipo_flags(SET_IPO_MENU, SET_IPO_CONSTANT);
			break;
		case ACTMENU_KEY_INTERP_LINEAR:
			//action_set_ipo_flags(SET_IPO_MENU, SET_IPO_LINEAR);
			break;
		case ACTMENU_KEY_INTERP_BEZIER:
			//action_set_ipo_flags(SET_IPO_MENU, SET_IPO_BEZIER);
			break;
	}
}

static uiBlock *action_keymenu_intpolmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_keymenu_intpolmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu_intpolmenu, NULL);
	
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
	uiEndBlock(C, block);
	
	return block;
}

static void do_action_keymenu(bContext *C, void *arg, int event)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	bAction	*act;
	//Key *key;
	
	if (!saction) return;

	act = saction->action;
	//key = get_action_mesh_key();

	switch(event)
	{
		case ACTMENU_KEY_DUPLICATE:
			//duplicate_action_keys();
 			break;
		case ACTMENU_KEY_DELETE:
			//delete_action_keys();
			break;
		case ACTMENU_KEY_CLEAN:
			//clean_action();
			break;
		case ACTMENU_KEY_SAMPLEKEYS:
			//sample_action_keys();
			break;
		case ACTMENU_KEY_INSERTKEY:
			//insertkey_action();
			break;
	}
}

static uiBlock *action_keymenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_keymenu", UI_EMBOSSP, UI_HELV);


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
					"Insert Key|I", 0, yco-=20, 
					menuwidth, 19, NULL, 0.0, 0.0, 0, 
					ACTMENU_KEY_INSERTKEY, "");

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
					 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Sample Keys|Alt O", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_SAMPLEKEYS, "");

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
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

/* Frame menu ---------------------------  */


// framemenu uses functions from keymenu
static uiBlock *action_framemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_framemenu", UI_EMBOSSP, UI_HELV);
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
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

/* Marker menu ---------------------------  */

static void do_markermenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_MARKERS_ADD:
			//add_marker(CFRA);
			break;
		case ACTMENU_MARKERS_DUPLICATE:
			//duplicate_marker();
			break;
		case ACTMENU_MARKERS_DELETE:
			//remove_marker();
			break;
		case ACTMENU_MARKERS_NAME:
			//rename_marker();
			break;
		case ACTMENU_MARKERS_MOVE:
			//transform_markers('g', 0);
			break;
		case ACTMENU_MARKERS_LOCALADD:
			//action_add_localmarker(G.saction->action, CFRA);
			break;
		case ACTMENU_MARKERS_LOCALDELETE:
			//action_remove_localmarkers(G.saction->action);
			break;
		case ACTMENU_MARKERS_LOCALRENAME:
			//action_rename_localmarker(G.saction->action);
			break;
		case ACTMENU_MARKERS_LOCALMOVE:
			/*G.saction->flag |= SACTION_POSEMARKERS_MOVE;
			transform_markers('g', 0);
			G.saction->flag &= ~SACTION_POSEMARKERS_MOVE;*/
			break;
	}
}

static uiBlock *action_markermenu(bContext *C, ARegion *ar, void *arg_unused)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_markermenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_markermenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|M", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_ADD, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Ctrl Shift D", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_DUPLICATE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker|Shift X", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_DELETE, "");
					
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
					
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "(Re)Name Marker|Ctrl M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_NAME, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Marker|Ctrl G", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_MOVE, "");
		
	if (saction->mode == SACTCONT_ACTION) {
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Pose Marker|Shift L", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_LOCALADD, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rename Pose Marker|Ctrl Shift L", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_LOCALRENAME, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Pose Marker|Alt L", 0, yco-=20,
						 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_LOCALDELETE, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Pose Marker|Ctrl L", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, ACTMENU_MARKERS_LOCALMOVE, "");
	}
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}


/* Channel menu ---------------------------  */

static void do_channelmenu_posmenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_DOWN:
			//rearrange_action_channels(REARRANGE_ACTCHAN_DOWN);
			break;
		case ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_UP:
			//rearrange_action_channels(REARRANGE_ACTCHAN_UP);
			break;
		case ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_TOP:
			//rearrange_action_channels(REARRANGE_ACTCHAN_TOP);
			break;
		case ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_BOTTOM:
			//rearrange_action_channels(REARRANGE_ACTCHAN_BOTTOM);
			break;
	}
}

static uiBlock *action_channelmenu_posmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, ar, "action_channelmenu_posmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_channelmenu_posmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move Up|Shift Page Up", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_UP, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move Down|Shift Page Down", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_DOWN, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
					
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move to Top|Ctrl Shift Page Up", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_TOP, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Move to Bottom|Ctrl Shift Page Down", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_CHANPOS_MOVE_CHANNEL_BOTTOM, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_channelmenu_groupmenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
		case ACTMENU_CHANNELS_GROUP_ADD_TOACTIVE:
			//action_groups_group(0);
			break;
		case ACTMENU_CHANNELS_GROUP_ADD_TONEW:
			//action_groups_group(1);
			break;
		case ACTMENU_CHANNELS_GROUP_REMOVE:
			//action_groups_ungroup();
			break;
		case ACTMENU_CHANNELS_GROUP_SYNCPOSE: /* Syncronise Pose-data and Action-data */
			//sync_pchan2achan_grouping();
			break;
	}
}

static uiBlock *action_channelmenu_groupmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, ar, "action_channelmenu_groupmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_channelmenu_groupmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Add to Active Group|Shift G", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_GROUP_ADD_TOACTIVE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Add to New Group|Ctrl Shift G", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_GROUP_ADD_TONEW, "");
		
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
					
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Remove From Group|Alt G", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_GROUP_REMOVE, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
					
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Synchronise with Armature", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_GROUP_SYNCPOSE, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_channelmenu_settingsmenu(bContext *C, void *arg, int event)
{
	//setflag_action_channels(event);
}

static uiBlock *action_channelmenu_settingsmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, ar, "action_channelmenu_settingsmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_channelmenu_settingsmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Toggle a Setting|Shift W", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_SETTINGS_TOGGLE, "");
					 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Enable a Setting|Ctrl Shift W", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_SETTINGS_ENABLE, "");
					
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Disable a Setting|Alt W", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_CHANNELS_SETTINGS_DISABLE, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_channelmenu(bContext *C, void *arg, int event)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	
	if (saction == NULL) return;
	
	switch(event)
	{
		case ACTMENU_CHANNELS_OPENLEVELS: /* Unfold selected channels one step */
			//openclose_level_action(1);
			break;
		case ACTMENU_CHANNELS_CLOSELEVELS: /* Fold selected channels one step */
			//openclose_level_action(-1);
			break;
		case ACTMENU_CHANNELS_EXPANDALL: /* Expands all channels */
			//expand_all_action();
			break;
		case ACTMENU_CHANNELS_SHOWACHANS: /* Unfold groups that are hiding selected achans */
			//expand_obscuregroups_action();
			break;
		case ACTMENU_CHANNELS_DELETE: /* Deletes selected channels */
			//delete_action_channels();
			break;
	}
}

static uiBlock *action_channelmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_channelmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_channelmenu, NULL);
		
	uiDefIconTextBlockBut(block, action_channelmenu_groupmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Grouping", 0, yco-=20, 120, 20, "");	 
						  
	uiDefIconTextBlockBut(block, action_channelmenu_posmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Ordering", 0, yco-=20, 120, 20, "");
	
	uiDefIconTextBlockBut(block, action_channelmenu_settingsmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Settings", 0, yco-=20, 120, 20, "");	
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
			"Delete|X", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_CHANNELS_DELETE, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
			"Toggle Show Hierachy|~", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_CHANNELS_EXPANDALL, "");
			
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
			"Show Group-Hidden Channels|Shift ~", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_CHANNELS_SHOWACHANS, "");
			
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
			"Expand One Level|Ctrl NumPad+", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_CHANNELS_OPENLEVELS, "");
			
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
			"Collapse One Level|Ctrl NumPad-", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_CHANNELS_CLOSELEVELS, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

/* Grease Pencil ---------------------------  */

/* Uses channelmenu functions */
static uiBlock *action_gplayermenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_gplayermenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_channelmenu, NULL);
		
	uiDefIconTextBlockBut(block, action_channelmenu_settingsmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Settings", 0, yco-=20, 120, 20, "");	
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
			"Delete|X", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_CHANNELS_DELETE, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

/* Select menu ---------------------------  */

static void do_selectmenu_columnmenu(bContext *C, void *arg, int event)
{
	switch (event) {
		case ACTMENU_SEL_COLUMN_MARKERSBETWEEN:
			//markers_selectkeys_between();
			break;
		case ACTMENU_SEL_COLUMN_KEYS:
			//column_select_action_keys(1);
			break;
		case ACTMENU_SEL_COLUMN_MARKERSCOLUMN:
			//column_select_action_keys(2);
			break;
		case ACTMENU_SEL_COLUMN_CFRA:
			//column_select_action_keys(3);
			break;
	}
}

static uiBlock *action_selectmenu_columnmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, ar, "action_selectmenu_columnmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_selectmenu_columnmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "On Selected Keys|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,  
					 ACTMENU_SEL_COLUMN_KEYS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "On Current Frame|Ctrl K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,  
					 ACTMENU_SEL_COLUMN_CFRA, "");
	
	if (SACTION_HASMARKERS) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "On Selected Markers|Shift K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_COLUMN_MARKERSCOLUMN, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Between Selected Markers|Alt K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_COLUMN_MARKERSBETWEEN, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_selectmenu(bContext *C, void *arg, int event)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	//Key *key;
	
	if (saction == NULL) return;

	//key = get_action_mesh_key();
	
	switch(event)
	{
		case ACTMENU_SEL_BORDER: /* Border Select */
			//borderselect_action();
			break;
			
		case ACTMENU_SEL_BORDERC: /* Border Select */
			//borderselect_actionchannels();
			break;
			
		case ACTMENU_SEL_BORDERM: /* Border Select */
			//borderselect_markers();
			break;
			
		case ACTMENU_SEL_ALL_KEYS: /* Select/Deselect All Keys */
			/*deselect_action_keys(1, 1);
			BIF_undo_push("(De)Select Keys");
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);*/
			break;
			
		case ACTMENU_SEL_ALL_CHAN: /* Select/Deselect All Channels */
			/*deselect_action_channels(1);
			BIF_undo_push("(De)Select Action Channels");
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);*/
			break;
			
		case ACTMENU_SEL_ALL_MARKERS: /* select/deselect all markers */
			/*deselect_markers(1, 0);
			BIF_undo_push("(De)Select Markers");
			allqueue(REDRAWMARKER, 0);*/
			break;
			
		case ACTMENU_SEL_INVERSE_KEYS: /* invert selection status of keys */
			/*deselect_action_keys(0, 2);
			BIF_undo_push("Inverse Keys");
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);*/
			break;
			
		case ACTMENU_SEL_INVERSE_CHANNELS: /* invert selection status of channels */
			/*deselect_action_channels(2);
			BIF_undo_push("Inverse Action Channels");
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);*/
			break;
			
		case ACTMENU_SEL_INVERSE_MARKERS: /* invert selection of markers */
			/*deselect_markers(0, 2);
			BIF_undo_push("Inverse Action Channels");
			allqueue(REDRAWMARKER, 0);*/
			break;
			
		case ACTMENU_SEL_LEFTKEYS:
			//selectkeys_leftright(1, SELECT_REPLACE);
			break;
			
		case ACTMENU_SEL_RIGHTKEYS:
			//selectkeys_leftright(0, SELECT_REPLACE);
			break;
	}
}

static uiBlock *action_selectmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "action_selectmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Border Select Keys|B", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_BORDER, "");
	if (SACTION_HASMARKERS) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Border Select Markers|Ctrl B", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_BORDERM, "");
	}
	if (saction->mode != SACTCONT_SHAPEKEY) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Border Select Channels|B", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_BORDERC, "");
	}
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Select/Deselect All Keys|A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_ALL_KEYS, "");
	if (SACTION_HASMARKERS) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Select/Deselect All Markers|Ctrl A", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_ALL_MARKERS, "");
	}
	if (saction->mode != SACTCONT_SHAPEKEY) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Select/Deselect All Channels|A", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_ALL_CHAN, "");
	}
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Inverse Keys|Ctrl I", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_INVERSE_KEYS, "");
	if (SACTION_HASMARKERS) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Inverse Markers|Ctrl Shift I", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_INVERSE_MARKERS, "");
	}
	if (saction->mode != SACTCONT_SHAPEKEY) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
						 "Inverse All Channels|Ctrl I", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_SEL_INVERSE_CHANNELS, "");
	}
		
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Back In Time|Alt RMB", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_LEFTKEYS, "");
								 
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Ahead In Time|Alt RMB", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_RIGHTKEYS, "");		 
			 
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
	uiEndBlock(C, block);
	
	return block;
}

/* View menu ---------------------------  */

static void do_viewmenu(bContext *C, void *arg, int event)
{
	switch(event) {
		case ACTMENU_VIEW_CENTERVIEW: /* Center View to Current Frame */
			//center_currframe();
			break;
		case ACTMENU_VIEW_AUTOUPDATE: /* Update Automatically */
			/*if (BTST(G.saction->lock, 0)) 
				G.saction->lock = BCLR(G.saction->lock, 0);
			else 
				G.saction->lock = BSET(G.saction->lock, 0);*/
			break;
		case ACTMENU_VIEW_PLAY3D: /* Play Back Animation */
			//play_anim(0);
			break;
		case ACTMENU_VIEW_PLAYALL: /* Play Back Animation in All */
			//play_anim(1);
			break;	
		case ACTMENU_VIEW_ALL: /* View All */
			//do_action_buttons(B_ACTHOME);
			break;
		case ACTMENU_VIEW_LOCK:
			/*G.v2d->flag ^= V2D_VIEWLOCK;
			if (G.v2d->flag & V2D_VIEWLOCK)
				view2d_do_locks(curarea, 0);*/
			break;
		case ACTMENU_VIEW_SLIDERS:	 /* Show sliders (when applicable) */
			//G.saction->flag ^= SACTION_SLIDERS;
			break;
		case ACTMENU_VIEW_MAXIMIZE: /* Maximize Window */
			/* using event B_FULL */
			break;
		case ACTMENU_VIEW_NEXTMARKER: /* Jump to next marker */
			//nextprev_marker(1);
			break;
		case ACTMENU_VIEW_PREVMARKER: /* Jump to previous marker */
			//nextprev_marker(-1);
			break;
		case ACTMENU_VIEW_TIME: /* switch between frames and seconds display */
			//G.saction->flag ^= SACTION_DRAWTIME;
			break;
		case ACTMENU_VIEW_NOHIDE: /* Show hidden channels */
			//G.saction->flag ^= SACTION_NOHIDE;
			break;
		case ACTMENU_VIEW_NEXTKEYFRAME: /* Jump to next keyframe */
			//nextprev_action_keyframe(1);
			break;
		case ACTMENU_VIEW_PREVKEYFRAME: /* Jump to previous keyframe */
			//nextprev_action_keyframe(-1);
			break;
		case ACTMENU_VIEW_TRANSDELDUPS: /* Don't delete duplicate/overlapping keyframes after transform */
			//G.saction->flag ^= SACTION_NOTRANSKEYCULL;
			break;
		case ACTMENU_VIEW_HORIZOPTIMISE: /* Include keyframes not in view (horizontally) when preparing to draw */
			//G.saction->flag ^= SACTION_HORIZOPTIMISEON;
			break;
		case ACTMENU_VIEW_GCOLORS: /* Draw grouped-action channels using its group's color */
			//G.saction->flag ^= SACTION_NODRAWGCOLORS;
			break;
		case ACTMENU_VIEW_PREVRANGESET: /* Set preview range */
			//anim_previewrange_set();
			break;
		case ACTMENU_VIEW_PREVRANGECLEAR: /* Clear preview range */
			//anim_previewrange_clear();
			break;
		case ACTMENU_VIEW_PREVRANGEAUTO: /* Auto preview-range length */
			//action_previewrange_set(G.saction->action);
			break;
	}
}

static uiBlock *action_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	View2D *v2d= UI_view2d_fromcontext_rwin(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Center View to Current Frame|C", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_CENTERVIEW, "");
		
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			 
	if (saction->flag & SACTION_DRAWTIME) {
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
	
	if (saction->mode == SACTCONT_GPENCIL) {
			// this option may get removed in future
		uiDefIconTextBut(block, BUTM, 1, (saction->flag & SACTION_HORIZOPTIMISEON)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
						 "Cull Out-of-View Keys (Time)|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_HORIZOPTIMISE, "");
	}
	else {
		uiDefIconTextBut(block, BUTM, 1, (saction->flag & SACTION_SLIDERS)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
						 "Show Sliders|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_SLIDERS, "");
						 
		uiDefIconTextBut(block, BUTM, 1, (saction->flag & SACTION_NOHIDE)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
						 "Show Hidden Channels|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_NOHIDE, "");
						 
		uiDefIconTextBut(block, BUTM, 1, (saction->flag & SACTION_NODRAWGCOLORS)?ICON_CHECKBOX_DEHLT:ICON_CHECKBOX_HLT, 
						 "Use Group Colors|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_GCOLORS, "");
						 
			// this option may get removed in future
		uiDefIconTextBut(block, BUTM, 1, (saction->flag & SACTION_HORIZOPTIMISEON)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
						 "Cull Out-of-View Keys (Time)|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_HORIZOPTIMISE, "");
		
		uiDefIconTextBut(block, BUTM, 1, (saction->flag & SACTION_NOTRANSKEYCULL)?ICON_CHECKBOX_DEHLT:ICON_CHECKBOX_HLT, 
						 "AutoMerge Keyframes|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_TRANSDELDUPS, "");
	}	
	
		
	uiDefIconTextBut(block, BUTM, 1, (v2d->flag & V2D_VIEWSYNC_SCREEN_TIME)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Lock Time to Other Windows|", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_LOCK, "");
					 
	/*uiDefIconTextBut(block, BUTM, 1, BTST(saction->lock, 0)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Update Automatically|", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_AUTOUPDATE, "");*/

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
				"Jump To Next Marker|PageUp", 0, yco-=20,
				menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_VIEW_NEXTMARKER, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
				"Jump To Prev Marker|PageDown", 0, yco-=20, 
				menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_VIEW_PREVMARKER, "");
				
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
				"Jump To Next Keyframe|Ctrl PageUp", 0, yco-=20,
				menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_VIEW_NEXTKEYFRAME, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
				"Jump To Prev Keyframe|Ctrl PageDown", 0, yco-=20, 
				menuwidth, 19, NULL, 0.0, 0.0, 0, ACTMENU_VIEW_PREVKEYFRAME, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
					menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Play Back Animation|Alt A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PLAY3D, "");
	//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
	//				 "Play Back Animation in 3D View|Alt Shift A", 0, yco-=20,
	//				 menuwidth, 19, NULL, 0.0, 0.0, 1, 
	//				 ACTMENU_VIEW_PLAYALL, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Set Preview Range|Ctrl P", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PREVRANGESET, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Clear Preview Range|Alt P", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PREVRANGECLEAR, "");
		
	if ((saction->mode == SACTCONT_ACTION) && (saction->action)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Preview Range from Action Length|Ctrl Alt P", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PREVRANGEAUTO, "");
	}
		
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "View All|Home", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_ALL, "");
	
/*	if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, 
						 "Maximize Window|Ctrl UpArrow", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_VIEW_MAXIMIZE, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, 
						 "Tile Window|Ctrl DownArrow", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_VIEW_MAXIMIZE, "");
*/
	
	if (curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

/* ************************ header area region *********************** */

static void do_action_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
			
		case B_ACTCOPYKEYS:
			WM_operator_name_call(C, "ACT_OT_keyframes_copy", WM_OP_EXEC_REGION_WIN, NULL);
			break;
		case B_ACTPASTEKEYS:
			WM_operator_name_call(C, "ACT_OT_keyframes_paste", WM_OP_EXEC_REGION_WIN, NULL);
			break;
	}
}

static void saction_idpoin_handle(bContext *C, ID *id, int event)
{
	SpaceAction *saction= (SpaceAction*)CTX_wm_space_data(C);
	Object *obact= CTX_data_active_object(C);
	// AnimData *adt= BKE_id_add_animdata((ID *)obact);

	switch (event) {
		case UI_ID_BROWSE:
		case UI_ID_DELETE:
			saction->action= (bAction*)id;
			/* we must set this action to be the one used by active object (if not pinned) */
			if (saction->pin == 0)
				obact->adt->action= saction->action;
			
			ED_area_tag_redraw(CTX_wm_area(C));
			ED_undo_push(C, "Assign Action");
			break;
		case UI_ID_RENAME:
			break;
		case UI_ID_ADD_NEW:
			/* XXX not implemented */
			break;
		case UI_ID_OPEN:
			/* XXX not implemented */
			break;
		case UI_ID_ALONE:
			/* XXX not implemented */
			break;
		case UI_ID_PIN:
			break;
	}
}

void action_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceAction *saction= (SpaceAction *)CTX_wm_space_data(C);
	bAnimContext ac;
	uiBlock *block;
	int xco, yco= 3, xmax;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_action_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* get context... (also syncs data) */
	ANIM_animdata_get_context(C, &ac);
	
	if ((sa->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, action_viewmenu, CTX_wm_area(C), 
					  "View", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefPulldownBut(block, action_selectmenu, CTX_wm_area(C), 
					  "Select", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		if ( (saction->mode == SACTCONT_DOPESHEET) ||
			 ((saction->action) && (saction->mode==SACTCONT_ACTION)) ) 
		{
			xmax= GetButStringLength("Channel");
			uiDefPulldownBut(block, action_channelmenu, CTX_wm_area(C), 
						  "Channel", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
		else if (saction->mode==SACTCONT_GPENCIL) {
			xmax= GetButStringLength("Channel");
			uiDefPulldownBut(block, action_gplayermenu, CTX_wm_area(C), 
						  "Channel", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
		
		xmax= GetButStringLength("Marker");
		uiDefPulldownBut(block, action_markermenu, CTX_wm_area(C), 
					  "Marker", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		if (saction->mode == SACTCONT_GPENCIL) {
			xmax= GetButStringLength("Frame");
			uiDefPulldownBut(block, action_framemenu, CTX_wm_area(C), 
						  "Frame", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
		else {
			xmax= GetButStringLength("Key");
			uiDefPulldownBut(block, action_keymenu, CTX_wm_area(C), 
						  "Key", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* MODE SELECTOR */
	uiDefButC(block, MENU, B_REDR, 
			"Editor Mode %t|DopeSheet %x3|Action Editor %x0|ShapeKey Editor %x1|Grease Pencil %x2", 
			xco,yco,90,YIC, &saction->mode, 0, 1, 0, 0, 
			"Editing modes for this editor");
	
	
	xco += (90 + 8);
	
	/*if (ac.data)*/ 
	{
		/* MODE-DEPENDENT DRAWING */
		if (saction->mode == SACTCONT_DOPESHEET) {
			/* FILTERING OPTIONS */
			xco -= 10;
			
			//uiBlockBeginAlign(block);
				uiDefIconButBitI(block, TOG, ADS_FILTER_ONLYSEL, B_REDR, ICON_RESTRICT_SELECT_OFF,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Only display selected Objects");
			//uiBlockEndAlign(block);
			xco += 5;
			
			uiBlockBeginAlign(block);
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOSCE, B_REDR, ICON_SCENE_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Scene Animation");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOWOR, B_REDR, ICON_WORLD_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display World Animation");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOSHAPEKEYS, B_REDR, ICON_SHAPEKEY_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display ShapeKeys");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOMAT, B_REDR, ICON_MATERIAL_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Materials");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOLAM, B_REDR, ICON_LAMP_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Lamps");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCAM, B_REDR, ICON_CAMERA_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Cameras");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCUR, B_REDR, ICON_CURVE_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Curves");
			uiBlockEndAlign(block);
			xco += 30;
		}
		else if (saction->mode == SACTCONT_ACTION) { // not too appropriate for shapekeys atm...
			/* NAME ETC */
			//uiClearButLock();
			
			/* NAME ETC (it is assumed that */
			xco= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)saction->action, ID_AC, &saction->pin, xco, yco,
				saction_idpoin_handle, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_DELETE|UI_ID_FAKE_USER|UI_ID_ALONE|UI_ID_PIN);
			
			xco += 8;
		}
		
		/* COPY PASTE */
		uiBlockBeginAlign(block);
		uiDefIconBut(block, BUT, B_ACTCOPYKEYS, ICON_COPYDOWN,	xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected keyframes from the selected channel(s) to the buffer");
		uiDefIconBut(block, BUT, B_ACTPASTEKEYS, ICON_PASTEDOWN,	xco+=XIC,yco,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the keyframes from the buffer");
		uiBlockEndAlign(block);
		xco += (XIC + 8);
		
		/* draw AUTOSNAP */
		if (saction->mode != SACTCONT_GPENCIL) {
			if (saction->flag & SACTION_DRAWTIME) {
				uiDefButC(block, MENU, B_REDR,
						"Auto-Snap Keyframes %t|No Snap %x0|Second Step %x1|Nearest Second %x2|Nearest Marker %x3", 
						xco,yco,70,YIC, &(saction->autosnap), 0, 1, 0, 0, 
						"Auto-snapping mode for keyframes when transforming");
			}
			else {
				uiDefButC(block, MENU, B_REDR, 
						"Auto-Snap Keyframes %t|No Snap %x0|Frame Step %x1|Nearest Frame %x2|Nearest Marker %x3", 
						xco,yco,70,YIC, &(saction->autosnap), 0, 1, 0, 0, 
						"Auto-snapping mode for keyframes when transforming");
			}
			
			xco += (70 + 8);
		}
		
		/* draw LOCK */
			// XXX this feature is probably not relevant anymore!
		//uiDefIconButS(block, ICONTOG, B_LOCK, ICON_UNLOCKED,	xco, yco, XIC, YIC, 
		//			  &(saction->lock), 0, 0, 0, 0, 
		//			  "Updates other affected window spaces automatically "
		//			  "to reflect changes in real time");
	}

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, (int)(ar->v2d.tot.ymax-ar->v2d.tot.ymin));
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


