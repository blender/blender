/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 */

#pragma once

/* internal exports only */

/* **************************************** */
/* Macros, etc. only used by NLA */

/* **************************************** */
/* `space_nla.cc` / `nla_buttons.cc`. */

bool nla_panel_context(const bContext *C,
                       PointerRNA *adt_ptr,
                       PointerRNA *nlt_ptr,
                       PointerRNA *strip_ptr);

void nla_buttons_register(ARegionType *art);

/* **************************************** */
/* `nla_draw.cc` */

void draw_nla_main_data(bAnimContext *ac, SpaceNla *snla, ARegion *region);
void draw_nla_track_list(const bContext *C,
                         bAnimContext *ac,
                         ARegion *region,
                         const ListBase /* bAnimListElem */ &anim_data);

/* **************************************** */
/* `nla_select.cc` */

/* defines for left-right select tool */
enum eNlaEdit_LeftRightSelect_Mode {
  NLAEDIT_LRSEL_TEST = -1,
  NLAEDIT_LRSEL_NONE,
  NLAEDIT_LRSEL_LEFT,
  NLAEDIT_LRSEL_RIGHT,
};

/* --- */

void NLA_OT_select_all(wmOperatorType *ot);
void NLA_OT_select_box(wmOperatorType *ot);
void NLA_OT_select_leftright(wmOperatorType *ot);
void NLA_OT_click_select(wmOperatorType *ot);

/* **************************************** */
/* `nla_edit.cc` */

/* defines for snap strips */
enum eNlaEdit_Snap_Mode {
  NLAEDIT_SNAP_CFRA = 1,
  NLAEDIT_SNAP_NEAREST_FRAME,
  NLAEDIT_SNAP_NEAREST_SECOND,
  NLAEDIT_SNAP_NEAREST_MARKER,
};

/* --- */

/**
 * NLA Editor internal API function for exiting tweak-mode.
 */
bool nlaedit_disable_tweakmode(bAnimContext *ac, bool do_solo);

void NLA_OT_tweakmode_enter(wmOperatorType *ot);
void NLA_OT_tweakmode_exit(wmOperatorType *ot);

/* --- */

void NLA_OT_previewrange_set(wmOperatorType *ot);

void NLA_OT_view_all(wmOperatorType *ot);
void NLA_OT_view_selected(wmOperatorType *ot);
void NLA_OT_view_frame(wmOperatorType *ot);

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
/* `nla_tracks.cc` */

/**
 * Helper - add NLA Tracks alongside existing ones.
 */
bool nlaedit_add_tracks_existing(bAnimContext *ac, bool above_sel);
/**
 * helper - add NLA Tracks to empty (and selected) AnimData blocks.
 */
bool nlaedit_add_tracks_empty(bAnimContext *ac);

/* --- */

void NLA_OT_channels_click(wmOperatorType *ot);

void NLA_OT_action_pushdown(wmOperatorType *ot);
void NLA_OT_action_unlink(wmOperatorType *ot);

void NLA_OT_tracks_add(wmOperatorType *ot);
void NLA_OT_tracks_delete(wmOperatorType *ot);

void NLA_OT_selected_objects_add(wmOperatorType *ot);

/* **************************************** */
/* `nla_ops.cc` */

/**
 * Tweak-mode is NOT enabled.
 */
bool nlaop_poll_tweakmode_off(bContext *C);
/**
 * Tweak-mode IS enabled.
 */
bool nlaop_poll_tweakmode_on(bContext *C);

/**
 * Is tweak-mode enabled - for use in NLA operator code.
 */
bool nlaedit_is_tweakmode_on(bAnimContext *ac);

/* --- */

void nla_operatortypes();
void nla_keymap(wmKeyConfig *keyconf);
