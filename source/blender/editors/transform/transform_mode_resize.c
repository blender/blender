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

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "transform.h"
#include "transform_constraints.h"
#include "transform_convert.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Resize) */

/** \name Transform Resize
 * \{ */

static float ResizeBetween(TransInfo *t, const float p1[3], const float p2[3])
{
  float d1[3], d2[3], len_d1;

  sub_v3_v3v3(d1, p1, t->center_global);
  sub_v3_v3v3(d2, p2, t->center_global);

  if (t->con.applyRot != NULL && (t->con.mode & CON_APPLY)) {
    mul_m3_v3(t->con.pmtx, d1);
    mul_m3_v3(t->con.pmtx, d2);
  }

  project_v3_v3v3(d1, d1, d2);

  len_d1 = len_v3(d1);

  /* Use 'invalid' dist when `center == p1` (after projecting),
   * in this case scale will _never_ move the point in relation to the center,
   * so it makes no sense to take it into account when scaling. see: T46503 */
  return len_d1 != 0.0f ? len_v3(d2) / len_d1 : TRANSFORM_DIST_INVALID;
}

static void ApplySnapResize(TransInfo *t, float vec[3])
{
  float point[3];
  getSnapPoint(t, point);

  float dist = ResizeBetween(t, t->tsnap.snapTarget, point);
  if (dist != TRANSFORM_DIST_INVALID) {
    copy_v3_fl(vec, dist);
  }
}

static void applyResize(TransInfo *t, const int UNUSED(mval[2]))
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

    applySnapping(t, t->values_final);
  }

  size_to_mat3(mat, t->values_final);
  if (t->con.mode & CON_APPLY) {
    t->con.applySize(t, NULL, NULL, mat);

    /* Only so we have re-usable value with redo. */
    float pvec[3] = {0.0f, 0.0f, 0.0f};
    int j = 0;
    for (i = 0; i < 3; i++) {
      if (!(t->con.mode & (CON_AXIS0 << i))) {
        t->values_final[i] = 1.0f;
      }
      else {
        pvec[j++] = t->values_final[i];
      }
    }
    headerResize(t, pvec, str);
  }
  else {
    headerResize(t, t->values_final, str);
  }

  copy_m3_m3(t->mat, mat);  // used in gizmo

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      ElementResize(t, tc, td, mat);
    }
  }

  /* evil hack - redo resize if cliping needed */
  if (t->flag & T_CLIP_UV && clipUVTransform(t, t->values_final, 1)) {
    size_to_mat3(mat, t->values_final);

    if (t->con.mode & CON_APPLY) {
      t->con.applySize(t, NULL, NULL, mat);
    }

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        ElementResize(t, tc, td, mat);
      }

      /* In proportional edit it can happen that */
      /* vertices in the radius of the brush end */
      /* outside the clipping area               */
      /* XXX HACK - dg */
      if (t->flag & T_PROP_EDIT) {
        clipUVData(t);
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initResize(TransInfo *t)
{
  t->mode = TFM_RESIZE;
  t->transform = applyResize;
  t->tsnap.applySnap = ApplySnapResize;
  t->tsnap.distance = ResizeBetween;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->flag |= T_NULL_ONE;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  if ((t->flag & T_EDIT) == 0) {
    t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
    t->num.val_flag[0] |= NUM_NO_ZERO;
    t->num.val_flag[1] |= NUM_NO_ZERO;
    t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
  }

  t->idx_max = 2;
  t->num.idx_max = 2;
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
