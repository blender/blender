/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#pragma once

#include "ED_clip.hh"

struct Mask;
struct bContext;
struct wmOperatorType;

/* internal exports only */

/* `mask_add.cc` */

void MASK_OT_add_vertex(wmOperatorType *ot);
void MASK_OT_add_feather_vertex(wmOperatorType *ot);
void MASK_OT_primitive_circle_add(wmOperatorType *ot);
void MASK_OT_primitive_square_add(wmOperatorType *ot);

/* `mask_ops.cc` */

Mask *ED_mask_new(bContext *C, const char *name);
/**
 * Get active layer. Will create mask/layer to be sure there's an active layer.
 */
MaskLayer *ED_mask_layer_ensure(bContext *C, bool *r_added_mask);

void MASK_OT_new(wmOperatorType *ot);
void MASK_OT_layer_new(wmOperatorType *ot);
void MASK_OT_layer_remove(wmOperatorType *ot);
void MASK_OT_cyclic_toggle(wmOperatorType *ot);

void MASK_OT_slide_point(wmOperatorType *ot);
void MASK_OT_slide_spline_curvature(wmOperatorType *ot);

void MASK_OT_delete(wmOperatorType *ot);

void MASK_OT_hide_view_clear(wmOperatorType *ot);
void MASK_OT_hide_view_set(wmOperatorType *ot);
void MASK_OT_feather_weight_clear(wmOperatorType *ot);
void MASK_OT_switch_direction(wmOperatorType *ot);
/* Named to match mesh recalculate normals. */
void MASK_OT_normals_make_consistent(wmOperatorType *ot);

void MASK_OT_handle_type_set(wmOperatorType *ot);

void MASK_OT_layer_move(wmOperatorType *ot);

void MASK_OT_duplicate(wmOperatorType *ot);
void MASK_OT_copy_splines(wmOperatorType *ot);
void MASK_OT_paste_splines(wmOperatorType *ot);

/* `mask_relationships.cc` */

/** based on #OBJECT_OT_parent_set */
void MASK_OT_parent_set(wmOperatorType *ot);
void MASK_OT_parent_clear(wmOperatorType *ot);

/* `mask_select.cc` */

void MASK_OT_select(wmOperatorType *ot);
void MASK_OT_select_all(wmOperatorType *ot);

void MASK_OT_select_box(wmOperatorType *ot);
void MASK_OT_select_lasso(wmOperatorType *ot);
void MASK_OT_select_circle(wmOperatorType *ot);
void MASK_OT_select_linked_pick(wmOperatorType *ot);
void MASK_OT_select_linked(wmOperatorType *ot);
void MASK_OT_select_more(wmOperatorType *ot);
void MASK_OT_select_less(wmOperatorType *ot);

/* 'check' select */
bool ED_mask_spline_select_check(const MaskSpline *spline);
bool ED_mask_layer_select_check(const MaskLayer *mask_layer);
bool ED_mask_select_check(const Mask *mask);

void ED_mask_spline_select_set(MaskSpline *spline, bool do_select);
void ED_mask_layer_select_set(MaskLayer *mask_layer, bool do_select);
void ED_mask_select_toggle_all(Mask *mask, int action);
void ED_mask_select_flush_all(Mask *mask);

/* mask_editor.c */

/* Generalized solution for preserving editor viewport when making changes while lock-to-selection
 * is enabled.
 * Any mask operator can use this API, without worrying that some editors do not have an idea of
 * lock-to-selection. */

struct MaskViewLockState {
  ClipViewLockState space_clip_state;
};

void ED_mask_view_lock_state_store(const bContext *C, MaskViewLockState *state);
void ED_mask_view_lock_state_restore_no_jump(const bContext *C, const MaskViewLockState *state);

/* `mask_query.cc` */

bool ED_mask_find_nearest_diff_point(const bContext *C,
                                     Mask *mask,
                                     const float normal_co[2],
                                     int threshold,
                                     bool feather,
                                     float tangent[2],
                                     bool use_deform,
                                     bool use_project,
                                     MaskLayer **r_mask_layer,
                                     MaskSpline **r_spline,
                                     MaskSplinePoint **r_point,
                                     float *r_u,
                                     float *r_score);
bool ED_mask_feather_find_nearest(const bContext *C,
                                  Mask *mask,
                                  const float normal_co[2],
                                  float threshold,
                                  MaskLayer **r_mask_layer,
                                  MaskSpline **r_spline,
                                  MaskSplinePoint **r_point,
                                  MaskSplinePointUW **r_uw,
                                  float *r_score);
MaskSplinePoint *ED_mask_point_find_nearest(const bContext *C,
                                            Mask *mask,
                                            const float normal_co[2],
                                            float threshold,
                                            MaskLayer **r_mask_layer,
                                            MaskSpline **r_spline,
                                            eMaskWhichHandle *r_which_handle,
                                            float *r_score);

/* `mask_shapekey.cc` */

void MASK_OT_shape_key_insert(wmOperatorType *ot);
void MASK_OT_shape_key_clear(wmOperatorType *ot);
void MASK_OT_shape_key_feather_reset(wmOperatorType *ot);
void MASK_OT_shape_key_rekey(wmOperatorType *ot);
