/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"

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
/** \name Transform (Bevel Weight) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_BevelWeight {
  const TransInfo *t;
  const TransDataContainer *tc;
  float weight;
};

static void transdata_elem_bevel_weight(const TransInfo *UNUSED(t),
                                        const TransDataContainer *UNUSED(tc),
                                        TransData *td,
                                        const float weight)
{
  if (td->loc == NULL) {
    return;
  }
  *td->loc = td->iloc[0] + weight * td->factor;
  CLAMP(*td->loc, 0.0f, 1.0f);
}

static void transdata_elem_bevel_weight_fn(void *__restrict iter_data_v,
                                           const int iter,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{
  struct TransDataArgs_BevelWeight *data = iter_data_v;
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_bevel_weight(data->t, data->tc, td, data->weight);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Bevel Weight)
 * \{ */

static void applyBevelWeight(TransInfo *t, const int UNUSED(mval[2]))
{
  float weight;
  int i;
  char str[UI_MAX_DRAW_STR];

  weight = t->values[0] + t->values_modal_offset[0];

  CLAMP_MAX(weight, 1.0f);

  transform_snap_increment(t, &weight);

  applyNumInput(&t->num, &weight);

  t->values_final[0] = weight;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (weight >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: +%s %s"), c, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: %s %s"), c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (weight >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: +%.3f %s"), weight, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: %.3f %s"), weight, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_bevel_weight(t, tc, td, weight);
      }
    }
    else {
      struct TransDataArgs_BevelWeight data = {
          .t = t,
          .tc = tc,
          .weight = weight,
      };
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_bevel_weight_fn, &settings);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initBevelWeight(TransInfo *t)
{
  t->mode = TFM_BWEIGHT;
  t->transform = applyBevelWeight;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_DELTA);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

/** \} */
