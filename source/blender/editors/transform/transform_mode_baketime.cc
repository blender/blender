/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Bake-Time)
 * \{ */

static void applyBakeTime(TransInfo *t)
{
  float time;
  int i;
  char str[UI_MAX_DRAW_STR];

  float fac = 0.1f;

/* XXX, disable precision for now,
 * this isn't even accessible by the user */
#if 0
  if (t->mouse.precision) {
    /* Calculate ratio for shift-key position, and for total, and blend these for precision. */
    time = float(t->center2d[0] - t->mouse.precision_mval[0]) * fac;
    time += 0.1f * (float(t->center2d[0] * fac - mval[0]) - time);
  }
  else
#endif
  {
    time = (t->center2d[0] - t->mval[0]) * fac;
  }

  transform_snap_increment(t, &time);

  applyNumInput(&t->num, &time);

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (time >= 0.0f) {
      SNPRINTF(str, IFACE_("Time: +%s %s"), c, t->proptext);
    }
    else {
      SNPRINTF(str, IFACE_("Time: %s %s"), c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (time >= 0.0f) {
      SNPRINTF(str, IFACE_("Time: +%.3f %s"), time, t->proptext);
    }
    else {
      SNPRINTF(str, IFACE_("Time: %.3f %s"), time, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      float *dst, ival;
      if (td->val) {
        dst = td->val;
        ival = td->ival;
      }
      else {
        dst = &td->loc[0];
        ival = td->iloc[0];
      }

      *dst = ival + time * td->factor;
      if (td->ext->size && *dst < *td->ext->size) {
        *dst = *td->ext->size;
      }
      if (td->ext->quat && *dst > *td->ext->quat) {
        *dst = *td->ext->quat;
      }
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initBakeTime(TransInfo *t, wmOperator * /*op*/)
{
  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 1.0f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE; /* Don't think this uses units? */
}

/** \} */

TransModeInfo TransMode_baketime = {
    /*flags*/ 0,
    /*init_fn*/ initBakeTime,
    /*transform_fn*/ applyBakeTime,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
