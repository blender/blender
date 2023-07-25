/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "DNA_gpencil_legacy_types.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (GPencil Strokes Shrink/Fatten)
 * \{ */

static void applyGPShrinkFatten(TransInfo *t, const int[2] /*mval*/)
{
  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values_final[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);
    SNPRINTF(str, TIP_("Shrink/Fatten: %s"), c);
  }
  else {
    SNPRINTF(str, TIP_("Shrink/Fatten: %3f"), ratio);
  }

  bool recalc = false;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    bGPdata *gpd = static_cast<bGPdata *>(td->ob->data);
    const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
    /* Only recalculate data when in curve edit mode. */
    if (is_curve_edit) {
      recalc = true;
    }

    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival * ratio;
        /* Apply proportional editing. */
        *td->val = interpf(*td->val, td->ival, td->factor);
        if (*td->val <= 0.0f) {
          *td->val = 0.001f;
        }
      }
    }
  }

  if (recalc) {
    recalcData(t);
  }

  ED_area_status_text(t->area, str);
}

static void initGPShrinkFatten(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_GPENCIL_SHRINKFATTEN;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

#ifdef USE_NUM_NO_ZERO
  t->num.val_flag[0] |= NUM_NO_ZERO;
#endif
}

/** \} */

TransModeInfo TransMode_gpshrinkfatten = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initGPShrinkFatten,
    /*transform_fn*/ applyGPShrinkFatten,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
