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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_action/action_ops.c
 *  \ingroup spaction
 */


#include <stdlib.h>
#include <math.h>


#include "DNA_space_types.h"

#include "BLI_utildefines.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_transform.h"

#include "action_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************** registration - operator types **********************************/

void action_operatortypes(void)
{
	/* keyframes */
	/* selection */
	WM_operatortype_append(ACTION_OT_clickselect);
	WM_operatortype_append(ACTION_OT_select_all_toggle);
	WM_operatortype_append(ACTION_OT_select_border);
	WM_operatortype_append(ACTION_OT_select_column);
	WM_operatortype_append(ACTION_OT_select_linked);
	WM_operatortype_append(ACTION_OT_select_more);
	WM_operatortype_append(ACTION_OT_select_less);
	WM_operatortype_append(ACTION_OT_select_leftright);
	
	/* editing */
	WM_operatortype_append(ACTION_OT_snap);
	WM_operatortype_append(ACTION_OT_mirror);
	WM_operatortype_append(ACTION_OT_frame_jump);
	WM_operatortype_append(ACTION_OT_handle_type);
	WM_operatortype_append(ACTION_OT_interpolation_type);
	WM_operatortype_append(ACTION_OT_extrapolation_type);
	WM_operatortype_append(ACTION_OT_keyframe_type);
	WM_operatortype_append(ACTION_OT_sample);
	WM_operatortype_append(ACTION_OT_clean);
	WM_operatortype_append(ACTION_OT_delete);
	WM_operatortype_append(ACTION_OT_duplicate);
	WM_operatortype_append(ACTION_OT_keyframe_insert);
	WM_operatortype_append(ACTION_OT_copy);
	WM_operatortype_append(ACTION_OT_paste);
	WM_operatortype_append(ACTION_OT_new);
	
	WM_operatortype_append(ACTION_OT_previewrange_set);
	WM_operatortype_append(ACTION_OT_view_all);
	WM_operatortype_append(ACTION_OT_view_selected);
	
	WM_operatortype_append(ACTION_OT_markers_make_local);
}

void ED_operatormacros_action(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;
	
	ot = WM_operatortype_append_macro("ACTION_OT_duplicate_move", "Duplicate",
	                                  "Make a copy of all selected keyframes and move them",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "ACTION_OT_duplicate");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_transform");
	RNA_enum_set(otmacro->ptr, "mode", TFM_TIME_DUPLICATE);
}

/* ************************** registration - keymaps **********************************/

static void action_keymap_keyframes(wmKeyConfig *keyconf, wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;
	
	/* action_select.c - selection tools */
	/* click-select: keyframe (replace) */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "column", false);
	RNA_boolean_set(kmi->ptr, "channel", false);
	/* click-select: all on same frame (replace) */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "column", true);
	RNA_boolean_set(kmi->ptr, "channel", false);
	/* click-select: keyframe (add) */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "column", false);
	RNA_boolean_set(kmi->ptr, "channel", false);
	/* click-select: all on same frame (add) */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "column", true);
	RNA_boolean_set(kmi->ptr, "channel", false);
	/* click-select: all on same channel (replace) */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "column", false);
	RNA_boolean_set(kmi->ptr, "channel", true);
	/* click-select: all on same channel (add) */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_ALT | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "column", false);
	RNA_boolean_set(kmi->ptr, "channel", true);
		
	/* click-select: left/right */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_leftright", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", ACTKEYS_LRSEL_TEST);
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_leftright", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_enum_set(kmi->ptr, "mode", ACTKEYS_LRSEL_TEST);
	
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_leftright", LEFTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", ACTKEYS_LRSEL_LEFT);
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_leftright", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", ACTKEYS_LRSEL_RIGHT);
	
	/* deselect all */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "invert", false);
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "invert", true);
	
	/* borderselect */
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", false);
	kmi = WM_keymap_add_item(keymap, "ACTION_OT_select_border", BKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", true);
	
	/* column select */
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, 0, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_KEYS);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, KM_CTRL, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_CFRA);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_MARKERS_COLUMN);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_MARKERS_BETWEEN);
	
	/* select more/less */
	WM_keymap_add_item(keymap, "ACTION_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	
	/* select linked */
	WM_keymap_add_item(keymap, "ACTION_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	
	
	/* action_edit.c */
	/* jump to selected keyframes */
	WM_keymap_add_item(keymap, "ACTION_OT_frame_jump", GKEY, KM_PRESS, KM_CTRL, 0);
		
	/* menu + single-step transform */
	WM_keymap_add_item(keymap, "ACTION_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_mirror", MKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* menu + set setting */
	WM_keymap_add_item(keymap, "ACTION_OT_handle_type", VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_interpolation_type", TKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_extrapolation_type", EKEY, KM_PRESS, KM_SHIFT, 0); 
	WM_keymap_add_item(keymap, "ACTION_OT_keyframe_type", RKEY, KM_PRESS, 0, 0); 
	
	/* destructive */
	WM_keymap_add_item(keymap, "ACTION_OT_clean", OKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_sample", OKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "ACTION_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "ACTION_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_keyframe_insert", IKEY, KM_PRESS, 0, 0);
	
	/* copy/paste */
	WM_keymap_add_item(keymap, "ACTION_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "ACTION_OT_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif

	/* auto-set range */
	WM_keymap_add_item(keymap, "ACTION_OT_previewrange_set", PKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	
	/* animation module */
	/* channels list
	 * NOTE: these operators were originally for the channels list, but are added here too for convenience...
	 */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_editable_toggle", TABKEY, KM_PRESS, 0, 0);
	
	/* transform system */
	transform_keymap_for_space(keyconf, keymap, SPACE_ACTION);
	
	/* special markers hotkeys for anim editors: see note in definition of this function */
	ED_marker_keymap_animedit_conflictfree(keymap);
}

/* --------------- */

void action_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	
	/* channels */
	/* Channels are not directly handled by the Action Editor module, but are inherited from the Animation module. 
	 * All the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as these
	 * are all used for the Graph-Editor too.
	 */
	
	/* keyframes */
	keymap = WM_keymap_find(keyconf, "Dopesheet", SPACE_ACTION, 0);
	action_keymap_keyframes(keyconf, keymap);
}

