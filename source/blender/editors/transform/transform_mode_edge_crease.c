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
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Crease) */

/** \name Transform Crease
 * \{ */

static void applyCrease(TransInfo *t, const int UNUSED(mval[2]))
{
  float crease;
  int i;
  char str[UI_MAX_DRAW_STR];

  crease = t->values[0];

  CLAMP_MAX(crease, 1.0f);

  snapGridIncrement(t, &crease);

  applyNumInput(&t->num, &crease);

  t->values_final[0] = crease;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (crease >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: +%s %s"), c, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: %s %s"), c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (crease >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: +%.3f %s"), crease, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: %.3f %s"), crease, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival + crease * td->factor;
        if (*td->val < 0.0f) {
          *td->val = 0.0f;
        }
        if (*td->val > 1.0f) {
          *td->val = 1.0f;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initCrease(TransInfo *t)
{
  t->mode = TFM_CREASE;
  t->transform = applyCrease;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_DELTA);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}
/** \} */
