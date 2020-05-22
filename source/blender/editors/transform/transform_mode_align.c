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

#include "ED_screen.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_mode.h"

/* -------------------------------------------------------------------- */
/* Transform (Align) */

/** \name Transform Align
 * \{ */

static void applyAlign(TransInfo *t, const int UNUSED(mval[2]))
{
  float center[3];
  int i;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    /* saving original center */
    copy_v3_v3(center, tc->center_local);
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float mat[3][3], invmat[3][3];

      if (td->flag & TD_SKIP) {
        continue;
      }

      /* around local centers */
      if (t->flag & (T_OBJECT | T_POSE)) {
        copy_v3_v3(tc->center_local, td->center);
      }
      else {
        if (t->settings->selectmode & SCE_SELECT_FACE) {
          copy_v3_v3(tc->center_local, td->center);
        }
      }

      invert_m3_m3(invmat, td->axismtx);

      mul_m3_m3m3(mat, t->spacemtx, invmat);

      ElementRotation(t, tc, td, mat, t->around);
    }
    /* restoring original center */
    copy_v3_v3(tc->center_local, center);
  }

  recalcData(t);

  ED_area_status_text(t->area, TIP_("Align"));
}

void initAlign(TransInfo *t)
{
  t->flag |= T_NO_CONSTRAINT;

  t->transform = applyAlign;

  initMouseInputMode(t, &t->mouse, INPUT_NONE);
}
/** \} */
