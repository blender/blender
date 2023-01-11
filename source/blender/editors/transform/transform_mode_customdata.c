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
/** \name Transform Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_Value {
  const TransInfo *t;
  const TransDataContainer *tc;
  float value;
};

static void transdata_elem_value(const TransInfo *UNUSED(t),
                                 const TransDataContainer *UNUSED(tc),
                                 TransData *td,
                                 const float value)
{
  if (td->val == NULL) {
    return;
  }

  *td->val = td->ival + value * td->factor;
  CLAMP(*td->val, 0.0f, 1.0f);
}

static void transdata_elem_value_fn(void *__restrict iter_data_v,
                                    const int iter,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  struct TransDataArgs_Value *data = iter_data_v;
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_value(data->t, data->tc, td, data->value);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Value
 * \{ */

static void apply_value_impl(TransInfo *t, const char *value_name)
{
  float value;
  int i;
  char str[UI_MAX_DRAW_STR];

  value = t->values[0] + t->values_modal_offset[0];

  CLAMP_MAX(value, 1.0f);

  transform_snap_increment(t, &value);

  applyNumInput(&t->num, &value);

  t->values_final[0] = value;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (value >= 0.0f) {
      BLI_snprintf(str, sizeof(str), "%s: +%s %s", value_name, c, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), "%s: %s %s", value_name, c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (value >= 0.0f) {
      BLI_snprintf(str, sizeof(str), "%s: +%.3f %s", value_name, value, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), "%s: %.3f %s", value_name, value, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_value(t, tc, td, value);
      }
    }
    else {
      struct TransDataArgs_Value data = {
          .t = t,
          .tc = tc,
          .value = value,
      };
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_value_fn, &settings);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

static void applyCrease(TransInfo *t, const int UNUSED(mval[2]))
{
  apply_value_impl(t, TIP_("Crease"));
}

static void applyBevelWeight(TransInfo *t, const int UNUSED(mval[2]))
{
  apply_value_impl(t, TIP_("Bevel Weight"));
}

static void init_mode_impl(TransInfo *t)
{
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

void initEgdeCrease(TransInfo *t)
{
  init_mode_impl(t);
  t->mode = TFM_EDGE_CREASE;
  t->transform = applyCrease;
}

void initVertCrease(TransInfo *t)
{
  init_mode_impl(t);
  t->mode = TFM_VERT_CREASE;
  t->transform = applyCrease;
}

void initBevelWeight(TransInfo *t)
{
  init_mode_impl(t);
  t->mode = TFM_BWEIGHT;
  t->transform = applyBevelWeight;
}

/** \} */
