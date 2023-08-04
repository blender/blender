/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"

#include "MEM_guardedalloc.h"

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
/** \name To Sphere Utilities
 * \{ */

struct ToSphereInfo {
  float prop_size_prev;
  float radius;
};

/** Calculate average radius. */
static void to_sphere_radius_update(TransInfo *t)
{
  ToSphereInfo *data = static_cast<ToSphereInfo *>(t->custom.mode.data);
  float radius = 0.0f;
  float vec[3];

  const bool is_local_center = transdata_check_local_center(t, t->around);
  const bool is_data_space = (t->options & CTX_POSE_BONE) != 0;

  if (t->flag & T_PROP_EDIT_ALL) {
    int factor_accum = 0.0f;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
        if (td->factor == 0.0f) {
          continue;
        }
        const float *center = is_local_center ? td->center : tc->center_local;
        if (is_data_space) {
          copy_v3_v3(vec, td->center);
        }
        else {
          copy_v3_v3(vec, td->iloc);
        }

        sub_v3_v3(vec, center);
        radius += td->factor * len_v3(vec);
        factor_accum += td->factor;
      }
    }
    if (factor_accum != 0.0f) {
      radius /= factor_accum;
    }
  }
  else {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
        const float *center = is_local_center ? td->center : tc->center_local;
        if (is_data_space) {
          copy_v3_v3(vec, td->center);
        }
        else {
          copy_v3_v3(vec, td->iloc);
        }

        sub_v3_v3(vec, center);
        radius += len_v3(vec);
      }
    }
    radius /= float(t->data_len_all);
  }

  data->prop_size_prev = t->prop_size;
  data->radius = radius;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (ToSphere) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_ToSphere {
  const TransInfo *t;
  const TransDataContainer *tc;
  float ratio;
  ToSphereInfo to_sphere_info;
  bool is_local_center;
  bool is_data_space;
};

static void transdata_elem_to_sphere(const TransInfo * /*t*/,
                                     const TransDataContainer *tc,
                                     TransData *td,
                                     const float ratio,
                                     const ToSphereInfo *to_sphere_info,
                                     const bool is_local_center,
                                     const bool is_data_space)
{
  float vec[3];
  const float *center = is_local_center ? td->center : tc->center_local;
  if (is_data_space) {
    copy_v3_v3(vec, td->center);
  }
  else {
    copy_v3_v3(vec, td->iloc);
  }

  sub_v3_v3(vec, center);
  const float radius = normalize_v3(vec);
  const float tratio = ratio * td->factor;
  mul_v3_fl(vec, radius * (1.0f - tratio) + to_sphere_info->radius * tratio);
  add_v3_v3(vec, center);

  if (is_data_space) {
    sub_v3_v3(vec, td->center);
    mul_m3_v3(td->smtx, vec);
    add_v3_v3(vec, td->iloc);
  }

  copy_v3_v3(td->loc, vec);
}

static void transdata_elem_to_sphere_fn(void *__restrict iter_data_v,
                                        const int iter,
                                        const TaskParallelTLS *__restrict /*tls*/)
{
  TransDataArgs_ToSphere *data = static_cast<TransDataArgs_ToSphere *>(iter_data_v);
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_to_sphere(data->t,
                           data->tc,
                           td,
                           data->ratio,
                           &data->to_sphere_info,
                           data->is_local_center,
                           data->is_data_space);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (ToSphere)
 * \{ */

static void applyToSphere(TransInfo *t)
{
  const bool is_local_center = transdata_check_local_center(t, t->around);
  const bool is_data_space = (t->options & CTX_POSE_BONE) != 0;

  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &ratio);

  applyNumInput(&t->num, &ratio);

  CLAMP(ratio, 0.0f, 1.0f);

  t->values_final[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    SNPRINTF(str, TIP_("To Sphere: %s %s"), c, t->proptext);
  }
  else {
    /* default header print */
    SNPRINTF(str, TIP_("To Sphere: %.4f %s"), ratio, t->proptext);
  }

  const ToSphereInfo *to_sphere_info = static_cast<const ToSphereInfo *>(t->custom.mode.data);
  if (to_sphere_info->prop_size_prev != t->prop_size) {
    to_sphere_radius_update(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_to_sphere(t, tc, td, ratio, to_sphere_info, is_local_center, is_data_space);
      }
    }
    else {
      TransDataArgs_ToSphere data{};
      data.t = t;
      data.tc = tc;
      data.ratio = ratio;
      data.to_sphere_info = *to_sphere_info;
      data.is_local_center = is_local_center;
      data.is_data_space = is_data_space;

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_to_sphere_fn, &settings);
    }
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initToSphere(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_TOSPHERE;

  initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_RATIO);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->num.val_flag[0] |= NUM_NULL_ONE | NUM_NO_NEGATIVE;

  ToSphereInfo *data = static_cast<ToSphereInfo *>(MEM_callocN(sizeof(*data), __func__));
  t->custom.mode.data = data;
  t->custom.mode.use_free = true;

  to_sphere_radius_update(t);
}

/** \} */

TransModeInfo TransMode_tosphere = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initToSphere,
    /*transform_fn*/ applyToSphere,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
