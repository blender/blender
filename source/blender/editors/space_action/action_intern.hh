/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#pragma once

struct ARegion;
struct ARegionType;
struct Object;
struct Scene;
struct SpaceAction;
struct bAnimContext;
struct bContext;
struct wmOperatorType;

/* internal exports only */

/* **************************************** */
/* `space_action.cc` / `action_buttons.cc` */

void action_buttons_register(ARegionType *art);

/* ***************************************** */
/* `action_draw.cc` */

/**
 * Left hand part.
 */
void draw_channel_names(bContext *C, bAnimContext *ac, ARegion *region);
/**
 * Draw keyframes in each channel.
 */
void draw_channel_strips(bAnimContext *ac, SpaceAction *saction, ARegion *region);

void timeline_draw_cache(const SpaceAction *saction, const Object *ob, const Scene *scene);

/* ***************************************** */
/* `action_select.cc` */

void ACTION_OT_select_all(wmOperatorType *ot);
void ACTION_OT_select_box(wmOperatorType *ot);
void ACTION_OT_select_lasso(wmOperatorType *ot);
void ACTION_OT_select_circle(wmOperatorType *ot);
void ACTION_OT_select_column(wmOperatorType *ot);
void ACTION_OT_select_linked(wmOperatorType *ot);
void ACTION_OT_select_more(wmOperatorType *ot);
void ACTION_OT_select_less(wmOperatorType *ot);
void ACTION_OT_select_leftright(wmOperatorType *ot);
void ACTION_OT_clickselect(wmOperatorType *ot);

/* defines for left-right select tool */
enum eActKeys_LeftRightSelect_Mode {
  ACTKEYS_LRSEL_TEST = 0,
  ACTKEYS_LRSEL_LEFT,
  ACTKEYS_LRSEL_RIGHT,
};

/* defines for column-select mode */
enum eActKeys_ColumnSelect_Mode {
  ACTKEYS_COLUMNSEL_KEYS = 0,
  ACTKEYS_COLUMNSEL_CFRA,
  ACTKEYS_COLUMNSEL_MARKERS_COLUMN,
  ACTKEYS_COLUMNSEL_MARKERS_BETWEEN,
};

/* ***************************************** */
/* `action_edit.cc` */

void ACTION_OT_previewrange_set(wmOperatorType *ot);
void ACTION_OT_view_all(wmOperatorType *ot);
void ACTION_OT_view_selected(wmOperatorType *ot);
void ACTION_OT_view_frame(wmOperatorType *ot);

void ACTION_OT_copy(wmOperatorType *ot);
void ACTION_OT_paste(wmOperatorType *ot);

void ACTION_OT_keyframe_insert(wmOperatorType *ot);
void ACTION_OT_duplicate(wmOperatorType *ot);
void ACTION_OT_delete(wmOperatorType *ot);
void ACTION_OT_clean(wmOperatorType *ot);
void ACTION_OT_sample(wmOperatorType *ot);

void ACTION_OT_keyframe_type(wmOperatorType *ot);
void ACTION_OT_handle_type(wmOperatorType *ot);
void ACTION_OT_interpolation_type(wmOperatorType *ot);
void ACTION_OT_extrapolation_type(wmOperatorType *ot);
void ACTION_OT_easing_type(wmOperatorType *ot);

void ACTION_OT_frame_jump(wmOperatorType *ot);

void ACTION_OT_snap(wmOperatorType *ot);
void ACTION_OT_mirror(wmOperatorType *ot);

void ACTION_OT_new(wmOperatorType *ot);
void ACTION_OT_unlink(wmOperatorType *ot);

void ACTION_OT_push_down(wmOperatorType *ot);
void ACTION_OT_stash(wmOperatorType *ot);
void ACTION_OT_stash_and_create(wmOperatorType *ot);

void ACTION_OT_layer_next(wmOperatorType *ot);
void ACTION_OT_layer_prev(wmOperatorType *ot);

void ACTION_OT_markers_make_local(wmOperatorType *ot);

/* defines for snap keyframes
 * NOTE: keep in sync with eEditKeyframes_Snap (in ED_keyframes_edit.h)
 */
enum eActKeys_Snap_Mode {
  ACTKEYS_SNAP_CFRA = 1,
  ACTKEYS_SNAP_NEAREST_FRAME,
  ACTKEYS_SNAP_NEAREST_SECOND,
  ACTKEYS_SNAP_NEAREST_MARKER,
};

/* defines for mirror keyframes
 * NOTE: keep in sync with eEditKeyframes_Mirror (in ED_keyframes_edit.h)
 */
enum eActKeys_Mirror_Mode {
  ACTKEYS_MIRROR_CFRA = 1,
  ACTKEYS_MIRROR_YAXIS,
  ACTKEYS_MIRROR_XAXIS,
  ACTKEYS_MIRROR_MARKER,
};

/* ***************************************** */
/* `action_ops.cc` */

void action_operatortypes();
void action_keymap(wmKeyConfig *keyconf);
