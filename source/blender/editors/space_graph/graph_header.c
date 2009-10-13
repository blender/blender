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
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_transform.h"
#include "ED_screen.h"
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

#include "graph_intern.h"

/* ********************************************************* */
/* Menu Defines... */

static void graph_viewmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceIpo *sipo= CTX_wm_space_graph(C);
	PointerRNA spaceptr;
	
	/* retrieve state */
	RNA_pointer_create(&sc->id, &RNA_SpaceGraphEditor, sipo, &spaceptr);
	
	/* create menu */
	uiItemO(layout, NULL, ICON_MENU_PANEL, "GRAPH_OT_properties");
	
	uiItemS(layout);
	
	uiItemR(layout, NULL, 0, &spaceptr, "show_cframe_indicator", 0);
	uiItemR(layout, NULL, 0, &spaceptr, "show_sliders", 0);
	uiItemR(layout, NULL, 0, &spaceptr, "automerge_keyframes", 0);
	
	if (sipo->flag & SIPO_NOHANDLES)
		uiItemO(layout, "Show Handles", ICON_CHECKBOX_DEHLT, "GRAPH_OT_handles_view_toggle");
	else
		uiItemO(layout, "Show Handles", ICON_CHECKBOX_HLT, "GRAPH_OT_handles_view_toggle");
	
	uiItemR(layout, NULL, 0, &spaceptr, "only_selected_curves_handles", 0);
	
	
	if (sipo->flag & SIPO_DRAWTIME)
		uiItemO(layout, "Show Frames", 0, "ANIM_OT_time_toggle");
	else
		uiItemO(layout, "Show Seconds", 0, "ANIM_OT_time_toggle");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "ANIM_OT_previewrange_set");
	uiItemO(layout, NULL, 0, "ANIM_OT_previewrange_clear");
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_previewrange_set");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_frame_jump");
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_view_all");
	
	if (sa->full) 
		uiItemO(layout, NULL, 0, "SCREEN_OT_screen_full_area"); // "Tile Window", Ctrl UpArrow
	else 
		uiItemO(layout, NULL, 0, "SCREEN_OT_screen_full_area"); // "Maximize Window", Ctrl DownArrow
}

static void graph_selectmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemO(layout, NULL, 0, "GRAPH_OT_select_all_toggle");
	uiItemBooleanO(layout, "Invert All", 0, "GRAPH_OT_select_all_toggle", "invert", 1);
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_select_border");
	uiItemBooleanO(layout, "Border Axis Range", 0, "GRAPH_OT_select_border", "axis_range", 1);
	
	uiItemS(layout);
	
	uiItemEnumO(layout, "Columns on Selected Keys", 0, "GRAPH_OT_select_column", "mode", GRAPHKEYS_COLUMNSEL_KEYS);
	uiItemEnumO(layout, "Column on Current Frame", 0, "GRAPH_OT_select_column", "mode", GRAPHKEYS_COLUMNSEL_CFRA);
	
	uiItemEnumO(layout, "Columns on Selected Markers", 0, "GRAPH_OT_select_column", "mode", GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN);
	uiItemEnumO(layout, "Between Selected Markers", 0, "GRAPH_OT_select_column", "mode", GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN);
}

static void graph_channelmenu(bContext *C, uiLayout *layout, void *arg_unused)
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

static void graph_edit_transformmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemEnumO(layout, "Grab/Move", 0, "TFM_OT_transform", "mode", TFM_TIME_TRANSLATE);
	uiItemEnumO(layout, "Extend", 0, "TFM_OT_transform", "mode", TFM_TIME_EXTEND);
	uiItemEnumO(layout, "Scale", 0, "TFM_OT_transform", "mode", TFM_TIME_SCALE);
}

static void graph_edit_snapmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_snap", "type", GRAPHKEYS_SNAP_CFRA);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_snap", "type", GRAPHKEYS_SNAP_NEAREST_FRAME);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_snap", "type", GRAPHKEYS_SNAP_NEAREST_SECOND);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_snap", "type", GRAPHKEYS_SNAP_NEAREST_MARKER);
}

static void graph_edit_mirrormenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_mirror", "type", GRAPHKEYS_MIRROR_CFRA);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_mirror", "type", GRAPHKEYS_MIRROR_YAXIS);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_mirror", "type", GRAPHKEYS_MIRROR_XAXIS);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_mirror", "type", GRAPHKEYS_MIRROR_MARKER);
}

static void graph_edit_handlesmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_handle_type", "type", HD_FREE);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_handle_type", "type", HD_AUTO);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_handle_type", "type", HD_VECT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_handle_type", "type", HD_ALIGN);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_handle_type", "type", HD_AUTO_ANIM); // xxx?
}

static void graph_edit_ipomenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_interpolation_type", "type", BEZT_IPO_CONST);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_interpolation_type", "type", BEZT_IPO_LIN);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_interpolation_type", "type", BEZT_IPO_BEZ);
}

static void graph_edit_expomenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_extrapolation_type", "type", FCURVE_EXTRAPOLATE_CONSTANT);
	uiItemEnumO(layout, NULL, 0, "GRAPH_OT_extrapolation_type", "type", FCURVE_EXTRAPOLATE_LINEAR);
}

static void graph_editmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	uiItemMenuF(layout, "Transform", 0, graph_edit_transformmenu, NULL);
	uiItemMenuF(layout, "Snap", 0, graph_edit_snapmenu, NULL);
	uiItemMenuF(layout, "Mirror", 0, graph_edit_mirrormenu, NULL);
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_insert_keyframe");
	uiItemO(layout, NULL, 0, "GRAPH_OT_fmodifier_add");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_duplicate");
	uiItemO(layout, NULL, 0, "GRAPH_OT_delete");
	
	uiItemS(layout);
	
	uiItemMenuF(layout, "Handle Type", 0, graph_edit_handlesmenu, NULL);
	uiItemMenuF(layout, "Interpolation Mode", 0, graph_edit_ipomenu, NULL);
	uiItemMenuF(layout, "Extrapolation Mode", 0, graph_edit_expomenu, NULL);
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_clean");
	uiItemO(layout, NULL, 0, "GRAPH_OT_sample");
	uiItemO(layout, NULL, 0, "GRAPH_OT_bake");
	
	uiItemS(layout);
	
	uiItemO(layout, NULL, 0, "GRAPH_OT_copy");
	uiItemO(layout, NULL, 0, "GRAPH_OT_paste");
}

/* ********************************************************* */

enum {
	B_REDR = 0,
	B_MODECHANGE,
} eGraphEdit_Events;

static void do_graph_buttons(bContext *C, void *arg, int event)
{
	ED_area_tag_refresh(CTX_wm_area(C));
	ED_area_tag_redraw(CTX_wm_area(C));
}


void graph_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceIpo *sipo= CTX_wm_space_graph(C);
	uiBlock *block;
	int xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_graph_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if ((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		xmax= GetButStringLength("View");
		uiDefMenuBut(block, graph_viewmenu, NULL, "View", xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefMenuBut(block, graph_selectmenu, NULL, "Select", xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Channel");
		uiDefMenuBut(block, graph_channelmenu, NULL, "Channel", xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Key");
		uiDefMenuBut(block, graph_editmenu, NULL, "Key", xco, yco, xmax-3, 20, "");
		xco+= xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* mode selector */
	uiDefButS(block, MENU, B_MODECHANGE, 
			"Editor Mode %t|F-Curve Editor %x0|Drivers %x1", 
			xco,yco,110,YIC, &sipo->mode, 0, 1, 0, 0, 
			"Editing modes for this editor");
	xco+= 120;
	
	/* filtering buttons */
	xco= ANIM_headerUI_standard_buttons(C, sipo->ads, block, xco, yco);
	
	/* auto-snap selector */
	if (sipo->flag & SIPO_DRAWTIME) {
		uiDefButS(block, MENU, B_REDR,
				"Auto-Snap Keyframes %t|No Time-Snap %x0|Nearest Second %x2|Nearest Marker %x3", 
				xco,yco,90,YIC, &sipo->autosnap, 0, 1, 0, 0, 
				"Auto-snapping mode for keyframe times when transforming");
	}
	else {
		uiDefButS(block, MENU, B_REDR, 
				"Auto-Snap Keyframes %t|No Time-Snap %x0|Nearest Frame %x2|Nearest Marker %x3", 
				xco,yco,90,YIC, &sipo->autosnap, 0, 1, 0, 0, 
				"Auto-snapping mode for keyframe times when transforming");
	}
	xco += 98;
	
	/* copy + paste */
	uiBlockBeginAlign(block);
		uiDefIconButO(block, BUT, "GRAPH_OT_copy", WM_OP_INVOKE_REGION_WIN, ICON_COPYDOWN, xco+=XIC,yco,XIC,YIC, "Copies the selected keyframes from the selected channel(s) to the buffer");
		uiDefIconButO(block, BUT, "GRAPH_OT_paste", WM_OP_INVOKE_REGION_WIN, ICON_PASTEDOWN, xco+=XIC,yco,XIC,YIC, "Pastes the keyframes from the buffer");
	uiBlockEndAlign(block);
	xco += (XIC + 8);
	
	/* ghost curves */
	// XXX these icons need to be changed
	if (sipo->ghostCurves.first)
		uiDefIconButO(block, BUT, "GRAPH_OT_ghost_curves_clear", WM_OP_INVOKE_REGION_WIN, ICON_GHOST_DISABLED, xco,yco,XIC,YIC, "Clear F-Curve snapshots (Ghosts) for this Graph Editor instance");
	else 
		uiDefIconButO(block, BUT, "GRAPH_OT_ghost_curves_create", WM_OP_INVOKE_REGION_WIN, ICON_GHOST_ENABLED, xco,yco,XIC,YIC, "Create snapshot (Ghosts) of selected F-Curves as background aid for this Graph Editor instance");
	xco+= XIC;
	
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, (int)(ar->v2d.tot.ymax - ar->v2d.tot.ymin));
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


