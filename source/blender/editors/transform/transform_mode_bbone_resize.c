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
 */

#include <stdlib.h>

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_constraints.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (EditBone (B-bone) width scaling) */

/** \name Transform B-bone width scaling
 * \{ */

static void headerBoneSize(TransInfo *t, const float vec[3], char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
  }

  /* hmm... perhaps the y-axis values don't need to be shown? */
  if (t->con.mode & CON_APPLY) {
    if (t->num.idx_max == 0) {
      BLI_snprintf(
          str, UI_MAX_DRAW_STR, TIP_("ScaleB: %s%s %s"), &tvec[0], t->con.text, t->proptext);
    }
    else {
      BLI_snprintf(str,
                   UI_MAX_DRAW_STR,
                   TIP_("ScaleB: %s : %s : %s%s %s"),
                   &tvec[0],
                   &tvec[NUM_STR_REP_LEN],
                   &tvec[NUM_STR_REP_LEN * 2],
                   t->con.text,
                   t->proptext);
    }
  }
  else {
    BLI_snprintf(str,
                 UI_MAX_DRAW_STR,
                 TIP_("ScaleB X: %s  Y: %s  Z: %s%s %s"),
                 &tvec[0],
                 &tvec[NUM_STR_REP_LEN],
                 &tvec[NUM_STR_REP_LEN * 2],
                 t->con.text,
                 t->proptext);
  }
}

static void ElementBoneSize(TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3])
{
  float tmat[3][3], smat[3][3], oldy;
  float sizemat[3][3];

  mul_m3_m3m3(smat, mat, td->mtx);
  mul_m3_m3m3(tmat, td->smtx, smat);

  if (t->con.applySize) {
    t->con.applySize(t, tc, td, tmat);
  }

  /* we've tucked the scale in loc */
  oldy = td->iloc[1];
  size_to_mat3(sizemat, td->iloc);
  mul_m3_m3m3(tmat, tmat, sizemat);
  mat3_to_size(td->loc, tmat);
  td->loc[1] = oldy;
}

static void applyBoneSize(TransInfo *t, const int UNUSED(mval[2]))
{
  float mat[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];

  if (t->flag & T_INPUT_IS_VALUES_FINAL) {
    copy_v3_v3(t->values_final, t->values);
  }
  else {
    float ratio = t->values[0];

    copy_v3_fl(t->values_final, ratio);

    snapGridIncrement(t, t->values_final);

    if (applyNumInput(&t->num, t->values_final)) {
      constraintNumInput(t, t->values_final);
    }
  }

  size_to_mat3(mat, t->values_final);

  if (t->con.applySize) {
    t->con.applySize(t, NULL, NULL, mat);
  }

  copy_m3_m3(t->mat, mat);  // used in gizmo

  headerBoneSize(t, t->values_final, str);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      ElementBoneSize(t, tc, td, mat);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initBoneSize(TransInfo *t)
{
  t->mode = TFM_BONESIZE;
  t->transform = applyBoneSize;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->idx_max = 2;
  t->num.idx_max = 2;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
  t->num.unit_type[2] = B_UNIT_NONE;
}
/** \} */
