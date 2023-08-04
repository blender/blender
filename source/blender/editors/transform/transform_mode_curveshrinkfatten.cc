/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math.h"
#include "BLI_math_bits.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.hh"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Curve Shrink/Fatten)
 * \{ */

static void applyCurveShrinkFatten(TransInfo *t)
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

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        if (td->ival == 0.0f && ratio > 1.0f) {
          /* Allow Shrink/Fatten for zero radius. */
          *td->val = (ratio - 1.0f) * uint_as_float(POINTER_AS_UINT(t->custom.mode.data));
        }
        else {
          *td->val = td->ival * ratio;
        }

        /* Apply proportional editing. */
        *td->val = interpf(*td->val, td->ival, td->factor);
        CLAMP_MIN(*td->val, 0.0f);
      }
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initCurveShrinkFatten(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_CURVE_SHRINKFATTEN;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  float scale_factor = 0.0f;
  if (((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW) &&
       (t->data_len_all == 1)) ||
      (t->data_len_all == 3 && TRANS_DATA_CONTAINER_FIRST_OK(t)->data[0].val == nullptr))
  {
    /* For cases where only one point on the curve is being transformed and the radius of that
     * point is zero, use the factor to multiply the offset of the ratio and allow scaling.
     * Note that for bezier curves, 3 TransData equals 1 point in most cases. */
    RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
    scale_factor = rv3d->pixsize * t->mouse.factor * t->zfac;
  }
  t->custom.mode.data = POINTER_FROM_UINT(float_as_uint(scale_factor));
}

/** \} */

TransModeInfo TransMode_curveshrinkfatten = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initCurveShrinkFatten,
    /*transform_fn*/ applyCurveShrinkFatten,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
