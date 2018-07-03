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

#include "DNA_gpencil_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_transform.h"

#include "gpencil_intern.h"

/* ****************************************** */
/* Grease Pencil Keymaps */

/* Generic Drawing Keymap */
static void ed_keymap_gpencil_general(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil", 0, 0);
	wmKeyMapItem *kmi;

	/* Draw  --------------------------------------- */
	/* draw */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, 0, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* draw - straight lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_CTRL, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_STRAIGHT);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* draw - poly lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", RIGHTMOUSE, KM_PRESS, KM_CTRL, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_POLY);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	/* erase */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", RIGHTMOUSE, KM_PRESS, 0, DKEY);
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

/* Stroke Editing Keymap - Only when editmode is enabled */
static void ed_keymap_gpencil_editing(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil Stroke Edit Mode", 0, 0);
	wmKeyMapItem *kmi;

	/* set poll callback - so that this keymap only gets enabled when stroke editmode is enabled */
	keymap->poll = gp_stroke_editmode_poll;

	/* ----------------------------------------------- */

	/* Exit EditMode */
	WM_keymap_add_item(keymap, "GPENCIL_OT_editmode_toggle", TABKEY, KM_PRESS, 0, 0);

	/* Pie Menu - For settings/tools easy access */
	WM_keymap_add_menu_pie(keymap, "GPENCIL_MT_pie_sculpt", EKEY, KM_PRESS, 0, DKEY);

	/* Brush Settings */
	/* NOTE: We cannot expose these in the standard keymap, as they will interfere with regular hotkeys
	 *       in other modes. However, when we are dealing with Stroke Edit Mode, we know for certain
	 *       that the only data being edited is that of the Grease Pencil strokes
	 */

	/* CTRL + FKEY = Eraser Radius */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "user_preferences.edit.grease_pencil_eraser_radius");

	/* Interpolation */
	WM_keymap_add_item(keymap, "GPENCIL_OT_interpolate", EKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_interpolate_sequence", EKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);

	/* Sculpting ------------------------------------- */

	/* Brush-Based Editing:
	 *   EKEY + LMB                          = Single stroke, draw immediately
	 *        + Other Modifiers (Ctrl/Shift) = Invert, Smooth, etc.
	 *
	 * For the modal version, use D+E -> Sculpt
	 */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, 0, EKEY);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, KM_CTRL, EKEY);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	/*RNA_boolean_set(kmi->ptr, "use_invert", true);*/

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_brush_paint", LEFTMOUSE, KM_PRESS, KM_SHIFT, EKEY);
	RNA_boolean_set(kmi->ptr, "wait_for_input", false);
	/*RNA_boolean_set(kmi->ptr, "use_smooth", true);*/


	/* Shift-FKEY = Sculpt Strength */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_sculpt.brush.strength");

	/* FKEY = Sculpt Brush Size */
	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path_primary", "tool_settings.gpencil_sculpt.brush.size");


	/* Selection ------------------------------------- */
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

	/* normal select */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	/* whole stroke select */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "entire_strokes", true);

	/* select linked */
	/* NOTE: While LKEY is redundant, not having it breaks the mode illusion too much */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);

	/* select grouped */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_grouped", GKEY, KM_PRESS, KM_SHIFT, 0);

	/* select more/less */
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GPENCIL_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);

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
}

/* ==================== */

void ED_keymap_gpencil(wmKeyConfig *keyconf)
{
	ed_keymap_gpencil_general(keyconf);
	ed_keymap_gpencil_editing(keyconf);
}

/* ****************************************** */

void ED_operatortypes_gpencil(void)
{
	/* Drawing ----------------------- */

	WM_operatortype_append(GPENCIL_OT_draw);

	/* Editing (Strokes) ------------ */

	WM_operatortype_append(GPENCIL_OT_editmode_toggle);
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

	WM_operatortype_append(GPENCIL_OT_convert);

	WM_operatortype_append(GPENCIL_OT_stroke_arrange);
	WM_operatortype_append(GPENCIL_OT_stroke_change_color);
	WM_operatortype_append(GPENCIL_OT_stroke_lock_color);
	WM_operatortype_append(GPENCIL_OT_stroke_apply_thickness);
	WM_operatortype_append(GPENCIL_OT_stroke_cyclical_set);
	WM_operatortype_append(GPENCIL_OT_stroke_join);
	WM_operatortype_append(GPENCIL_OT_stroke_flip);
	WM_operatortype_append(GPENCIL_OT_stroke_subdivide);

	WM_operatortype_append(GPENCIL_OT_palette_add);
	WM_operatortype_append(GPENCIL_OT_palette_remove);
	WM_operatortype_append(GPENCIL_OT_palette_change);
	WM_operatortype_append(GPENCIL_OT_palette_lock_layer);
	WM_operatortype_append(GPENCIL_OT_palettecolor_add);
	WM_operatortype_append(GPENCIL_OT_palettecolor_remove);
	WM_operatortype_append(GPENCIL_OT_palettecolor_isolate);
	WM_operatortype_append(GPENCIL_OT_palettecolor_hide);
	WM_operatortype_append(GPENCIL_OT_palettecolor_reveal);
	WM_operatortype_append(GPENCIL_OT_palettecolor_lock_all);
	WM_operatortype_append(GPENCIL_OT_palettecolor_unlock_all);
	WM_operatortype_append(GPENCIL_OT_palettecolor_move);
	WM_operatortype_append(GPENCIL_OT_palettecolor_select);
	WM_operatortype_append(GPENCIL_OT_palettecolor_copy);

	WM_operatortype_append(GPENCIL_OT_brush_add);
	WM_operatortype_append(GPENCIL_OT_brush_remove);
	WM_operatortype_append(GPENCIL_OT_brush_change);
	WM_operatortype_append(GPENCIL_OT_brush_move);
	WM_operatortype_append(GPENCIL_OT_brush_presets_create);
	WM_operatortype_append(GPENCIL_OT_brush_copy);
	WM_operatortype_append(GPENCIL_OT_brush_select);

	/* Editing (Time) --------------- */

	/* Interpolation */
	WM_operatortype_append(GPENCIL_OT_interpolate);
	WM_operatortype_append(GPENCIL_OT_interpolate_sequence);
	WM_operatortype_append(GPENCIL_OT_interpolate_reverse);
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
