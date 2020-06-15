/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 * \brief transform modes used by different operators.
 */

#ifndef __TRANSFORM_MODE_H__
#define __TRANSFORM_MODE_H__

struct AnimData;
struct LinkNode;
struct TransData;
struct TransDataContainer;
struct TransInfo;
struct wmOperator;

/* header of TransDataEdgeSlideVert, TransDataEdgeSlideEdge */
typedef struct TransDataGenericSlideVert {
  struct BMVert *v;
  struct LinkNode **cd_loop_groups;
  float co_orig_3d[3];
} TransDataGenericSlideVert;

/* transform_mode.c */
bool transdata_check_local_center(TransInfo *t, short around);
bool transform_mode_is_changeable(const int mode);
void protectedTransBits(short protectflag, float vec[3]);
void constraintTransLim(TransInfo *t, TransData *td);
void postInputRotation(TransInfo *t, float values[3]);
void headerRotation(TransInfo *t, char *str, float final);
void ElementRotation_ex(TransInfo *t,
                        TransDataContainer *tc,
                        TransData *td,
                        const float mat[3][3],
                        const float *center);
void ElementRotation(
    TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3], const short around);
void headerResize(TransInfo *t, const float vec[3], char *str);
void ElementResize(TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3]);
short getAnimEdit_SnapMode(TransInfo *t);
void doAnimEdit_SnapFrame(
    TransInfo *t, TransData *td, TransData2D *td2d, struct AnimData *adt, short autosnap);
void transform_mode_init(TransInfo *t, struct wmOperator *op, const int mode);

/* transform_mode_align.c */
void initAlign(TransInfo *t);

/* transform_mode_baketime.c */
void initBakeTime(TransInfo *t);

/* transform_mode_bbone_resize.c */
void initBoneSize(TransInfo *t);

/* transform_mode_bend.c */
void initBend(TransInfo *t);

/* transform_mode_boneenvelope.c */
void initBoneEnvelope(TransInfo *t);

/* transform_mode_boneroll.c */
void initBoneRoll(TransInfo *t);

/* transform_mode_curveshrinkfatten.c */
void initCurveShrinkFatten(TransInfo *t);

/* transform_mode_edge_bevelweight.c */
void initBevelWeight(TransInfo *t);

/* transform_mode_edge_crease.c */
void initCrease(TransInfo *t);

/* transform_mode_edge_rotate_normal.c */
void initNormalRotation(TransInfo *t);

/* transform_mode_edge_seq_slide.c */
void initSeqSlide(TransInfo *t);

/* transform_mode_edge_slide.c */
void drawEdgeSlide(TransInfo *t);
void doEdgeSlide(TransInfo *t, float perc);
void initEdgeSlide_ex(
    TransInfo *t, bool use_double_side, bool use_even, bool flipped, bool use_clamp);
void initEdgeSlide(TransInfo *t);

/* transform_mode_gpopacity.c */
void initGPOpacity(TransInfo *t);

/* transform_mode_gpshrinkfatten.c */
void initGPShrinkFatten(TransInfo *t);

/* transform_mode_maskshrinkfatten.c */
void initMaskShrinkFatten(TransInfo *t);

/* transform_mode_mirror.c */
void initMirror(TransInfo *t);

/* transform_mode_push_pull.c */
void initPushPull(TransInfo *t);

/* transform_mode_resize.c */
void initResize(TransInfo *t);

/* transform_mode_rotate.c */
void initRotation(TransInfo *t);

/* transform_mode_shear.c */
void initShear(TransInfo *t);

/* transform_mode_shrink_fatten.c */
void initShrinkFatten(TransInfo *t);

/* transform_mode_skin_resize.c */
void initSkinResize(TransInfo *t);

/* transform_mode_tilt.c */
void initTilt(TransInfo *t);

/* transform_mode_timescale.c */
void initTimeScale(TransInfo *t);

/* transform_mode_timeslide.c */
void initTimeSlide(TransInfo *t);

/* transform_mode_timetranslate.c */
void initTimeTranslate(TransInfo *t);

/* transform_mode_tosphere.c */
void initToSphere(TransInfo *t);

/* transform_mode_trackball.c */
void initTrackball(TransInfo *t);

/* transform_mode_translate.c */
void initTranslation(TransInfo *t);

/* transform_mode_vert_slide.c */
void drawVertSlide(TransInfo *t);
void doVertSlide(TransInfo *t, float perc);
void initVertSlide_ex(TransInfo *t, bool use_even, bool flipped, bool use_clamp);
void initVertSlide(TransInfo *t);
#endif
