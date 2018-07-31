/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_ops.c
 *  \ingroup edgpencil
 */


#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "BLI_sys_types.h"

#include "BKE_context.h"
#include "BKE_brush.h"
#include "BKE_gpencil.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_transform.h"

#include "gpencil_intern.h"

/* ****************************************** */
/* Grease Pencil Keymaps */

/* Generic Drawing Keymap - Annotations */
static void ed_keymap_gpencil_general(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil", 0, 0);
	wmKeyMapItem *kmi;

	/* Draw  --------------------------------------- */
	/* draw */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_annotate", LEFTMOUSE, KM_PRESS, 0, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* draw - straight lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_annotate", LEFTMOUSE, KM_PRESS, KM_ALT, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_STRAIGHT);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* draw - poly lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_annotate", LEFTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_POLY);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* erase */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_annotate", RIGHTMOUSE, KM_PRESS, 0, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_ERASER);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* Viewport Tools ------------------------------- */

	/* Enter EditMode */
	WM_keymap_add_item(keymap, "GPENCIL_OT_editmode_toggle", TABKEY, KM_PRESS, 0, DKEY);

	/* Pie Menu - For standard tools */
	WM_keymap_add_menu_pie(keymap, "GPENCIL_MT_pie_tool_palette", QKEY, KM_PRESS, 0, DKEY);
	WM_keymap_add_menu_pie(keymap, "GPENCIL_MT_pie_settings_palette", WKEY, KM_PRESS, 0, DKEY);

	/* Add Blank Frame */
	/* XXX: BKEY or NKEY? BKEY is easier to reach from DKEY, so we'll use that for now */
	WM_keymap_add_item(keymap, "GPENCIL_OT_blank_frame_add", BKEY, KM_PRESS, 0, DKEY);

	/* Delete Active Frame - For easier video tutorials/review sessions */
	/* NOTE: This works even when not in EditMode */
	WM_keymap_add_item(keymap, "GPENCIL_OT_active_frames_delete_all", XKEY, KM_PRESS, 0, DKEY);
	WM_keymap_add_item(keymap, "GPENCIL_OT_active_frames_delete_all", DELKEY, KM_PRESS, 0, DKEY);
}

/* ==================== */

/* Poll callback for stroke editing mode */
static bool gp_stroke_editmode_poll(bContext *C)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	return (gpd && (gpd->flag & GP_DATA_STROKE_EDITMODE));
}

/* Poll callback for stroke painting mode */
static bool gp_stroke_paintmode_poll(bContext *C)
{
	/* TODO: limit this to mode, but review 2D editors */
	bGPdata *gpd = CTX_data_gpencil_data(C);
	return (gpd && (gpd->flag & GP_DATA_STROKE_PAINTMODE));
}

/* Poll callback for stroke painting (draw brush) */
static bool gp_stroke_paintmode_draw_poll(bContext *C)
{
	/* TODO: limit this to mode, but review 2D editors */
	bGPdata *gpd = CTX_data_gpencil_data(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Brush *brush = BKE_brush_getactive_gpencil(ts);
	return (gpd && (gpd->flag & GP_DATA_STROKE_PAINTMODE) && (brush) &&
	        (brush->gpencil_settings->brush_type == GP_BRUSH_TYPE_DRAW));
}

/* Poll callback for stroke painting (erase brush) */
static bool gp_stroke_paintmode_erase_poll(bContext *C)
{
	/* TODO: limit this to mode, but review 2D editors */
	bGPdata *gpd = CTX_data_gpencil_data(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Brush *brush = BKE_brush_getactive_gpencil(ts);
	return (gpd && (gpd->flag & GP_DATA_STROKE_PAINTMODE) && (brush) &&
	        (brush->gpencil_settings->brush_type == GP_BRUSH_TYPE_ERASE));
}

/* Poll callback for stroke painting (fill) */
static bool gp_stroke_paintmode_fill_poll(bContext *C)
{
	/* TODO: limit this to mode, but review 2D editors */
	bGPdata *gpd = CTX_data_gpencil_data(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Brush *brush = BKE_brush_getactive_gpencil(ts);
	return (gpd && (gpd->flag & GP_DATA_STROKE_PAINTMODE) && (brush) &&
	        (brush->gpencil_settings->brush_type == GP_BRUSH_TYPE_FILL));
}

/* Poll callback for stroke sculpting mode */
static bool gp_stroke_sculptmode_poll(bContext *C)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	Object *ob = CTX_data_active_object(C);
	ScrArea *sa = CTX_wm_area(C);

	/* if not gpencil object and not view3d, need sculpt keys if edit mode */
	if (sa->spacetype != SPACE_VIEW3D) {
		return ((gpd) && (gpd->flag & GP_DATA_STROKE_EDITMODE));
	}
	else {
		/* weight paint is a submode of sculpt */
		if ((ob) && (ob->type == OB_GPENCIL)) {
			return GPENCIL_SCULPT_OR_WEIGHT_MODE(gpd);
		}
	}

	return 0;
}

/* Poll callback for stroke weight paint mode */
static bool gp_stroke_weightmode_poll(bContext *C)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	Object *ob = CTX_data_active_object(C);

	if ((ob) && (ob->type == OB_GPENCIL)) {
		return (gpd && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE));
	}

	return 0;
}

static void ed_keymap_gpencil_selection(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;

	/* select all */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_SELECT);
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_all", AKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_DESELECT);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	/* circle select */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_circle", CKEY, KM_PRESS, 0, 0);

	/* border select */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_border", BKEY, KM_PRESS, 0, 0);

	/* lasso select */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	/* In the Node Editor, lasso select needs ALT modifier too (as somehow CTRL+LMB drag gets taken for "cut" quite early)
	* There probably isn't too much harm adding this for other editors too as part of standard GP editing keymap. This hotkey
	* combo doesn't seem to see much use under standard scenarios?
	*/
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_SHIFT | KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	/* whole stroke select */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "entire_strokes", true);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "entire_strokes", true);
	RNA_boolean_set(kmi->ptr, "extend", true);

	/* select linked */
	/* NOTE: While LKEY is redundant, not having it breaks the mode illusion too much */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);

	/* select alternate */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_alternate", LKEY, KM_PRESS, KM_SHIFT, 0);

	/* select grouped */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_grouped", GKEY, KM_PRESS, KM_SHIFT, 0);

	/* select more/less */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);

}

static void ed_keymap_gpencil_sculpt(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;

	/* Pie Menu - For settings/tools easy access */
	WM_keymap_add_menu_pie(keymap, "GPENCIL_PIE_sculpt", EKEY, KM_PRESS, 0, DKEY);

	/* Sculpting ------------------------------------- */

	/* Brush-Based Editing:
	*   EKEY + LMB                          = Single stroke, draw immediately
	*        + Other Modifiers (Ctrl/Shift) = Invert, Smooth, etc.
	*
	* For the modal version, use D+E -> Sculpt
	*/
	/* GPXX: disabled to make toolsystem works */
	//kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, 0, 0);
	//RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	RNA_boolean_set(kmi->ptr, "keep_brush", true);
	/*RNA_boolean_set(kmi->ptr, "use_invert", true);*/

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	RNA_boolean_set(kmi->ptr, "keep_brush", true);
	/*RNA_boolean_set(kmi->ptr, "use_smooth", true);*/

	/* Shift-FKEY = Sculpt Strength */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_sculpt.brush.strength");

	/* FKEY = Sculpt Brush Size */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_sculpt.brush.size");

	/* menu sculpt specials */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_gpencil_sculpt_specials", WKEY, KM_PRESS, 0, 0);
}

static void ed_keymap_gpencil_weight(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;


	/* Brush-Based Editing:
	*   EKEY + LMB                          = Single stroke, draw immediately
	*        + Other Modifiers (Ctrl/Shift) = Invert, Smooth, etc.
	*
	* For the modal version, use D+E -> Sculpt
	*/
	/* GPXX: disabled to make toolsystem works */
	//kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, 0, 0);
	//RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	RNA_boolean_set(kmi->ptr, "keep_brush", true);
	/*RNA_boolean_set(kmi->ptr, "use_invert", true);*/

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	RNA_boolean_set(kmi->ptr, "keep_brush", true);
	/*RNA_boolean_set(kmi->ptr, "use_smooth", true);*/
}

/* Stroke Editing Keymap - Only when editmode is enabled */
static void ed_keymap_gpencil_editing(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Edit Mode", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback - so that this keymap only gets enabled when stroke editmode is enabled */
	keymap->poll = gp_stroke_editmode_poll;

	/* ----------------------------------------------- */

	/* Brush Settings */
	/* NOTE: We cannot expose these in the standard keymap, as they will interfere with regular hotkeys
	 *       in other modes. However, when we are dealing with Stroke Edit Mode, we know for certain
	 *       that the only data being edited is that of the Grease Pencil strokes
	 */

	/* Interpolation */
	WM_keymap_add_item(keymap, "GPENCIL_OT_interpolate", EKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_interpolate_sequence", EKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);

	/* normal select */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);

	/* Selection */
	ed_keymap_gpencil_selection(keymap);

	/* Editing ----------------------------------------- */

	/* duplicate and move selected points */
	WM_keymap_add_item(keymap, "GPENCIL_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);

	/* delete */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_gpencil_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_gpencil_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "GPENCIL_OT_dissolve", XKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_dissolve", DELKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "GPENCIL_OT_active_frames_delete_all", XKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_active_frames_delete_all", DELKEY, KM_PRESS, KM_SHIFT, 0);

	/* menu edit specials */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_gpencil_edit_specials", WKEY, KM_PRESS, 0, 0);

	/* menu separate */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_separate", PKEY, KM_PRESS, 0, 0);

	/* split strokes */
	WM_keymap_add_item(keymap, "GPENCIL_OT_stroke_split", VKEY, KM_PRESS, 0, 0);

	/* join strokes */
	WM_keymap_add_item(keymap, "GPENCIL_OT_stroke_join", JKEY, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_stroke_join", JKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", GP_STROKE_JOINCOPY);

	/* copy + paste */
	WM_keymap_add_item(keymap, "GPENCIL_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);

#ifdef __APPLE__
	WM_keymap_add_item(keymap, "GPENCIL_OT_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif

	/* snap */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);

	/* convert to geometry */
	WM_keymap_add_item(keymap, "GPENCIL_OT_convert", CKEY, KM_PRESS, KM_ALT, 0);


	/* Show/Hide */
	/* NOTE: These are available only in EditMode now, since they clash with general-purpose hotkeys */
	WM_keymap_add_item(keymap, "GPENCIL_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);

	WM_keymap_add_item(keymap, "GPENCIL_OT_selection_opacity_toggle", HKEY, KM_PRESS, KM_CTRL, 0);

	/* toogle multiedit support */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.overlay.use_gpencil_edit_lines");
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.overlay.use_gpencil_multiedit_line_only");

	/* Isolate Layer */
	WM_keymap_add_item(keymap, "GPENCIL_OT_layer_isolate", PADASTERKEY, KM_PRESS, 0, 0);

	/* Move to Layer */
	WM_keymap_add_item(keymap, "GPENCIL_OT_move_to_layer", MKEY, KM_PRESS, 0, 0);

	/* Transform Tools */
	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", GKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);

	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_rotate", RKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_resize", SKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_mirror", MKEY, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_bend", WKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "TRANSFORM_OT_tosphere", SKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "TRANSFORM_OT_shear", SKEY, KM_PRESS, KM_ALT | KM_CTRL | KM_SHIFT, 0);

	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", SKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", TFM_GPENCIL_SHRINKFATTEN);

	/* Proportional Editing */
	ED_keymap_proportional_cycle(keyconf, keymap);
	ED_keymap_proportional_editmode(keyconf, keymap, true);

	/* menu - add GP object (3d view only) */
	WM_keymap_add_item(keymap, "OBJECT_OT_gpencil_add", AKEY, KM_PRESS, KM_SHIFT, 0);

	/* menu vertex group */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_gpencil_vertex_group", GKEY, KM_PRESS, KM_CTRL, 0);

	/* toggle edit mode */
	WM_keymap_add_item(keymap, "GPENCIL_OT_editmode_toggle", TABKEY, KM_PRESS, 0, 0);
}

/* keys for draw with a drawing brush (no fill) */
static void ed_keymap_gpencil_painting_draw(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Paint (Draw brush)", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback */
	keymap->poll = gp_stroke_paintmode_draw_poll;

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* draw - straight lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_STRAIGHT);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* draw - poly lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_POLY);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* erase */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_ERASER);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* Tablet Mappings for Drawing ------------------ */
	/* For now, only support direct drawing using the eraser, as most users using a tablet
	* may still want to use that as their primary pointing device!
	*/
#if 0
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", TABLET_STYLUS, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
#endif

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", TABLET_ERASER, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_ERASER);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* Selection (used by eraser) */
	/* border select */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_border", BKEY, KM_PRESS, 0, 0);

	/* lasso select */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
}

/* keys for draw with a eraser brush (erase) */
static void ed_keymap_gpencil_painting_erase(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Paint (Erase)", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback */
	keymap->poll = gp_stroke_paintmode_erase_poll;

	/* erase */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_ERASER);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", TABLET_ERASER, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_ERASER);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* Selection (used by eraser) */
	/* border select */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_border", BKEY, KM_PRESS, 0, 0);

	/* lasso select */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
}

/* keys for draw with a fill brush */
static void ed_keymap_gpencil_painting_fill(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Paint (Fill)", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback */
	keymap->poll = gp_stroke_paintmode_fill_poll;

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_fill", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "on_back", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_fill", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "on_back", true);

	/* if press alternative key, the brush now it's for drawing areas */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	/* disable straight lines */
	RNA_boolean_set(kmi->ptr, "disable_straight", true);

	/* if press alternative key, the brush now it's for drawing lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	/* disable straight lines */
	RNA_boolean_set(kmi->ptr, "disable_straight", true);
	/* enable special stroke with no fill flag */
	RNA_boolean_set(kmi->ptr, "disable_fill", true);
}

/* Stroke Painting Keymap - Only when paintmode is enabled */
static void ed_keymap_gpencil_painting(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Paint Mode", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback - so that this keymap only gets enabled when stroke paintmode is enabled */
	keymap->poll = gp_stroke_paintmode_poll;

	/* FKEY = Brush Size */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_paint.brush.size");

	/* CTRL + FKEY = Eraser Radius */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "user_preferences.edit.grease_pencil_eraser_radius");

	/* menu draw specials */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_gpencil_draw_specials", WKEY, KM_PRESS, 0, 0);

	/* menu draw delete */
	WM_keymap_add_menu(keymap, "GPENCIL_MT_gpencil_draw_delete", XKEY, KM_PRESS, 0, 0);

}

/* Stroke Sculpting Keymap - Only when sculptmode is enabled */
static void ed_keymap_gpencil_sculpting(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Sculpt Mode", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback - so that this keymap only gets enabled when stroke sculptmode is enabled */
	keymap->poll = gp_stroke_sculptmode_poll;

	/* Selection */
	ed_keymap_gpencil_selection(keymap);

	/* sculpt */
	ed_keymap_gpencil_sculpt(keymap);

	/* toogle multiedit support */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.overlay.use_gpencil_edit_lines");
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.overlay.use_gpencil_multiedit_line_only");
}

/* Stroke Weight Paint Keymap - Only when weight is enabled */
static void ed_keymap_gpencil_weightpainting(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Weight Mode", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback - so that this keymap only gets enabled when stroke sculptmode is enabled */
	keymap->poll = gp_stroke_weightmode_poll;

	/* Selection */
	ed_keymap_gpencil_selection(keymap);

	/* sculpt */
	ed_keymap_gpencil_weight(keymap);

	/* Shift-FKEY = Sculpt Strength */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_sculpt.weight_brush.strength");

	/* FKEY = Sculpt Brush Size */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_sculpt.weight_brush.size");

	/* toogle multiedit support */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.overlay.use_gpencil_edit_lines");
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.overlay.use_gpencil_multiedit_line_only");
}
/* ==================== */

void ED_keymap_gpencil(wmKeyConfig *keyconf)
{
	ed_keymap_gpencil_general(keyconf);
	ed_keymap_gpencil_editing(keyconf);
	ed_keymap_gpencil_painting(keyconf);
	ed_keymap_gpencil_painting_draw(keyconf);
	ed_keymap_gpencil_painting_erase(keyconf);
	ed_keymap_gpencil_painting_fill(keyconf);
	ed_keymap_gpencil_sculpting(keyconf);
	ed_keymap_gpencil_weightpainting(keyconf);
}

/* ****************************************** */

void ED_operatortypes_gpencil(void)
{
	/* Annotations -------------------- */

	WM_operatortype_append(GPENCIL_OT_annotate);

	/* Drawing ----------------------- */

	WM_operatortype_append(GPENCIL_OT_draw);
	WM_operatortype_append(GPENCIL_OT_fill);

	/* Editing (Strokes) ------------ */

	WM_operatortype_append(GPENCIL_OT_editmode_toggle);
	WM_operatortype_append(GPENCIL_OT_paintmode_toggle);
	WM_operatortype_append(GPENCIL_OT_sculptmode_toggle);
	WM_operatortype_append(GPENCIL_OT_weightmode_toggle);
	WM_operatortype_append(GPENCIL_OT_selection_opacity_toggle);

	WM_operatortype_append(GPENCIL_OT_select);
	WM_operatortype_append(GPENCIL_OT_select_all);
	WM_operatortype_append(GPENCIL_OT_select_circle);
	WM_operatortype_append(GPENCIL_OT_select_border);
	WM_operatortype_append(GPENCIL_OT_select_lasso);

	WM_operatortype_append(GPENCIL_OT_select_linked);
	WM_operatortype_append(GPENCIL_OT_select_grouped);
	WM_operatortype_append(GPENCIL_OT_select_more);
	WM_operatortype_append(GPENCIL_OT_select_less);
	WM_operatortype_append(GPENCIL_OT_select_first);
	WM_operatortype_append(GPENCIL_OT_select_last);
	WM_operatortype_append(GPENCIL_OT_select_alternate);

	WM_operatortype_append(GPENCIL_OT_duplicate);
	WM_operatortype_append(GPENCIL_OT_delete);
	WM_operatortype_append(GPENCIL_OT_dissolve);
	WM_operatortype_append(GPENCIL_OT_copy);
	WM_operatortype_append(GPENCIL_OT_paste);

	WM_operatortype_append(GPENCIL_OT_move_to_layer);
	WM_operatortype_append(GPENCIL_OT_layer_change);

	WM_operatortype_append(GPENCIL_OT_snap_to_grid);
	WM_operatortype_append(GPENCIL_OT_snap_to_cursor);
	WM_operatortype_append(GPENCIL_OT_snap_cursor_to_selected);

	WM_operatortype_append(GPENCIL_OT_reproject);

	WM_operatortype_append(GPENCIL_OT_brush_paint);

	/* Editing (Buttons) ------------ */

	WM_operatortype_append(GPENCIL_OT_data_add);
	WM_operatortype_append(GPENCIL_OT_data_unlink);

	WM_operatortype_append(GPENCIL_OT_layer_add);
	WM_operatortype_append(GPENCIL_OT_layer_remove);
	WM_operatortype_append(GPENCIL_OT_layer_move);
	WM_operatortype_append(GPENCIL_OT_layer_duplicate);

	WM_operatortype_append(GPENCIL_OT_hide);
	WM_operatortype_append(GPENCIL_OT_reveal);
	WM_operatortype_append(GPENCIL_OT_lock_all);
	WM_operatortype_append(GPENCIL_OT_unlock_all);
	WM_operatortype_append(GPENCIL_OT_layer_isolate);
	WM_operatortype_append(GPENCIL_OT_layer_merge);

	WM_operatortype_append(GPENCIL_OT_blank_frame_add);

	WM_operatortype_append(GPENCIL_OT_active_frame_delete);
	WM_operatortype_append(GPENCIL_OT_active_frames_delete_all);
	WM_operatortype_append(GPENCIL_OT_frame_duplicate);
	WM_operatortype_append(GPENCIL_OT_frame_clean_fill);

	WM_operatortype_append(GPENCIL_OT_convert);

	WM_operatortype_append(GPENCIL_OT_stroke_arrange);
	WM_operatortype_append(GPENCIL_OT_stroke_change_color);
	WM_operatortype_append(GPENCIL_OT_stroke_lock_color);
	WM_operatortype_append(GPENCIL_OT_stroke_apply_thickness);
	WM_operatortype_append(GPENCIL_OT_stroke_cyclical_set);
	WM_operatortype_append(GPENCIL_OT_stroke_join);
	WM_operatortype_append(GPENCIL_OT_stroke_flip);
	WM_operatortype_append(GPENCIL_OT_stroke_subdivide);
	WM_operatortype_append(GPENCIL_OT_stroke_simplify);
	WM_operatortype_append(GPENCIL_OT_stroke_simplify_fixed);
	WM_operatortype_append(GPENCIL_OT_stroke_separate);
	WM_operatortype_append(GPENCIL_OT_stroke_split);

	WM_operatortype_append(GPENCIL_OT_brush_presets_create);
	WM_operatortype_append(GPENCIL_OT_brush_select);

	WM_operatortype_append(GPENCIL_OT_sculpt_select);

	/* vertex groups */
	WM_operatortype_append(GPENCIL_OT_vertex_group_assign);
	WM_operatortype_append(GPENCIL_OT_vertex_group_remove_from);
	WM_operatortype_append(GPENCIL_OT_vertex_group_select);
	WM_operatortype_append(GPENCIL_OT_vertex_group_deselect);
	WM_operatortype_append(GPENCIL_OT_vertex_group_invert);
	WM_operatortype_append(GPENCIL_OT_vertex_group_smooth);

	/* color handle */
	WM_operatortype_append(GPENCIL_OT_lock_layer);
	WM_operatortype_append(GPENCIL_OT_color_isolate);
	WM_operatortype_append(GPENCIL_OT_color_hide);
	WM_operatortype_append(GPENCIL_OT_color_reveal);
	WM_operatortype_append(GPENCIL_OT_color_lock_all);
	WM_operatortype_append(GPENCIL_OT_color_unlock_all);
	WM_operatortype_append(GPENCIL_OT_color_select);

	/* Editing (Time) --------------- */

	/* Interpolation */
	WM_operatortype_append(GPENCIL_OT_interpolate);
	WM_operatortype_append(GPENCIL_OT_interpolate_sequence);
	WM_operatortype_append(GPENCIL_OT_interpolate_reverse);

	/* Primitives */
	WM_operatortype_append(GPENCIL_OT_primitive);

	/* convert old 2.7 files to 2.8 */
	WM_operatortype_append(GPENCIL_OT_convert_old_files);

}

void ED_operatormacros_gpencil(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;

	/* Duplicate + Move = Interactively place newly duplicated strokes */
	ot = WM_operatortype_append_macro("GPENCIL_OT_duplicate_move", "Duplicate Strokes",
	                                  "Make copies of the selected Grease Pencil strokes and move them",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "GPENCIL_OT_duplicate");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_boolean_set(otmacro->ptr, "gpencil_strokes", true);

}

/* ****************************************** */
