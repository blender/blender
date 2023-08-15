/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 * \brief transform modes used by different operators.
 */

#pragma once

struct BMVert;
struct LinkNode;
struct TransData;
struct TransDataContainer;
struct TransInfo;
struct bContext;
struct wmOperator;
struct wmEvent;

struct TransModeInfo {
  int flags; /* eTFlag */

  void (*init_fn)(TransInfo *, wmOperator *);

  /** Main transform mode function. */
  void (*transform_fn)(TransInfo *);

  /**
   * Optional callback to transform a single matrix.
   *
   * \note used by the gizmo to transform the matrix used to position it.
   */
  void (*transform_matrix_fn)(TransInfo *, float[4][4]);

  /* Event handler function that determines whether the viewport needs to be redrawn. */
  enum eRedrawFlag (*handle_event_fn)(TransInfo *, const wmEvent *);

  /**
   * Get the transform distance between two points (used by Closest snap)
   *
   * \note Return value can be anything,
   * where the smallest absolute value defines what's closest.
   */
  float (*snap_distance_fn)(TransInfo *t, const float p1[3], const float p2[3]);
  void (*snap_apply_fn)(TransInfo *, float *);

  /** Custom drawing. */
  void (*draw_fn)(TransInfo *);
};

/* header of TransDataEdgeSlideVert, TransDataEdgeSlideEdge */
struct TransDataGenericSlideVert {
  BMVert *v;
  LinkNode **cd_loop_groups;
  float co_orig_3d[3];
};

/* `transform_mode.cc` */

eTfmMode transform_mode_really_used(bContext *C, eTfmMode mode);
bool transdata_check_local_center(const TransInfo *t, short around);
/**
 * Informs if the mode can be switched during modal.
 */
bool transform_mode_is_changeable(int mode);
void protectedTransBits(short protectflag, float vec[3]);
void protectedSizeBits(short protectflag, float size[3]);
void constraintTransLim(const TransInfo *t, TransData *td);
void constraintSizeLim(const TransInfo *t, TransData *td);
/**
 * Used by Transform Rotation and Transform Normal Rotation.
 */
void headerRotation(TransInfo *t, char *str, int str_size, float final);
/**
 * Applies values of rotation to `td->loc` and `td->ext->quat`
 * based on a rotation matrix (mat) and a pivot (center).
 *
 * Protected axis and other transform settings are taken into account.
 */
void ElementRotation_ex(const TransInfo *t,
                        const TransDataContainer *tc,
                        TransData *td,
                        const float mat[3][3],
                        const float *center);
void ElementRotation(const TransInfo *t,
                     const TransDataContainer *tc,
                     TransData *td,
                     const float mat[3][3],
                     short around);
void headerResize(TransInfo *t, const float vec[3], char *str, int str_size);
void ElementResize(const TransInfo *t,
                   const TransDataContainer *tc,
                   TransData *td,
                   const float mat[3][3]);
void transform_mode_init(TransInfo *t, wmOperator *op, int mode);
/**
 * When in modal and not set, initializes a default orientation for the mode.
 */
void transform_mode_default_modal_orientation_set(TransInfo *t, int type);

/* `transform_mode_align.cc` */

extern TransModeInfo TransMode_align;

/* `transform_mode_baketime.cc` */

extern TransModeInfo TransMode_baketime;

/* `transform_mode_bbone_resize.cc` */

extern TransModeInfo TransMode_bboneresize;

/* `transform_mode_bend.cc` */

extern TransModeInfo TransMode_bend;

/* `transform_mode_boneenvelope.cc` */

extern TransModeInfo TransMode_boneenvelope;

/* `transform_mode_boneroll.cc` */

extern TransModeInfo TransMode_boneroll;

/* `transform_mode_curveshrinkfatten.cc` */

extern TransModeInfo TransMode_curveshrinkfatten;

/* `transform_mode_customdata.cc` */

extern TransModeInfo TransMode_edgecrease;
extern TransModeInfo TransMode_vertcrease;
extern TransModeInfo TransMode_bevelweight;

/* `transform_mode_edge_rotate_normal.cc` */

extern TransModeInfo TransMode_rotatenormal;

/* `transform_mode_edge_seq_slide.cc` */

extern TransModeInfo TransMode_seqslide;

/* `transform_mode_edge_slide.cc` */

extern TransModeInfo TransMode_edgeslide;
void transform_mode_edge_slide_reproject_input(TransInfo *t);

/* `transform_mode_gpopacity.cc` */

extern TransModeInfo TransMode_gpopacity;

/* `transform_mode_gpshrinkfatten.cc` */

extern TransModeInfo TransMode_gpshrinkfatten;

/* `transform_mode_maskshrinkfatten.cc` */

extern TransModeInfo TransMode_maskshrinkfatten;

/* `transform_mode_mirror.cc` */

extern TransModeInfo TransMode_mirror;

/* `transform_mode_push_pull.cc` */

extern TransModeInfo TransMode_pushpull;

/* `transform_mode_resize.cc` */

extern TransModeInfo TransMode_resize;

/* `transform_mode_rotate.cc` */

extern TransModeInfo TransMode_rotate;

/* `transform_mode_shear.cc` */

extern TransModeInfo TransMode_shear;

/* `transform_mode_shrink_fatten.cc` */

extern TransModeInfo TransMode_shrinkfatten;

/* `transform_mode_skin_resize.cc` */

extern TransModeInfo TransMode_skinresize;

/* `transform_mode_snapsource.cc` */

extern TransModeInfo TransMode_snapsource;
void transform_mode_snap_source_init(TransInfo *t, wmOperator *op);

/* `transform_mode_tilt.cc` */

extern TransModeInfo TransMode_tilt;

/* `transform_mode_timescale.cc` */

extern TransModeInfo TransMode_timescale;

/* `transform_mode_timeslide.cc` */

extern TransModeInfo TransMode_timeslide;

/* `transform_mode_timetranslate.cc` */

extern TransModeInfo TransMode_timetranslate;

/* `transform_mode_tosphere.cc` */

extern TransModeInfo TransMode_tosphere;

/* `transform_mode_trackball.cc` */

extern TransModeInfo TransMode_trackball;

/* `transform_mode_translate.cc` */

extern TransModeInfo TransMode_translate;

/* `transform_mode_vert_slide.cc` */

extern TransModeInfo TransMode_vertslide;
void transform_mode_vert_slide_reproject_input(TransInfo *t);
