/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Tilt)
 * \{ */

static void applyTilt(TransInfo *t, const int[2] /*mval*/)
{
  int i;
  char str[UI_MAX_DRAW_STR];

  float final;

  final = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &final);

  applyNumInput(&t->num, &final);

  t->values_final[0] = final;

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    SNPRINTF(str, TIP_("Tilt: %s" BLI_STR_UTF8_DEGREE_SIGN " %s"), &c[0], t->proptext);

    /* XXX For some reason, this seems needed for this op, else RNA prop is not updated... :/ */
    t->values_final[0] = final;
  }
  else {
    SNPRINTF(str, TIP_("Tilt: %.2f" BLI_STR_UTF8_DEGREE_SIGN " %s"), RAD2DEGF(final), t->proptext);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival + final * td->factor;
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

static void initTilt(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_TILT;

  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = DEG2RAD(5.0);
  t->snap[1] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
}

/** \} */

TransModeInfo TransMode_tilt = {
    /*flags*/ T_NO_CONSTRAINT | T_NO_PROJECT,
    /*init_fn*/ initTilt,
    /*transform_fn*/ applyTilt,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
