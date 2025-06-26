/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#define SNAP_MIN_DISTANCE 30

#include "transform.hh"

namespace blender::ed::transform {

bool peelObjectsTransform(TransInfo *t,
                          const float mval[2],
                          bool use_peel_object,
                          /* Return args. */
                          float r_loc[3],
                          float r_no[3],
                          float *r_thickness);

bool transformModeUseSnap(const TransInfo *t);

void tranform_snap_target_median_calc(const TransInfo *t, float r_median[3]);
bool transform_snap_increment_ex(const TransInfo *t, bool use_local_space, float *r_val);
bool transform_snap_increment(const TransInfo *t, float *r_val);
float transform_snap_increment_get(const TransInfo *t);

void tranform_snap_source_restore_context(TransInfo *t);

void transform_snap_flag_from_modifiers_set(TransInfo *t);
bool transform_snap_is_active(const TransInfo *t);

bool validSnap(const TransInfo *t);

void transform_snap_grid_init(const TransInfo *t, float r_snap[3], float *r_snap_precision);
void transform_snap_reset_from_mode(TransInfo *t, wmOperator *op);
void initSnapping(TransInfo *t, wmOperator *op);
void freeSnapping(TransInfo *t);
void initSnapAngleIncrements(TransInfo *t);
bool transform_snap_project_individual_is_active(const TransInfo *t);
void transform_snap_project_individual_apply(TransInfo *t);
void transform_snap_mixed_apply(TransInfo *t, float *vec);
void resetSnapping(TransInfo *t);
eRedrawFlag handleSnapping(TransInfo *t, const wmEvent *event);
void drawSnapping(TransInfo *t);
bool usingSnappingNormal(const TransInfo *t);
bool validSnappingNormal(const TransInfo *t);

short *transform_snap_flag_from_spacetype_ptr(TransInfo *t, const struct PropertyRNA **r_prop);

void getSnapPoint(const TransInfo *t, float vec[3]);
void addSnapPoint(TransInfo *t);
eRedrawFlag updateSelectedSnapPoint(TransInfo *t);
void removeSnapPoint(TransInfo *t);

float transform_snap_distance_len_squared_fn(TransInfo *t, const float p1[3], const float p2[3]);

/* `transform_snap_sequencer.cc` */

TransSeqSnapData *snap_sequencer_data_alloc(const TransInfo *t);
void snap_sequencer_data_free(TransSeqSnapData *data);
bool snap_sequencer_calc(TransInfo *t);
void snap_sequencer_apply_seqslide(TransInfo *t, float *vec);
void snap_sequencer_image_apply_translate(TransInfo *t, float vec[2]);

/* `transform_snap_animation.cc` */
void snapFrameTransform(
    TransInfo *t, eSnapMode snap_mode, float val_initial, float val_final, float *r_val_final);
/**
 * This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker.
 */
void transform_snap_anim_flush_data(TransInfo *t,
                                    TransData *td,
                                    eSnapMode snap_mode,
                                    float *r_val_final);
bool transform_snap_nla_calc(TransInfo *t, float *vec);

}  // namespace blender::ed::transform
