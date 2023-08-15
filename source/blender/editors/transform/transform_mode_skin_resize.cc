/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Skin) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_SkinResize {
  const TransInfo *t;
  const TransDataContainer *tc;
  float mat_final[3][3];
};

static void transdata_elem_skin_resize(const TransInfo *t,
                                       const TransDataContainer * /*tc*/,
                                       TransData *td,
                                       const float mat[3][3])
{
  float tmat[3][3], smat[3][3];
  float fsize[3];

  if (t->flag & T_EDIT) {
    mul_m3_m3m3(smat, mat, td->mtx);
    mul_m3_m3m3(tmat, td->smtx, smat);
  }
  else {
    copy_m3_m3(tmat, mat);
  }

  if (t->con.applySize) {
    t->con.applySize(t, nullptr, nullptr, tmat);
  }

  mat3_to_size(fsize, tmat);
  td->loc[0] = td->iloc[0] * (1 + (fsize[0] - 1) * td->factor);
  td->loc[1] = td->iloc[1] * (1 + (fsize[1] - 1) * td->factor);
}

static void transdata_elem_skin_resize_fn(void *__restrict iter_data_v,
                                          const int iter,
                                          const TaskParallelTLS *__restrict /*tls*/)
{
  TransDataArgs_SkinResize *data = static_cast<TransDataArgs_SkinResize *>(iter_data_v);
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_skin_resize(data->t, data->tc, td, data->mat_final);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Skin)
 * \{ */

static void applySkinResize(TransInfo *t)
{
  float mat_final[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];

  if (t->flag & T_INPUT_IS_VALUES_FINAL) {
    copy_v3_v3(t->values_final, t->values);
  }
  else {
    copy_v3_fl(t->values_final, t->values[0]);
    add_v3_v3(t->values_final, t->values_modal_offset);

    transform_snap_increment(t, t->values_final);

    if (applyNumInput(&t->num, t->values_final)) {
      constraintNumInput(t, t->values_final);
    }

    transform_snap_mixed_apply(t, t->values_final);
  }

  size_to_mat3(mat_final, t->values_final);

  headerResize(t, t->values_final, str, sizeof(str));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_skin_resize(t, tc, td, mat_final);
      }
    }
    else {
      TransDataArgs_SkinResize data{};
      data.t = t;
      data.tc = tc;
      copy_m3_m3(data.mat_final, mat_final);
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_skin_resize_fn, &settings);
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initSkinResize(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_SKIN_RESIZE;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->flag |= T_NULL_ONE;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  if ((t->flag & T_EDIT) == 0) {
#ifdef USE_NUM_NO_ZERO
    t->num.val_flag[0] |= NUM_NO_ZERO;
    t->num.val_flag[1] |= NUM_NO_ZERO;
    t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
  }

  t->idx_max = 2;
  t->num.idx_max = 2;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
  t->num.unit_type[2] = B_UNIT_NONE;
}

/** \} */

TransModeInfo TransMode_skinresize = {
    /*flags*/ 0,
    /*init_fn*/ initSkinResize,
    /*transform_fn*/ applySkinResize,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
