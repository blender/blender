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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_nla/nla_intern.h
 *  \ingroup spnla
 */

#ifndef __NLA_INTERN_H__
#define __NLA_INTERN_H__

/* internal exports only */

/* **************************************** */
/* Macros, etc. only used by NLA */

/* **************************************** */
/* space_nla.c / nla_buttons.c */

ARegion *nla_has_buttons_region(ScrArea *sa);

bool nla_panel_context(const bContext *C, PointerRNA *adt_ptr, PointerRNA *nlt_ptr, PointerRNA *strip_ptr);

void nla_buttons_register(ARegionType *art);
void NLA_OT_properties(wmOperatorType *ot);

/* **************************************** */
/* nla_draw.c */

void draw_nla_main_data(bAnimContext *ac, SpaceNla *snla, ARegion *ar);
void draw_nla_channel_list(const bContext *C, bAnimContext *ac, ARegion *ar);

/* **************************************** */
/* nla_select.c */

/* defines for left-right select tool */
enum eNlaEdit_LeftRightSelect_Mode {
	NLAEDIT_LRSEL_TEST  = -1,
	NLAEDIT_LRSEL_NONE,
	NLAEDIT_LRSEL_LEFT,
	NLAEDIT_LRSEL_RIGHT
};

/* --- */

void NLA_OT_select_all_toggle(wmOperatorType *ot);
void NLA_OT_select_border(wmOperatorType *ot);
void NLA_OT_select_leftright(wmOperatorType *ot);
void NLA_OT_click_select(wmOperatorType *ot);

/* **************************************** */
/* nla_edit.c */

/* defines for snap strips */
enum eNlaEdit_Snap_Mode {
	NLAEDIT_SNAP_CFRA = 1,
	NLAEDIT_SNAP_NEAREST_FRAME,
	NLAEDIT_SNAP_NEAREST_SECOND,
	NLAEDIT_SNAP_NEAREST_MARKER
};

/* --- */

bool nlaedit_disable_tweakmode(bAnimContext *ac);

void NLA_OT_tweakmode_enter(wmOperatorType *ot);
void NLA_OT_tweakmode_exit(wmOperatorType *ot);

/* --- */

void NLA_OT_previewrange_set(wmOperatorType *ot);

void NLA_OT_view_all(wmOperatorType *ot);
void NLA_OT_view_selected(wmOperatorType *ot);

void NLA_OT_actionclip_add(wmOperatorType *ot);
void NLA_OT_transition_add(wmOperatorType *ot);
void NLA_OT_soundclip_add(wmOperatorType *ot);

void NLA_OT_meta_add(wmOperatorType *ot);
void NLA_OT_meta_remove(wmOperatorType *ot);

void NLA_OT_duplicate(wmOperatorType *ot);
void NLA_OT_delete(wmOperatorType *ot);
void NLA_OT_split(wmOperatorType *ot);

void NLA_OT_mute_toggle(wmOperatorType *ot);

void NLA_OT_swap(wmOperatorType *ot);
void NLA_OT_move_up(wmOperatorType *ot);
void NLA_OT_move_down(wmOperatorType *ot);

void NLA_OT_action_sync_length(wmOperatorType *ot);

void NLA_OT_make_single_user(wmOperatorType *ot);

void NLA_OT_apply_scale(wmOperatorType *ot);
void NLA_OT_clear_scale(wmOperatorType *ot);

void NLA_OT_snap(wmOperatorType *ot);

void NLA_OT_fmodifier_add(wmOperatorType *ot);
void NLA_OT_fmodifier_copy(wmOperatorType *ot);
void NLA_OT_fmodifier_paste(wmOperatorType *ot);


/* **************************************** */
/* nla_channels.c */

bool nlaedit_add_tracks_existing(bAnimContext *ac, bool above_sel);
bool nlaedit_add_tracks_empty(bAnimContext *ac);

/* --- */

void NLA_OT_channels_click(wmOperatorType *ot);

void NLA_OT_action_pushdown(wmOperatorType *ot);

void NLA_OT_tracks_add(wmOperatorType *ot);
void NLA_OT_tracks_delete(wmOperatorType *ot);

void NLA_OT_selected_objects_add(wmOperatorType *ot);

/* **************************************** */
/* nla_ops.c */

int nlaop_poll_tweakmode_off(bContext *C);
int nlaop_poll_tweakmode_on(bContext *C);

bool nlaedit_is_tweakmode_on(bAnimContext *ac);

/* --- */

void nla_operatortypes(void);
void nla_keymap(wmKeyConfig *keyconf);

#endif /* __NLA_INTERN_H__ */

