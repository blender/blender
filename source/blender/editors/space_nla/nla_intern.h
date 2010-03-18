/**
 * $Id:
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
#ifndef ED_NLA_INTERN_H
#define ED_NLA_INTERN_H

/* internal exports only */

/* **************************************** */
/* Macros, etc. only used by NLA */

/* **************************************** */
/* space_nla.c / nla_buttons.c */

ARegion *nla_has_buttons_region(ScrArea *sa);

void nla_buttons_register(ARegionType *art);
void NLA_OT_properties(wmOperatorType *ot);

/* **************************************** */
/* nla_draw.c */

void draw_nla_main_data(bAnimContext *ac, SpaceNla *snla, ARegion *ar);
void draw_nla_channel_list(bContext *C, bAnimContext *ac, SpaceNla *snla, ARegion *ar);

/* **************************************** */
/* nla_header.c */

void nla_header_buttons(const bContext *C, ARegion *ar);

/* **************************************** */
/* nla_select.c */

/* defines for left-right select tool */
enum {
	NLAEDIT_LRSEL_TEST	= -1,
	NLAEDIT_LRSEL_NONE,
	NLAEDIT_LRSEL_LEFT,
	NLAEDIT_LRSEL_RIGHT,
} eNlaEdit_LeftRightSelect_Mode;

/* --- */

void NLA_OT_select_all_toggle(wmOperatorType *ot);
void NLA_OT_select_border(wmOperatorType *ot);
void NLA_OT_click_select(wmOperatorType *ot);

/* **************************************** */
/* nla_edit.c */

/* defines for snap strips
 */
enum {
	NLAEDIT_SNAP_CFRA = 1,
	NLAEDIT_SNAP_NEAREST_FRAME,
	NLAEDIT_SNAP_NEAREST_SECOND,
	NLAEDIT_SNAP_NEAREST_MARKER,	
} eNlaEdit_Snap_Mode;

/* --- */

void NLA_OT_tweakmode_enter(wmOperatorType *ot);
void NLA_OT_tweakmode_exit(wmOperatorType *ot);

/* --- */

void NLA_OT_actionclip_add(wmOperatorType *ot);
void NLA_OT_transition_add(wmOperatorType *ot);

void NLA_OT_meta_add(wmOperatorType *ot);
void NLA_OT_meta_remove(wmOperatorType *ot);

void NLA_OT_duplicate(wmOperatorType *ot);
void NLA_OT_delete(wmOperatorType *ot);
void NLA_OT_split(wmOperatorType *ot);

void NLA_OT_mute_toggle(wmOperatorType *ot);

void NLA_OT_move_up(wmOperatorType *ot);
void NLA_OT_move_down(wmOperatorType *ot);

void NLA_OT_action_sync_length(wmOperatorType *ot);

void NLA_OT_apply_scale(wmOperatorType *ot);
void NLA_OT_clear_scale(wmOperatorType *ot);

void NLA_OT_snap(wmOperatorType *ot);

void NLA_OT_fmodifier_add(wmOperatorType *ot);
void NLA_OT_fmodifier_copy(wmOperatorType *ot);
void NLA_OT_fmodifier_paste(wmOperatorType *ot);


/* **************************************** */
/* nla_channels.c */

void NLA_OT_channels_click(wmOperatorType *ot);

void NLA_OT_tracks_add(wmOperatorType *ot);
void NLA_OT_delete_tracks(wmOperatorType *ot);

/* **************************************** */
/* nla_ops.c */

int nlaop_poll_tweakmode_off(bContext *C);
int nlaop_poll_tweakmode_on (bContext *C);

short nlaedit_is_tweakmode_on(bAnimContext *ac);

/* --- */

void nla_operatortypes(void);
void nla_keymap(wmKeyConfig *keyconf);

#endif /* ED_NLA_INTERN_H */

