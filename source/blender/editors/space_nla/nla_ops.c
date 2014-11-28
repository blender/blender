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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_nla/nla_ops.c
 *  \ingroup spnla
 */

#include <string.h>
#include <stdio.h>

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"


#include "nla_intern.h" /* own include */

/* ************************** poll callbacks for operators **********************************/

/* tweakmode is NOT enabled */
int nlaop_poll_tweakmode_off(bContext *C)
{
	Scene *scene;
	
	/* for now, we check 2 things: 
	 *  1) active editor must be NLA
	 *	2) tweakmode is currently set as a 'per-scene' flag 
	 *	   so that it will affect entire NLA data-sets,
	 *	   but not all AnimData blocks will be in tweakmode for 
	 *	   various reasons
	 */
	if (ED_operator_nla_active(C) == 0)
		return 0;
	
	scene = CTX_data_scene(C);
	if ((scene == NULL) || (scene->flag & SCE_NLA_EDIT_ON))
		return 0;
	
	return 1;
}

/* tweakmode IS enabled */
int nlaop_poll_tweakmode_on(bContext *C)
{
	Scene *scene;
	
	/* for now, we check 2 things: 
	 *  1) active editor must be NLA
	 *	2) tweakmode is currently set as a 'per-scene' flag 
	 *	   so that it will affect entire NLA data-sets,
	 *	   but not all AnimData blocks will be in tweakmode for 
	 *	   various reasons
	 */
	if (ED_operator_nla_active(C) == 0)
		return 0;
	
	scene = CTX_data_scene(C);
	if ((scene == NULL) || !(scene->flag & SCE_NLA_EDIT_ON))
		return 0;
	
	return 1;
}

/* is tweakmode enabled - for use in NLA operator code */
bool nlaedit_is_tweakmode_on(bAnimContext *ac)
{
	if (ac && ac->scene)
		return (ac->scene->flag & SCE_NLA_EDIT_ON) != 0;
	return 0;
}

/* ************************** registration - operator types **********************************/

void nla_operatortypes(void)
{
	/* view */
	WM_operatortype_append(NLA_OT_properties);
	
	/* channels */
	WM_operatortype_append(NLA_OT_channels_click);
	
	WM_operatortype_append(NLA_OT_action_pushdown);
	
	WM_operatortype_append(NLA_OT_tracks_add);
	WM_operatortype_append(NLA_OT_tracks_delete);
	
	WM_operatortype_append(NLA_OT_selected_objects_add);
	
	/* select */
	WM_operatortype_append(NLA_OT_click_select);
	WM_operatortype_append(NLA_OT_select_border);
	WM_operatortype_append(NLA_OT_select_all_toggle);
	WM_operatortype_append(NLA_OT_select_leftright);
	
	/* view */
	WM_operatortype_append(NLA_OT_view_all);
	WM_operatortype_append(NLA_OT_view_selected);
	
	WM_operatortype_append(NLA_OT_previewrange_set);
	
	/* edit */
	WM_operatortype_append(NLA_OT_tweakmode_enter);
	WM_operatortype_append(NLA_OT_tweakmode_exit);
	
	WM_operatortype_append(NLA_OT_actionclip_add);
	WM_operatortype_append(NLA_OT_transition_add);
	WM_operatortype_append(NLA_OT_soundclip_add);
	
	WM_operatortype_append(NLA_OT_meta_add);
	WM_operatortype_append(NLA_OT_meta_remove);
	
	WM_operatortype_append(NLA_OT_duplicate);
	WM_operatortype_append(NLA_OT_delete);
	WM_operatortype_append(NLA_OT_split);
	
	WM_operatortype_append(NLA_OT_mute_toggle);
	
	WM_operatortype_append(NLA_OT_swap);
	WM_operatortype_append(NLA_OT_move_up);
	WM_operatortype_append(NLA_OT_move_down);
	
	WM_operatortype_append(NLA_OT_action_sync_length);
	
	WM_operatortype_append(NLA_OT_make_single_user);
	
	WM_operatortype_append(NLA_OT_apply_scale);
	WM_operatortype_append(NLA_OT_clear_scale);
	
	WM_operatortype_append(NLA_OT_snap);
	
	WM_operatortype_append(NLA_OT_fmodifier_add);
	WM_operatortype_append(NLA_OT_fmodifier_copy);
	WM_operatortype_append(NLA_OT_fmodifier_paste);
}

/* ************************** registration - keymaps **********************************/

static void nla_keymap_channels(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;

	/* keymappings here are NLA-specific (different to standard channels keymap) */
	
	/* selection --------------------------------------------------------------------- */
	/* click-select */
	// XXX for now, only leftmouse....
	kmi = WM_keymap_add_item(keymap, "NLA_OT_channels_click", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_channels_click", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	
	/* channel operations ------------------------------------------------------------ */
	/* add tracks */
	kmi = WM_keymap_add_item(keymap, "NLA_OT_tracks_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "above_selected", false);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_tracks_add", AKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "above_selected", true);
	
	/* delete tracks */
	WM_keymap_add_item(keymap, "NLA_OT_tracks_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NLA_OT_tracks_delete", DELKEY, KM_PRESS, 0, 0);
}

static void nla_keymap_main(wmKeyConfig *keyconf, wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;
	
	/* selection ------------------------------------------------ */
	/* click select */
	kmi = WM_keymap_add_item(keymap, "NLA_OT_click_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_click_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
		
	/* select left/right */
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_leftright", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", NLAEDIT_LRSEL_TEST);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_leftright", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_enum_set(kmi->ptr, "mode", NLAEDIT_LRSEL_TEST);
	
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_leftright", LEFTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", NLAEDIT_LRSEL_LEFT);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_leftright", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", NLAEDIT_LRSEL_RIGHT);
		
	
	/* deselect all */
	/* TODO: uniformize with other select_all ops? */
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "invert", false);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "invert", true);
	
	/* borderselect */
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", false);
	kmi = WM_keymap_add_item(keymap, "NLA_OT_select_border", BKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", true);
	
	/* view ---------------------------------------------------- */
	/* auto-set range */
	WM_keymap_add_item(keymap, "NLA_OT_previewrange_set", PKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "NLA_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NLA_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NLA_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	
	/* editing ------------------------------------------------ */
	
	/* add strips */
	WM_keymap_add_item(keymap, "NLA_OT_actionclip_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NLA_OT_transition_add", TKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NLA_OT_soundclip_add", KKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* meta-strips */
	WM_keymap_add_item(keymap, "NLA_OT_meta_add", GKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NLA_OT_meta_remove", GKEY, KM_PRESS, KM_ALT, 0);
		
	/* duplicate */
	kmi = WM_keymap_add_item(keymap, "NLA_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "linked", false);
	
	kmi = WM_keymap_add_item(keymap, "NLA_OT_duplicate", DKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "linked", true);
	
	/* single user */
	WM_keymap_add_item(keymap, "NLA_OT_make_single_user", UKEY, KM_PRESS, 0, 0);
		
	/* delete */
	WM_keymap_add_item(keymap, "NLA_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NLA_OT_delete", DELKEY, KM_PRESS, 0, 0);

	/* split */
	WM_keymap_add_item(keymap, "NLA_OT_split", YKEY, KM_PRESS, 0, 0);
	
	/* toggles */
	WM_keymap_add_item(keymap, "NLA_OT_mute_toggle", HKEY, KM_PRESS, 0, 0);
	
	/* swap */
	WM_keymap_add_item(keymap, "NLA_OT_swap", FKEY, KM_PRESS, KM_ALT, 0);
		
	/* move up */
	WM_keymap_add_item(keymap, "NLA_OT_move_up", PAGEUPKEY, KM_PRESS, 0, 0);
	/* move down */
	WM_keymap_add_item(keymap, "NLA_OT_move_down", PAGEDOWNKEY, KM_PRESS, 0, 0);
	
	/* apply scale */
	WM_keymap_add_item(keymap, "NLA_OT_apply_scale", AKEY, KM_PRESS, KM_CTRL, 0);
	/* clear scale */
	WM_keymap_add_item(keymap, "NLA_OT_clear_scale", SKEY, KM_PRESS, KM_ALT, 0);
	
	/* snap */
	WM_keymap_add_item(keymap, "NLA_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* add f-modifier */
	WM_keymap_add_item(keymap, "NLA_OT_fmodifier_add", MKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	
	/* transform system */
	transform_keymap_for_space(keyconf, keymap, SPACE_NLA);
	
	/* special markers hotkeys for anim editors: see note in definition of this function */
	ED_marker_keymap_animedit_conflictfree(keymap);
}

/* --------------- */

void nla_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	
	/* keymap for all regions ------------------------------------------- */
	keymap = WM_keymap_find(keyconf, "NLA Generic", SPACE_NLA, 0);
	
	/* region management */
	WM_keymap_add_item(keymap, "NLA_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	/* tweakmode
	 *	- enter and exit are separate operators with the same hotkey...
	 *	  This works as they use different poll()'s
	 */
	WM_keymap_add_item(keymap, "NLA_OT_tweakmode_enter", TABKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NLA_OT_tweakmode_exit", TABKEY, KM_PRESS, 0, 0);
	
	/* find (i.e. a shortcut for setting the name filter) */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_find", FKEY, KM_PRESS, KM_CTRL, 0);
	
	/* channels ---------------------------------------------------------- */
	/* Channels are not directly handled by the NLA Editor module, but are inherited from the Animation module. 
	 * Most of the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as there
	 * are many similarities with the other Animation Editors.
	 *
	 * However, those operations which involve clicking on channels and/or the placement of them in the view are implemented here instead
	 */
	keymap = WM_keymap_find(keyconf, "NLA Channels", SPACE_NLA, 0);
	nla_keymap_channels(keymap);
	
	/* data ------------------------------------------------------------- */
	keymap = WM_keymap_find(keyconf, "NLA Editor", SPACE_NLA, 0);
	nla_keymap_main(keyconf, keymap);
}
