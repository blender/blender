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

#include "transform.h"
#include "transform_convert.h"
#include "transform_snap.h"

#include "transform_mode.h"

/* -------------------------------------------------------------------- */
/** \name Transform (EditBone Roll)
 * \{ */

static void applyBoneRoll(TransInfo *t, const int UNUSED(mval[2]))
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

    SNPRINTF(str, TIP_("Roll: %s"), &c[0]);
  }
  else {
    SNPRINTF(str, TIP_("Roll: %.2f"), RAD2DEGF(final));
  }

  /* set roll values */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      *(td->val) = td->ival - final;
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

static void initBoneRoll(TransInfo *t, struct wmOperator *UNUSED(op))
{
  t->mode = TFM_BONE_ROLL;

  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = DEG2RAD(5.0);
  t->snap[1] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
}

/** \} */

TransModeInfo TransMode_boneroll = {
    /*flags*/ T_NO_CONSTRAINT | T_NO_PROJECT,
    /*init_fn*/ initBoneRoll,
    /*transform_fn*/ applyBoneRoll,
    /*transform_matrix_fn*/ NULL,
    /*handle_event_fn*/ NULL,
    /*snap_distance_fn*/ NULL,
    /*snap_apply_fn*/ NULL,
    /*draw_fn*/ NULL,
};
