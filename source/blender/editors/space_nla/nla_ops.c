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
#include "ED_select_utils.h"
#include "ED_transform.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"


#include "nla_intern.h" /* own include */

/* ************************** poll callbacks for operators **********************************/

/* tweakmode is NOT enabled */
bool nlaop_poll_tweakmode_off(bContext *C)
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
bool nlaop_poll_tweakmode_on(bContext *C)
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
	WM_operatortype_append(NLA_OT_action_unlink);

	WM_operatortype_append(NLA_OT_tracks_add);
	WM_operatortype_append(NLA_OT_tracks_delete);

	WM_operatortype_append(NLA_OT_selected_objects_add);

	/* select */
	WM_operatortype_append(NLA_OT_click_select);
	WM_operatortype_append(NLA_OT_select_box);
	WM_operatortype_append(NLA_OT_select_all);
	WM_operatortype_append(NLA_OT_select_leftright);

	/* view */
	WM_operatortype_append(NLA_OT_view_all);
	WM_operatortype_append(NLA_OT_view_selected);
	WM_operatortype_append(NLA_OT_view_frame);

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

void nla_keymap(wmKeyConfig *keyconf)
{
	/* keymap for all regions ------------------------------------------- */
	WM_keymap_ensure(keyconf, "NLA Generic", SPACE_NLA, 0);

	/* channels ---------------------------------------------------------- */
	/* Channels are not directly handled by the NLA Editor module, but are inherited from the Animation module.
	 * Most of the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as there
	 * are many similarities with the other Animation Editors.
	 *
	 * However, those operations which involve clicking on channels and/or the placement of them in the view are implemented here instead
	 */
	WM_keymap_ensure(keyconf, "NLA Channels", SPACE_NLA, 0);

	/* data ------------------------------------------------------------- */
	WM_keymap_ensure(keyconf, "NLA Editor", SPACE_NLA, 0);
}
