/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <algorithm>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_nla.h"
#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_convert.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Animation Time Slide)
 * \{ */

static void headerTimeSlide(TransInfo *t, const float sval, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    const float *range = static_cast<const float *>(t->custom.mode.data);
    float minx = range[0];
    float maxx = range[1];
    float cval = t->values_final[0];
    float val;

    val = 2.0f * (cval - sval) / (maxx - minx);
    CLAMP(val, -1.0f, 1.0f);

    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", val);
  }

  BLI_snprintf(str, UI_MAX_DRAW_STR, IFACE_("TimeSlide: %s"), &tvec[0]);
}

static void applyTimeSlideValue(TransInfo *t, float sval, float cval)
{
  int i;
  const float *range = static_cast<const float *>(t->custom.mode.data);
  float minx = range[0];
  float maxx = range[1];

  /* Set value for drawing black line. */
  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;
    saction->timeslide = cval;
  }

  /* It doesn't matter whether we apply to t->data or
   * t->data2d, but t->data2d is more convenient. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      /* It is assumed that td->extra is a pointer to the AnimData,
       * whose active action is where this keyframe comes from
       * (this is only valid when not in NLA). */
      AnimData *adt = static_cast<AnimData *>((t->spacetype != SPACE_NLA) ? td->extra : nullptr);

      /* Only apply to data if in range. */
      if ((sval > minx) && (sval < maxx)) {
        float cvalc = std::clamp(cval, minx, maxx);
        float timefac;
        float *dst;
        float ival;

        if (td->val) {
          dst = td->val;
          ival = td->ival;
        }
        else {
          dst = &td->loc[0];
          ival = td->iloc[0];
        }

        /* NLA mapping magic here works as follows:
         * - `ival` goes from strip time to global time.
         * - Calculation is performed into `td->val` in global time
         *   (since `sval` and min/max are all in global time).
         * - `td->val` then gets put back into strip time.
         */
        if (adt) {
          /* Strip to global. */
          ival = BKE_nla_tweakedit_remap(adt, ival, NLATIME_CONVERT_MAP);
        }

        /* Left half? */
        if (ival < sval) {
          timefac = (sval - ival) / (sval - minx);
          *dst = cvalc - timefac * (cvalc - minx);
        }
        else {
          timefac = (ival - sval) / (maxx - sval);
          *dst = cvalc + timefac * (maxx - cvalc);
        }

        if (adt) {
          /* Global to strip. */
          *dst = BKE_nla_tweakedit_remap(adt, *dst, NLATIME_CONVERT_UNMAP);
        }
      }
    }
  }
}

static void applyTimeSlide(TransInfo *t)
{
  View2D *v2d = (View2D *)t->view;
  float cval[2], sval[2];
  const float *range = static_cast<const float *>(t->custom.mode.data);
  float minx = range[0];
  float maxx = range[1];
  char str[UI_MAX_DRAW_STR];

  /* Calculate mouse co-ordinates. */
  UI_view2d_region_to_view(v2d, t->mval[0], t->mval[1], &cval[0], &cval[1]);
  UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[1], &sval[0], &sval[1]);

  /* `t->values_final[0]` stores `cval[0]`,
   * which is the current mouse-pointer location (in frames). */
  /* XXX Need to be able to repeat this. */
  // t->values_final[0] = cval[0]; /* UNUSED (reset again later). */

  /* Handle numeric-input stuff. */
  t->vec[0] = 2.0f * (cval[0] - sval[0]) / (maxx - minx);
  applyNumInput(&t->num, &t->vec[0]);
  t->values_final[0] = (maxx - minx) * t->vec[0] / 2.0f + sval[0];

  headerTimeSlide(t, sval[0], str);
  applyTimeSlideValue(t, sval[0], t->values_final[0]);

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initTimeSlide(TransInfo *t, wmOperator * /*op*/)
{
  /* This tool is only really available in the Action Editor. */
  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;

    /* Set flag for drawing stuff. */
    saction->flag |= SACTION_MOVING;
  }
  else {
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_TIME_SLIDE;

  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  {
    Scene *scene = t->scene;
    float *range;
    t->custom.mode.data = range = static_cast<float *>(
        MEM_mallocN(sizeof(float[2]), "TimeSlide Min/Max"));
    t->custom.mode.use_free = true;

    float min = 999999999.0f, max = -999999999.0f;
    int i;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        AnimData *adt = static_cast<AnimData *>((t->spacetype != SPACE_NLA) ? td->extra : nullptr);
        float val = *(td->val);

        /* Strip/action time to global (mapped) time. */
        if (adt) {
          val = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_MAP);
        }

        if (min > val) {
          min = val;
        }
        if (max < val) {
          max = val;
        }
      }
    }

    if (min == max) {
      /* Just use the current frame ranges. */
      min = float(PSFRA);
      max = float(PEFRA);
    }

    range[0] = min;
    range[1] = max;
  }

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

TransModeInfo TransMode_timeslide = {
    /*flags*/ T_NULL_ONE,
    /*init_fn*/ initTimeSlide,
    /*transform_fn*/ applyTimeSlide,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
