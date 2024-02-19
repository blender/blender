/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_task.h"

#include "BKE_context.hh"
#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

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

static void transdata_elem_value(const TransInfo * /*t*/,
                                 const TransDataContainer * /*tc*/,
                                 TransData *td,
                                 const float value)
{
  if (td->val == nullptr) {
    return;
  }

  *td->val = td->ival + value * td->factor;
  CLAMP(*td->val, 0.0f, 1.0f);
}

static void transdata_elem_value_fn(void *__restrict iter_data_v,
                                    const int iter,
                                    const TaskParallelTLS *__restrict /*tls*/)
{
  TransDataArgs_Value *data = static_cast<TransDataArgs_Value *>(iter_data_v);
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
      SNPRINTF(str, "%s: +%s %s", value_name, c, t->proptext);
    }
    else {
      SNPRINTF(str, "%s: %s %s", value_name, c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (value >= 0.0f) {
      SNPRINTF(str, "%s: +%.3f %s", value_name, value, t->proptext);
    }
    else {
      SNPRINTF(str, "%s: %.3f %s", value_name, value, t->proptext);
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
      TransDataArgs_Value data{};
      data.t = t;
      data.tc = tc;
      data.value = value;
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_value_fn, &settings);
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void applyCrease(TransInfo *t)
{
  apply_value_impl(t, IFACE_("Crease"));
}

static void applyBevelWeight(TransInfo *t)
{
  apply_value_impl(t, IFACE_("Bevel Weight"));
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
}

static void initEgdeCrease(TransInfo *t, wmOperator * /*op*/)
{
  init_mode_impl(t);
  t->mode = TFM_EDGE_CREASE;
}

static void initVertCrease(TransInfo *t, wmOperator * /*op*/)
{
  init_mode_impl(t);
  t->mode = TFM_VERT_CREASE;
}

static void initBevelWeight(TransInfo *t, wmOperator * /*op*/)
{
  init_mode_impl(t);
  t->mode = TFM_BWEIGHT;
}

/** \} */

TransModeInfo TransMode_edgecrease = {
    /*flags*/ T_NO_CONSTRAINT | T_NO_PROJECT,
    /*init_fn*/ initEgdeCrease,
    /*transform_fn*/ applyCrease,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};

TransModeInfo TransMode_vertcrease = {
    /*flags*/ T_NO_CONSTRAINT | T_NO_PROJECT,
    /*init_fn*/ initVertCrease,
    /*transform_fn*/ applyCrease,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};

TransModeInfo TransMode_bevelweight = {
    /*flags*/ T_NO_CONSTRAINT | T_NO_PROJECT,
    /*init_fn*/ initBevelWeight,
    /*transform_fn*/ applyBevelWeight,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
