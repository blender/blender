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

#include "DNA_anim_types.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Animation Translation) */

/** \name Transform Animation Translation
 * \{ */

static void headerTimeTranslate(TransInfo *t, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  int ofs = 0;

  /* if numeric input is active, use results from that, otherwise apply snapping to result */
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    const short autosnap = getAnimEdit_SnapMode(t);
    float val = t->values_final[0];

    float snap_val;
    snapFrameTransform(t, autosnap, false, val, &snap_val);

    if (autosnap == SACTSNAP_FRAME) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.2f (%.4f)", snap_val, val);
    }
    else if (autosnap == SACTSNAP_SECOND) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.2f sec (%.4f)", snap_val, val);
    }
    else if (autosnap == SACTSNAP_TSTEP) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f sec", snap_val);
    }
    else {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", snap_val);
    }
  }

  ofs += BLI_snprintf(str, UI_MAX_DRAW_STR, TIP_("DeltaX: %s"), &tvec[0]);

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

static void applyTimeTranslateValue(TransInfo *t, const float deltax)
{
  const short autosnap = getAnimEdit_SnapMode(t);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    TransData2D *td2d = tc->data_2d;
    /* It doesn't matter whether we apply to t->data or
     * t->data2d, but t->data2d is more convenient. */
    for (int i = 0; i < tc->data_len; i++, td++, td2d++) {
      /* It is assumed that td->extra is a pointer to the AnimData,
       * whose active action is where this keyframe comes from.
       * (this is only valid when not in NLA)
       * (also: masks and gpencil dont have animadata)
       */
      AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;

      /* apply nearest snapping */
      *(td->val) = td->ival + deltax * td->factor;
      doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
    }
  }
}

static void applyTimeTranslate(TransInfo *t, const int mval[2])
{
  View2D *v2d = (View2D *)t->view;
  char str[UI_MAX_DRAW_STR];

  /* calculate translation amount from mouse movement - in 'time-grid space' */
  if (t->flag & T_MODAL) {
    float cval[2], sval[2];
    UI_view2d_region_to_view(v2d, mval[0], mval[0], &cval[0], &cval[1]);
    UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[0], &sval[0], &sval[1]);

    /* we only need to calculate effect for time (applyTimeTranslate only needs that) */
    t->values[0] = cval[0] - sval[0];
  }

  /* handle numeric-input stuff */
  t->vec[0] = t->values[0];
  applyNumInput(&t->num, &t->vec[0]);
  t->values_final[0] = t->vec[0];
  headerTimeTranslate(t, str);

  applyTimeTranslateValue(t, t->values_final[0]);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initTimeTranslate(TransInfo *t)
{
  /* this tool is only really available in the Action Editor... */
  if (!ELEM(t->spacetype, SPACE_ACTION, SPACE_SEQ)) {
    t->state = TRANS_CANCEL;
  }

  t->transform = applyTimeTranslate;

  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  /* num-input has max of (n-1) */
  t->idx_max = 0;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  /* initialize snap like for everything else */
  t->snap[0] = 0.0f;
  t->snap[1] = t->snap[2] = 1.0f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  /* No time unit supporting frames currently... */
  t->num.unit_type[0] = B_UNIT_NONE;
}
/** \} */
