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

/** \file blender/editors/space_sequencer/sequencer_ops.c
 *  \ingroup spseq
 */


#include <stdlib.h>
#include <math.h>


#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"



#include "WM_api.h"
#include "WM_types.h"

#include "ED_sequencer.h"
#include "ED_markers.h"
#include "ED_transform.h" /* transform keymap */

#include "sequencer_intern.h"


/* ************************** registration **********************************/


void sequencer_operatortypes(void)
{
	/* sequencer_edit.c */
	WM_operatortype_append(SEQUENCER_OT_cut);
	WM_operatortype_append(SEQUENCER_OT_mute);
	WM_operatortype_append(SEQUENCER_OT_unmute);
	WM_operatortype_append(SEQUENCER_OT_lock);
	WM_operatortype_append(SEQUENCER_OT_unlock);
	WM_operatortype_append(SEQUENCER_OT_reload);
	WM_operatortype_append(SEQUENCER_OT_refresh_all);
	WM_operatortype_append(SEQUENCER_OT_reassign_inputs);
	WM_operatortype_append(SEQUENCER_OT_swap_inputs);
	WM_operatortype_append(SEQUENCER_OT_duplicate);
	WM_operatortype_append(SEQUENCER_OT_delete);
	WM_operatortype_append(SEQUENCER_OT_offset_clear);
	WM_operatortype_append(SEQUENCER_OT_images_separate);
	WM_operatortype_append(SEQUENCER_OT_meta_toggle);
	WM_operatortype_append(SEQUENCER_OT_meta_make);
	WM_operatortype_append(SEQUENCER_OT_meta_separate);
	
	WM_operatortype_append(SEQUENCER_OT_gap_remove);
	WM_operatortype_append(SEQUENCER_OT_gap_insert);
	WM_operatortype_append(SEQUENCER_OT_snap);
	WM_operatortype_append(SEQUENCER_OT_strip_jump);
	WM_operatortype_append(SEQUENCER_OT_swap);
	WM_operatortype_append(SEQUENCER_OT_swap_data);
	WM_operatortype_append(SEQUENCER_OT_rendersize);

	WM_operatortype_append(SEQUENCER_OT_copy);
	WM_operatortype_append(SEQUENCER_OT_paste);

	WM_operatortype_append(SEQUENCER_OT_view_all);
	WM_operatortype_append(SEQUENCER_OT_view_selected);
	WM_operatortype_append(SEQUENCER_OT_view_all_preview);
	WM_operatortype_append(SEQUENCER_OT_view_toggle);
	WM_operatortype_append(SEQUENCER_OT_view_zoom_ratio);
	WM_operatortype_append(SEQUENCER_OT_view_ghost_border);

	WM_operatortype_append(SEQUENCER_OT_rebuild_proxy);
	WM_operatortype_append(SEQUENCER_OT_change_effect_input);
	WM_operatortype_append(SEQUENCER_OT_change_effect_type);
	WM_operatortype_append(SEQUENCER_OT_change_path);

	/* sequencer_select.c */
	WM_operatortype_append(SEQUENCER_OT_select_all);
	WM_operatortype_append(SEQUENCER_OT_select);
	WM_operatortype_append(SEQUENCER_OT_select_more);
	WM_operatortype_append(SEQUENCER_OT_select_less);
	WM_operatortype_append(SEQUENCER_OT_select_linked_pick);
	WM_operatortype_append(SEQUENCER_OT_select_linked);
	WM_operatortype_append(SEQUENCER_OT_select_handles);
	WM_operatortype_append(SEQUENCER_OT_select_active_side);
	WM_operatortype_append(SEQUENCER_OT_select_border);
	WM_operatortype_append(SEQUENCER_OT_select_grouped);

	/* sequencer_add.c */
	WM_operatortype_append(SEQUENCER_OT_scene_strip_add);
	WM_operatortype_append(SEQUENCER_OT_movieclip_strip_add);
	WM_operatortype_append(SEQUENCER_OT_mask_strip_add);
	WM_operatortype_append(SEQUENCER_OT_movie_strip_add);
	WM_operatortype_append(SEQUENCER_OT_sound_strip_add);
	WM_operatortype_append(SEQUENCER_OT_image_strip_add);
	WM_operatortype_append(SEQUENCER_OT_effect_strip_add);

	/* sequencer_buttons.c */
	WM_operatortype_append(SEQUENCER_OT_properties);

	/* sequencer_modifiers.c */
	WM_operatortype_append(SEQUENCER_OT_strip_modifier_add);
	WM_operatortype_append(SEQUENCER_OT_strip_modifier_remove);
	WM_operatortype_append(SEQUENCER_OT_strip_modifier_move);

	/* sequencer_view.h */
	WM_operatortype_append(SEQUENCER_OT_sample);
}


void sequencer_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* Common items ------------------------------------------------------------------ */
	keymap = WM_keymap_find(keyconf, "SequencerCommon", SPACE_SEQ, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_properties", NKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", OKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "scene.sequence_editor.show_overlay");

	/* operators common to sequence and preview view */
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_toggle", TABKEY, KM_PRESS, KM_CTRL, 0);

	/* Strips Region --------------------------------------------------------------- */
	keymap = WM_keymap_find(keyconf, "Sequencer", SPACE_SEQ, 0);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_cut", KKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "type", SEQ_CUT_SOFT);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_cut", KKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", SEQ_CUT_HARD);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_mute", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_mute", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_unmute", HKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_unmute", HKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_lock", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_unlock", LKEY, KM_PRESS, KM_SHIFT | KM_ALT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_reassign_inputs", RKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_reload", RKEY, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_reload", RKEY, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "adjust_length", TRUE);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_offset_clear", OKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "SEQUENCER_OT_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif

	WM_keymap_add_item(keymap, "SEQUENCER_OT_images_separate", YKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_meta_toggle", TABKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_meta_make", GKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_meta_separate", GKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_strip_jump", PAGEUPKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "next", TRUE);
	RNA_boolean_set(kmi->ptr, "center", FALSE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_strip_jump", PAGEDOWNKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "next", FALSE);
	RNA_boolean_set(kmi->ptr, "center", FALSE);

	/* alt for center */
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_strip_jump", PAGEUPKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "next", TRUE);
	RNA_boolean_set(kmi->ptr, "center", TRUE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_strip_jump", PAGEDOWNKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "next", FALSE);
	RNA_boolean_set(kmi->ptr, "center", TRUE);

	RNA_enum_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_swap", LEFTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "side", SEQ_SIDE_LEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_swap", RIGHTARROWKEY, KM_PRESS, KM_ALT, 0)->ptr, "side", SEQ_SIDE_RIGHT);

	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_gap_remove", BACKSPACEKEY, KM_PRESS, 0, 0)->ptr, "all", FALSE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_gap_remove", BACKSPACEKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "all", TRUE);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_gap_insert", EQUALKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "SEQUENCER_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_swap_inputs", SKEY, KM_PRESS, KM_ALT, 0);

	/* multicam editing keyboard layout, switch to camera 1-10 using
	 * regular number keys */
	{
		int keys[] = { ONEKEY, TWOKEY, THREEKEY, FOURKEY, FIVEKEY,
			           SIXKEY, SEVENKEY, EIGHTKEY, NINEKEY, ZEROKEY };
		int i;

		for (i = 1; i <= 10; i++) {
			RNA_int_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_cut_multicam", keys[i - 1], KM_PRESS, 0, 0)->ptr, "camera", i);
		}
	}

	/* Mouse selection, a bit verbose :/ */
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_handle", FALSE);
	RNA_boolean_set(kmi->ptr, "left_right", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_time", FALSE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_handle", FALSE);
	RNA_boolean_set(kmi->ptr, "left_right", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_time", FALSE);


	/* 2.4x method, now use Alt for handles and select the side based on which handle was selected */
#if 0
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "linked_left", TRUE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "linked_right", TRUE);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "linked_left", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_right", TRUE);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_left", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_right", TRUE);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_left", TRUE);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_right", TRUE);
#endif

	/* 2.5 method, Alt and use selected handle */
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_handle", TRUE);
	RNA_boolean_set(kmi->ptr, "left_right", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_time", FALSE);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_handle", TRUE);
	RNA_boolean_set(kmi->ptr, "left_right", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_time", FALSE);

	/* match action editor */
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_handle", FALSE);
	RNA_boolean_set(kmi->ptr, "left_right", TRUE);     /* grr, these conflict - only use left_right if not over an active seq */
	RNA_boolean_set(kmi->ptr, "linked_time", TRUE);
	/* adjusted since 2.4 */

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "linked_handle", FALSE);
	RNA_boolean_set(kmi->ptr, "left_right", FALSE);
	RNA_boolean_set(kmi->ptr, "linked_time", TRUE);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "SEQUENCER_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_border", BKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_select_grouped", GKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_menu(keymap, "SEQUENCER_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_menu(keymap, "SEQUENCER_MT_change", CKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_int", OKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "scene.sequence_editor.overlay_frame");
	RNA_int_set(kmi->ptr, "value", 0);

	transform_keymap_for_space(keyconf, keymap, SPACE_SEQ);

	/* special markers hotkeys for anim editors: see note in definition of this function */
	ED_marker_keymap_animedit_conflictfree(keymap);


	/* Preview Region ----------------------------------------------------------- */
	keymap = WM_keymap_find(keyconf, "SequencerPreview", SPACE_SEQ, 0);
	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_all_preview", HOMEKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "SEQUENCER_OT_view_ghost_border", OKEY, KM_PRESS, 0, 0);

	/* would prefer to use numpad keys for job */
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);

	/* Setting zoom levels is not that useful, except for back to zoom level 1, removing keymap because of conflicts for now */
#if 0
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);

	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "SEQUENCER_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);
#endif

	/* sample */
	WM_keymap_add_item(keymap, "SEQUENCER_OT_sample", ACTIONMOUSE, KM_PRESS, 0, 0);
}

void ED_operatormacros_sequencer(void)
{
	wmOperatorType *ot;

	ot = WM_operatortype_append_macro("SEQUENCER_OT_duplicate_move", "Duplicate Strips",
	                                  "Duplicate selected strips and move them", OPTYPE_UNDO | OPTYPE_REGISTER);

	WM_operatortype_macro_define(ot, "SEQUENCER_OT_duplicate");
	WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
}
