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
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "ED_anim_api.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_markers.h"

/* button events */
enum {
	B_REDR 	= 0,
} eActHeader_ButEvents;

/* ************************ header area region *********************** */

static void do_viewmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *dummy_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "dummy_viewmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
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

static void do_nla_buttons(bContext *C, void *arg, int event)
{
	switch (event) {
		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
	}
}


void nla_header_buttons(const bContext *C, ARegion *ar)
{
	SpaceNla *snla= (SpaceNla *)CTX_wm_space_data(C);
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	int xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_nla_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if ((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, dummy_viewmenu, CTX_wm_area(C), 
						 "View", xco, yco-2, xmax-3, 24, "");
		xco+=XIC+xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* filtering buttons */
	if (snla->ads) {
		uiBlockBeginAlign(block);
			uiDefIconButBitI(block, TOG, ADS_FILTER_ONLYSEL, B_REDR, ICON_RESTRICT_SELECT_OFF,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Only display selected Objects");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NLA_NOACT, B_REDR, ICON_ACTION,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Include AnimData blocks with no NLA Data");
		uiBlockEndAlign(block);
		xco += 5;
		
		uiBlockBeginAlign(block);
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOSCE, B_REDR, ICON_SCENE_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display Scene Animation");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOWOR, B_REDR, ICON_WORLD_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display World Animation");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOSHAPEKEYS, B_REDR, ICON_SHAPEKEY_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display ShapeKeys");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOMAT, B_REDR, ICON_MATERIAL_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display Materials");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOLAM, B_REDR, ICON_LAMP_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display Lamps");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCAM, B_REDR, ICON_CAMERA_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display Cameras");
			uiDefIconButBitI(block, TOGN, ADS_FILTER_NOCUR, B_REDR, ICON_CURVE_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(snla->ads->filterflag), 0, 0, 0, 0, "Display Curves");
		uiBlockEndAlign(block);
		xco += 15;
	}
	else {
		// XXX this case shouldn't happen at all... for now, just pad out same amount of space
		xco += 7*XIC + 15;
	}
	xco += (XIC + 8);
	
	/* auto-snap selector */
	if (snla->flag & SNLA_DRAWTIME) {
		uiDefButS(block, MENU, B_REDR,
				"Auto-Snap Keyframes %t|No Time-Snap %x0|Nearest Second %x2|Nearest Marker %x3", 
				xco,yco,90,YIC, &snla->autosnap, 0, 1, 0, 0, 
				"Auto-snapping mode for times when transforming");
	}
	else {
		uiDefButS(block, MENU, B_REDR, 
				"Auto-Snap Keyframes %t|No Time-Snap %x0|Nearest Frame %x2|Nearest Marker %x3", 
				xco,yco,90,YIC, &snla->autosnap, 0, 1, 0, 0, 
				"Auto-snapping mode for times when transforming");
	}
	xco += 98;
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


