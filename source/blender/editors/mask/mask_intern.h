/* SPDX-FileCopyrightText: 2011 Blender Foundation
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

void MASK_OT_add_vertex(struct wmOperatorType *ot);
void MASK_OT_add_feather_vertex(struct wmOperatorType *ot);
void MASK_OT_primitive_circle_add(struct wmOperatorType *ot);
void MASK_OT_primitive_square_add(struct wmOperatorType *ot);

/* `mask_ops.cc` */

struct Mask *ED_mask_new(struct bContext *C, const char *name);
/**
 * Get active layer. Will create mask/layer to be sure there's an active layer.
 */
struct MaskLayer *ED_mask_layer_ensure(struct bContext *C, bool *r_added_mask);

void MASK_OT_new(struct wmOperatorType *ot);
void MASK_OT_layer_new(struct wmOperatorType *ot);
void MASK_OT_layer_remove(struct wmOperatorType *ot);
void MASK_OT_cyclic_toggle(struct wmOperatorType *ot);

void MASK_OT_slide_point(struct wmOperatorType *ot);
void MASK_OT_slide_spline_curvature(struct wmOperatorType *ot);

void MASK_OT_delete(struct wmOperatorType *ot);

void MASK_OT_hide_view_clear(struct wmOperatorType *ot);
void MASK_OT_hide_view_set(struct wmOperatorType *ot);
void MASK_OT_feather_weight_clear(struct wmOperatorType *ot);
void MASK_OT_switch_direction(struct wmOperatorType *ot);
/* Named to match mesh recalculate normals. */
void MASK_OT_normals_make_consistent(struct wmOperatorType *ot);

void MASK_OT_handle_type_set(struct wmOperatorType *ot);

void MASK_OT_layer_move(struct wmOperatorType *ot);

void MASK_OT_duplicate(struct wmOperatorType *ot);
void MASK_OT_copy_splines(struct wmOperatorType *ot);
void MASK_OT_paste_splines(struct wmOperatorType *ot);

/* `mask_relationships.cc` */

/** based on #OBJECT_OT_parent_set */
void MASK_OT_parent_set(struct wmOperatorType *ot);
void MASK_OT_parent_clear(struct wmOperatorType *ot);

/* `mask_select.cc` */

void MASK_OT_select(struct wmOperatorType *ot);
void MASK_OT_select_all(struct wmOperatorType *ot);

void MASK_OT_select_box(struct wmOperatorType *ot);
void MASK_OT_select_lasso(struct wmOperatorType *ot);
void MASK_OT_select_circle(struct wmOperatorType *ot);
void MASK_OT_select_linked_pick(struct wmOperatorType *ot);
void MASK_OT_select_linked(struct wmOperatorType *ot);
void MASK_OT_select_more(struct wmOperatorType *ot);
void MASK_OT_select_less(struct wmOperatorType *ot);

/* 'check' select */
bool ED_mask_spline_select_check(const struct MaskSpline *spline);
bool ED_mask_layer_select_check(const struct MaskLayer *mask_layer);
bool ED_mask_select_check(const struct Mask *mask);

void ED_mask_spline_select_set(struct MaskSpline *spline, bool do_select);
void ED_mask_layer_select_set(struct MaskLayer *mask_layer, bool do_select);
void ED_mask_select_toggle_all(struct Mask *mask, int action);
void ED_mask_select_flush_all(struct Mask *mask);

/* mask_editor.c */

/* Generalized solution for preserving editor viewport when making changes while lock-to-selection
 * is enabled.
 * Any mask operator can use this API, without worrying that some editors do not have an idea of
 * lock-to-selection. */

typedef struct MaskViewLockState {
  ClipViewLockState space_clip_state;
} MaskViewLockState;

void ED_mask_view_lock_state_store(const struct bContext *C, MaskViewLockState *state);
void ED_mask_view_lock_state_restore_no_jump(const struct bContext *C,
                                             const MaskViewLockState *state);

/* `mask_query.cc` */

bool ED_mask_find_nearest_diff_point(const struct bContext *C,
                                     struct Mask *mask,
                                     const float normal_co[2],
                                     int threshold,
                                     bool feather,
                                     float tangent[2],
                                     bool use_deform,
                                     bool use_project,
                                     struct MaskLayer **r_mask_layer,
                                     struct MaskSpline **r_spline,
                                     struct MaskSplinePoint **r_point,
                                     float *r_u,
                                     float *r_score);
bool ED_mask_feather_find_nearest(const struct bContext *C,
                                  struct Mask *mask,
                                  const float normal_co[2],
                                  float threshold,
                                  struct MaskLayer **r_mask_layer,
                                  struct MaskSpline **r_spline,
                                  struct MaskSplinePoint **r_point,
                                  struct MaskSplinePointUW **r_uw,
                                  float *r_score);
struct MaskSplinePoint *ED_mask_point_find_nearest(const struct bContext *C,
                                                   struct Mask *mask,
                                                   const float normal_co[2],
                                                   float threshold,
                                                   struct MaskLayer **r_mask_layer,
                                                   struct MaskSpline **r_spline,
                                                   eMaskWhichHandle *r_which_handle,
                                                   float *r_score);

/* `mask_shapekey.cc` */

void MASK_OT_shape_key_insert(struct wmOperatorType *ot);
void MASK_OT_shape_key_clear(struct wmOperatorType *ot);
void MASK_OT_shape_key_feather_reset(struct wmOperatorType *ot);
void MASK_OT_shape_key_rekey(struct wmOperatorType *ot);
