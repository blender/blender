/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Animation Translation)
 * \{ */

static void headerTimeTranslate(TransInfo *t, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  int ofs = 0;

  /* If numeric input is active, use results from that, otherwise apply snapping to result. */
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    eSnapMode snap_mode = t->tsnap.mode;
    float ival = TRANS_DATA_CONTAINER_FIRST_OK(t)->data->ival;
    float val = ival + t->values_final[0];

    snapFrameTransform(t, snap_mode, ival, val, &val);
    float delta_x = val - ival;

    if (snap_mode == SCE_SNAP_TO_SECOND) {
      /* Convert to seconds. */
      const Scene *scene = t->scene;
      delta_x /= FPS;
      val /= FPS;
    }

    if (snap_mode == SCE_SNAP_TO_FRAME) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.2f (%.4f)", delta_x, val);
    }
    else if (snap_mode == SCE_SNAP_TO_SECOND) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.2f sec (%.4f)", delta_x, val);
    }
    else {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", delta_x);
    }
  }

  ofs += BLI_snprintf_rlen(str, UI_MAX_DRAW_STR, IFACE_("DeltaX: %s"), &tvec[0]);

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_rlen(
        str + ofs, UI_MAX_DRAW_STR - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
  }
}

static void applyTimeTranslateValue(TransInfo *t, const float deltax)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    /* It doesn't matter whether we apply to t->data. */
    TransData *td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      float *dst, ival;
      if (td->val) {
        dst = td->val;
        ival = td->ival;
      }
      else {
        dst = &td->loc[0];
        ival = td->iloc[0];
      }

      *dst = ival + deltax * td->factor;
    }
  }
}

static void applyTimeTranslate(TransInfo *t)
{
  View2D *v2d = (View2D *)t->view;
  char str[UI_MAX_DRAW_STR];

  /* Calculate translation amount from mouse movement - in 'time-grid space'. */
  if (t->flag & T_MODAL) {
    float cval[2], sval[2];
    UI_view2d_region_to_view(v2d, t->mval[0], t->mval[0], &cval[0], &cval[1]);
    UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[0], &sval[0], &sval[1]);

    /* We only need to calculate effect for time (#applyTimeTranslate only needs that). */
    t->values[0] = cval[0] - sval[0];
  }

  /* Handle numeric-input stuff. */
  t->vec[0] = t->values[0];
  applyNumInput(&t->num, &t->vec[0]);
  t->values_final[0] = t->vec[0];
  headerTimeTranslate(t, str);

  applyTimeTranslateValue(t, t->values_final[0]);

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initTimeTranslate(TransInfo *t, wmOperator * /*op*/)
{
  /* This tool is only really available in the Action Editor. */
  if (!ELEM(t->spacetype, SPACE_ACTION, SPACE_SEQ)) {
    t->state = TRANS_CANCEL;
  }

  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  /* Numeric-input has max of (n-1). */
  t->idx_max = 0;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  /* Initialize snap like for everything else. */
  t->snap[0] = t->snap[1] = 1.0f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  /* No time unit supporting frames currently. */
  t->num.unit_type[0] = B_UNIT_NONE;
}

/** \} */

TransModeInfo TransMode_timetranslate = {
    /*flags*/ 0,
    /*init_fn*/ initTimeTranslate,
    /*transform_fn*/ applyTimeTranslate,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
