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

#include "DNA_action_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

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
	#define G_SACTION_HASMARKERS \
		((G.saction->action && G.saction->action->markers.first) \
		 || (G.scene->markers.first))
#endif

/* ------------------------------- */

/* *************************************************************** */
/* menus */

/* Key menu ---------------------------  */

static void do_keymenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *action_keymenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_keymenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_keymenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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

static void do_framemenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *action_framemenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_framemenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_framemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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
	
}

static uiBlock *action_markermenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_markermenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_markermenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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

static void do_gplayermenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *action_gplayermenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_gplayermenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_gplayermenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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

static void do_channelmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *action_channelmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_channelmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_channelmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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

static void do_selectmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *action_selectmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_selectmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_selectmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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
	
}

static uiBlock *action_viewmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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
			ED_region_tag_redraw(CTX_wm_region(C));
			break;
			
		case B_ACTCOPYKEYS:
			WM_operator_name_call(C, "ACT_OT_keyframes_copy", WM_OP_EXEC_REGION_WIN, NULL, NULL);
			break;
		case B_ACTPASTEKEYS:
			WM_operator_name_call(C, "ACT_OT_keyframes_paste", WM_OP_EXEC_REGION_WIN, NULL, NULL);
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
	
	if (ac.data) {
		/* MODE-DEPENDENT DRAWING */
		if (saction->mode == SACTCONT_DOPESHEET) {
			/* FILTERING OPTIONS */
			xco -= 10;
			
			//uiBlockBeginAlign(block);
				uiDefIconButBitI(block, TOG, ADS_FILTER_ONLYSEL, B_REDR, ICON_RESTRICT_SELECT_OFF,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Only display selected Objects");
			//uiBlockEndAlign(block);
			xco += 5;
			
			uiBlockBeginAlign(block);
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOOBJ, B_REDR, ICON_OBJECT,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Non-Armature Objects");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOARM, B_REDR, ICON_ARMATURE,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Armature Objects");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOSHAPEKEYS, B_REDR, ICON_EDIT,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display ShapeKeys");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOMAT, B_REDR, ICON_MATERIAL,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Materials");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOLAM, B_REDR, ICON_LAMP,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Lamps");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCAM, B_REDR, ICON_CAMERA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Cameras");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCUR, B_REDR, ICON_CURVE,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Curves");
			uiBlockEndAlign(block);
			xco += 5;
			
			uiBlockBeginAlign(block);		
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOIPOS, B_REDR, ICON_IPO,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Object IPO's");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOACTS, B_REDR, ICON_ACTION,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Actions");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCONSTRAINTS, B_REDR, ICON_CONSTRAINT,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Object Constraints");
			uiBlockEndAlign(block);
			xco += 30;
		}
		else if (saction->mode == SACTCONT_ACTION) { // not too appropriate for shapekeys atm...
			/* NAME ETC */
				// XXX missing stuff here!
			//ob= OBACT;
			//from = (ID *)ob;
			
			//xco= std_libbuttons(block, xco, 0, B_ACTPIN, saction->pin, 
			//					B_ACTIONBROWSE, ID_AC, 0, (ID*)saction->action, 
			//					from, &(saction->actnr), B_ACTALONE, 
			//					B_ACTLOCAL, B_ACTIONDELETE, 0, B_KEEPDATA);	
			
			//uiClearButLock();
			
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


