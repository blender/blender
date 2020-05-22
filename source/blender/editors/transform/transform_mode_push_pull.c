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
/* Transform (Push/Pull) */

/** \name Transform Push/Pull
 * \{ */

static void applyPushPull(TransInfo *t, const int UNUSED(mval[2]))
{
  float vec[3], axis_global[3];
  float distance;
  int i;
  char str[UI_MAX_DRAW_STR];

  distance = t->values[0];

  snapGridIncrement(t, &distance);

  applyNumInput(&t->num, &distance);

  t->values_final[0] = distance;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("Push/Pull: %s%s %s"), c, t->con.text, t->proptext);
  }
  else {
    /* default header print */
    BLI_snprintf(
        str, sizeof(str), TIP_("Push/Pull: %.4f%s %s"), distance, t->con.text, t->proptext);
  }

  if (t->con.applyRot && t->con.mode & CON_APPLY) {
    t->con.applyRot(t, NULL, NULL, axis_global, NULL);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      sub_v3_v3v3(vec, tc->center_local, td->center);
      if (t->con.applyRot && t->con.mode & CON_APPLY) {
        float axis[3];
        copy_v3_v3(axis, axis_global);
        t->con.applyRot(t, tc, td, axis, NULL);

        mul_m3_v3(td->smtx, axis);
        if (isLockConstraint(t)) {
          float dvec[3];
          project_v3_v3v3(dvec, vec, axis);
          sub_v3_v3(vec, dvec);
        }
        else {
          project_v3_v3v3(vec, vec, axis);
        }
      }
      normalize_v3_length(vec, distance * td->factor);

      add_v3_v3v3(td->loc, td->iloc, vec);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initPushPull(TransInfo *t)
{
  t->mode = TFM_PUSHPULL;
  t->transform = applyPushPull;

  initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 1.0f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_LENGTH;
}
/** \} */
