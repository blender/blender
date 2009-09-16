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
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"
#include "ED_util.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "action_intern.h"

enum {
	B_REDR= 0,
} eActHeader_Events;

/* ********************************************************* */
/* Menu Defines... */

static void act_viewmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceAction *sact= CTX_wm_space_action(C);
	PointerRNA spaceptr;
	
	/* retrieve state */
	RNA_pointer_create(&sc->id, &RNA_SpaceDopeSheetEditor, sact, &spaceptr);
	
	/* create menu */
	//uiItemO(layout, NULL, ICON_MENU_PANEL, "ACT_OT_properties");
	
	//uiItemS(layout);
	
	uiItemR(layout, NULL, 0, &spaceptr, "show_cframe_indicator", 0);
	uiItemR(layout, NULL, 0, &spaceptr, "show_sliders", 0);
	uiItemR(layout, NULL, 0, &spaceptr, "automerge_keyframes", 0);
	
	if (sact->flag & SACTION_DRAWTIME)
		uiItemO(layout, "Show Frames", 0, "ANIM_OT_time_toggle");
	else
		uiItemO(layout, "Show Seconds", 0, "ANIM_OT_time_toggle");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ANIM_OT_previewrange_set");
	uiItemO(layout, NULL, 0, "ANIM_OT_previewrange_clear");
	
	uiItemO(layout, NULL, 0, "ACT_OT_previewrange_set");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ACT_OT_frame_jump");
	
	uiItemO(layout, NULL, 0, "ACT_OT_view_all");
	
	if (sa->full) 
		uiItemO(layout, NULL, 0, "SCREEN_OT_screen_full_area"); // "Tile Window", Ctrl UpArrow
	else 
		uiItemO(layout, NULL, 0, "SCREEN_OT_screen_full_area"); // "Maximize Window", Ctrl DownArrow
}

static void act_selectmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemO(layout, NULL, 0, "ACT_OT_select_all_toggle");
	uiItemBooleanO(layout, "Invert All", 0, "ACT_OT_select_all_toggle", "invert", 1);
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ACT_OT_select_border");
	uiItemBooleanO(layout, "Border Axis Range", 0, "ACT_OT_select_border", "axis_range", 1);
	
	uiItemS(layout);
	
	uiItemEnumO(layout, "Columns on Selected Keys", 0, "ACT_OT_select_column", "mode", ACTKEYS_COLUMNSEL_KEYS);
	uiItemEnumO(layout, "Column on Current Frame", 0, "ACT_OT_select_column", "mode", ACTKEYS_COLUMNSEL_CFRA);
	
	uiItemEnumO(layout, "Columns on Selected Markers", 0, "ACT_OT_select_column", "mode", ACTKEYS_COLUMNSEL_MARKERS_COLUMN);
	uiItemEnumO(layout, "Between Selected Markers", 0, "ACT_OT_select_column", "mode", ACTKEYS_COLUMNSEL_MARKERS_BETWEEN);
}

static void act_channelmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemO(layout, NULL, 0, "ANIM_OT_channels_setting_toggle");
	uiItemO(layout, NULL, 0, "ANIM_OT_channels_setting_enable");
	uiItemO(layout, NULL, 0, "ANIM_OT_channels_setting_disable");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ANIM_OT_channels_editable_toggle");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ANIM_OT_channels_expand");
	uiItemO(layout, NULL, 0, "ANIM_OT_channels_collapse");
}

static void act_gplayermenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	//uiItemMenuF(layout, "Transform", 0, nla_edit_transformmenu, NULL, NULL);
	//uiItemS(layout);
	//uiItemO(layout, NULL, 0, "NLAEDIT_OT_duplicate");
}

static void act_edit_transformmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemEnumO(layout, "Grab/Move", 0, "TFM_OT_transform", "mode", TFM_TIME_TRANSLATE);
	uiItemEnumO(layout, "Extend", 0, "TFM_OT_transform", "mode", TFM_TIME_EXTEND);
	uiItemEnumO(layout, "Scale", 0, "TFM_OT_transform", "mode", TFM_TIME_SCALE);
}

static void act_edit_snapmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT); 
	uiItemEnumO(layout, NULL, 0, "ACT_OT_snap", "type", ACTKEYS_SNAP_CFRA);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_snap", "type", ACTKEYS_SNAP_NEAREST_FRAME);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_snap", "type", ACTKEYS_SNAP_NEAREST_SECOND);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_snap", "type", ACTKEYS_SNAP_NEAREST_MARKER);
}

static void act_edit_mirrormenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_mirror", "type", ACTKEYS_MIRROR_CFRA);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_mirror", "type", ACTKEYS_MIRROR_YAXIS);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_mirror", "type", ACTKEYS_MIRROR_XAXIS);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_mirror", "type", ACTKEYS_MIRROR_MARKER);
}

static void act_edit_keytypesmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_keyframe_type", "type", BEZT_KEYTYPE_KEYFRAME);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_keyframe_type", "type", BEZT_KEYTYPE_BREAKDOWN);
}

static void act_edit_handlesmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_handle_type", "type", HD_FREE);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_handle_type", "type", HD_AUTO);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_handle_type", "type", HD_VECT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_handle_type", "type", HD_ALIGN);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_handle_type", "type", HD_AUTO_ANIM); // xxx?
}

static void act_edit_ipomenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_interpolation_type", "type", BEZT_IPO_CONST);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_interpolation_type", "type", BEZT_IPO_LIN);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_interpolation_type", "type", BEZT_IPO_BEZ);
}

static void act_edit_expomenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_extrapolation_type", "type", FCURVE_EXTRAPOLATE_CONSTANT);
	uiItemEnumO(layout, NULL, 0, "ACT_OT_extrapolation_type", "type", FCURVE_EXTRAPOLATE_LINEAR);
}

static void act_editmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemMenuF(layout, "Transform", 0, act_edit_transformmenu, NULL);
	uiItemMenuF(layout, "Snap", 0, act_edit_snapmenu, NULL);
	uiItemMenuF(layout, "Mirror", 0, act_edit_mirrormenu, NULL);
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ACT_OT_insert_keyframe");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ACT_OT_duplicate");
	uiItemO(layout, NULL, 0, "ACT_OT_delete");
	
	uiItemS(layout);
	
	uiItemMenuF(layout, "Keyframe Type", 0, act_edit_keytypesmenu, NULL);
	uiItemMenuF(layout, "Handle Type", 0, act_edit_handlesmenu, NULL);
	uiItemMenuF(layout, "Interpolation Mode", 0, act_edit_ipomenu, NULL);
	uiItemMenuF(layout, "Extrapolation Mode", 0, act_edit_expomenu, NULL);
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ACT_OT_clean");
	uiItemO(layout, NULL, 0, "ACT_OT_sample");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ACT_OT_copy");
	uiItemO(layout, NULL, 0, "ACT_OT_paste");
}

/* ************************ header area region *********************** */

static void do_action_buttons(bContext *C, void *arg, int event)
{
	switch (event) {
		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
	}
}

void action_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceAction *saction= CTX_wm_space_action(C);
	bAnimContext ac;
	uiBlock *block;
	int xco, yco= 3, xmax;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_action_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* get context... (also syncs data) */
	ANIM_animdata_get_context(C, &ac);
	
	if ((sa->flag & HEADER_NO_PULLDOWN)==0) {
		
		xmax= GetButStringLength("View");
		uiDefMenuBut(block, act_viewmenu, NULL, "View", xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefMenuBut(block, act_selectmenu, NULL, "Select", xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		if ( (saction->mode == SACTCONT_DOPESHEET) ||
			 ((saction->action) && (saction->mode==SACTCONT_ACTION)) ) 
		{
			xmax= GetButStringLength("Channel");
			uiDefMenuBut(block, act_channelmenu, NULL, "Channel", xco, yco, xmax-3, 20, "");
			xco+= xmax;
		}
		else if (saction->mode==SACTCONT_GPENCIL) {
			xmax= GetButStringLength("Channel");
			uiDefMenuBut(block, act_gplayermenu, NULL, "Channel", xco, yco, xmax-3, 20, "");
			xco+= xmax;
		}
		
		//xmax= GetButStringLength("Marker");
		//uiDefMenuBut(block, act_markermenu, NULL, "Marker", xco, yco, xmax-3, 20, "");
		//xco+= xmax;
		
		if (saction->mode == SACTCONT_GPENCIL) {
			//xmax= GetButStringLength("Frame");
			//uiDefMenuBut(block, act_selectmenu, NULL, "Frame", xco, yco, xmax-3, 20, "");
			//xco+= xmax;
		}
		else {
			xmax= GetButStringLength("Key");
			uiDefMenuBut(block, act_editmenu, NULL, "Key", xco, yco, xmax-3, 20, "");
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
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOMBA, B_REDR, ICON_META_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display MetaBalls");
				uiDefIconButBitI(block, TOGN, ADS_FILTER_NOPART, B_REDR, ICON_PARTICLE_DATA,	(short)(xco+=XIC),yco,XIC,YIC, &(saction->ads.filterflag), 0, 0, 0, 0, "Display Particles");
			uiBlockEndAlign(block);
			xco += 30;
		}
		else if (saction->mode == SACTCONT_ACTION) {
			uiLayout *layout;
			bScreen *sc= CTX_wm_screen(C);
			PointerRNA ptr;

			RNA_pointer_create(&sc->id, &RNA_SpaceDopeSheetEditor, saction, &ptr);
			
			layout= uiBlockLayout(block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, xco, 20+3, 20, 1, U.uistyles.first);
			uiTemplateID(layout, (bContext*)C, &ptr, "action", "ACT_OT_new", NULL, NULL);
			uiBlockLayoutResolve(block, &xco, NULL);
			
			xco += 8;
		}
		
		/* draw AUTOSNAP */
		if (saction->mode != SACTCONT_GPENCIL) {
			if (saction->flag & SACTION_DRAWTIME) {
				uiDefButC(block, MENU, B_REDR,
						"Auto-Snap Keyframes %t|No Snap %x0|Second Step %x1|Nearest Second %x2|Nearest Marker %x3", 
						xco,yco,90,YIC, &(saction->autosnap), 0, 1, 0, 0, 
						"Auto-snapping mode for keyframes when transforming");
			}
			else {
				uiDefButC(block, MENU, B_REDR, 
						"Auto-Snap Keyframes %t|No Snap %x0|Frame Step %x1|Nearest Frame %x2|Nearest Marker %x3", 
						xco,yco,90,YIC, &(saction->autosnap), 0, 1, 0, 0, 
						"Auto-snapping mode for keyframes when transforming");
			}
			
			xco += (90 + 8);
		}
		
		/* COPY PASTE */
		uiBlockBeginAlign(block);
			uiDefIconButO(block, BUT, "ACT_OT_copy", WM_OP_INVOKE_REGION_WIN, ICON_COPYDOWN, xco,yco,XIC,YIC, "Copies the selected keyframes to the buffer.");
			xco += XIC;
			uiDefIconButO(block, BUT, "ACT_OT_paste", WM_OP_INVOKE_REGION_WIN, ICON_PASTEDOWN, xco,yco,XIC,YIC, "Pastes the keyframes from the buffer into the selected channels.");
		uiBlockEndAlign(block);
		xco += (XIC + 8);
	}

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, (int)(ar->v2d.tot.ymax-ar->v2d.tot.ymin));
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


