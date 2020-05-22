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
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Skin) */

/** \name Transform Skin
 * \{ */

static void applySkinResize(TransInfo *t, const int UNUSED(mval[2]))
{
  float mat[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];

  if (t->flag & T_INPUT_IS_VALUES_FINAL) {
    copy_v3_v3(t->values_final, t->values);
  }
  else {
    copy_v3_fl(t->values_final, t->values[0]);

    snapGridIncrement(t, t->values_final);

    if (applyNumInput(&t->num, t->values_final)) {
      constraintNumInput(t, t->values_final);
    }

    applySnapping(t, t->values_final);
  }

  size_to_mat3(mat, t->values_final);

  headerResize(t, t->values_final, str);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float tmat[3][3], smat[3][3];
      float fsize[3];
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (t->flag & T_EDIT) {
        mul_m3_m3m3(smat, mat, td->mtx);
        mul_m3_m3m3(tmat, td->smtx, smat);
      }
      else {
        copy_m3_m3(tmat, mat);
      }

      if (t->con.applySize) {
        t->con.applySize(t, NULL, NULL, tmat);
      }

      mat3_to_size(fsize, tmat);
      td->val[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
      td->val[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initSkinResize(TransInfo *t)
{
  t->mode = TFM_SKIN_RESIZE;
  t->transform = applySkinResize;

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
