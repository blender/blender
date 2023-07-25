/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "DNA_anim_types.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_nla.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Animation Time Scale)
 * \{ */

static void headerTimeScale(TransInfo *t, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", t->values_final[0]);
  }

  BLI_snprintf(str, UI_MAX_DRAW_STR, TIP_("ScaleX: %s"), &tvec[0]);
}

static void applyTimeScaleValue(TransInfo *t, float value)
{
  Scene *scene = t->scene;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    TransData2D *td2d = tc->data_2d;
    for (int i = 0; i < tc->data_len; i++, td++, td2d++) {
      /* it is assumed that td->extra is a pointer to the AnimData,
       * whose active action is where this keyframe comes from
       * (this is only valid when not in NLA)
       */
      AnimData *adt = static_cast<AnimData *>((t->spacetype != SPACE_NLA) ? td->extra : nullptr);
      float startx = scene->r.cfra;
      float fac = value;

      /* take proportional editing into account */
      fac = ((fac - 1.0f) * td->factor) + 1;

      /* check if any need to apply nla-mapping */
      if (adt) {
        startx = BKE_nla_tweakedit_remap(adt, startx, NLATIME_CONVERT_UNMAP);
      }

      /* now, calculate the new value */
      td->loc[0] = ((td->iloc[0] - startx) * fac) + startx;
    }
  }
}

static void applyTimeScale(TransInfo *t, const int[2] /*mval*/)
{
  char str[UI_MAX_DRAW_STR];

  /* handle numeric-input stuff */
  t->vec[0] = t->values[0];
  applyNumInput(&t->num, &t->vec[0]);
  t->values_final[0] = t->vec[0];
  headerTimeScale(t, str);

  applyTimeScaleValue(t, t->values_final[0]);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

static void initTimeScale(TransInfo *t, wmOperator * /*op*/)
{
  float center[2];

  /* this tool is only really available in the Action Editor
   * AND NLA Editor (for strip scaling)
   */
  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA) == 0) {
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_TIME_SCALE;

  /* recalculate center2d to use scene->r.cfra and mouse Y, since that's
   * what is used in time scale */
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    t->center_global[0] = t->scene->r.cfra;
    projectFloatView(t, t->center_global, center);
    center[1] = t->mouse.imval[1];
  }

  /* force a reinit with the center2d used here */
  initMouseInput(t, &t->mouse, center, t->mouse.imval, false);

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->num.val_flag[0] |= NUM_NULL_ONE;

  /* Numeric-input has max of (n-1). */
  t->idx_max = 0;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  /* Initialize snap like for everything else. */
  t->snap[0] = t->snap[1] = 1.0f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
}

/** \} */

TransModeInfo TransMode_timescale = {
    /*flags*/ T_NULL_ONE,
    /*init_fn*/ initTimeScale,
    /*transform_fn*/ applyTimeScale,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
