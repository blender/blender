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

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation - Trackball) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_Trackball {
  const TransInfo *t;
  const TransDataContainer *tc;
  float axis[3];
  float angle;
  float mat_final[3][3];
};

static void transdata_elem_trackball(const TransInfo *t,
                                     const TransDataContainer *tc,
                                     TransData *td,
                                     const float axis[3],
                                     const float angle,
                                     const float mat_final[3][3])
{
  float mat_buf[3][3];
  const float(*mat)[3] = mat_final;
  if (t->flag & T_PROP_EDIT) {
    axis_angle_normalized_to_mat3(mat_buf, axis, td->factor * angle);
    mat = mat_buf;
  }
  ElementRotation(t, tc, td, mat, t->around);
}

static void transdata_elem_trackball_fn(void *__restrict iter_data_v,
                                        const int iter,
                                        const TaskParallelTLS *__restrict /*tls*/)
{
  TransDataArgs_Trackball *data = static_cast<TransDataArgs_Trackball *>(iter_data_v);
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_trackball(data->t, data->tc, td, data->axis, data->angle, data->mat_final);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation - Trackball)
 * \{ */

static void applyTrackballValue_calc_axis_angle(const TransInfo *t,
                                                const float phi[2],
                                                float r_axis[3],
                                                float *r_angle)
{
  float axis1[3], axis2[3];
  normalize_v3_v3(axis1, t->persinv[0]);
  normalize_v3_v3(axis2, t->persinv[1]);

  mul_v3_v3fl(r_axis, axis1, phi[0]);
  madd_v3_v3fl(r_axis, axis2, phi[1]);
  *r_angle = normalize_v3(r_axis);
}

static void applyTrackballValue(TransInfo *t, const float axis[3], const float angle)
{
  float mat_final[3][3];
  int i;

  axis_angle_normalized_to_mat3(mat_final, axis, angle);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_trackball(t, tc, td, axis, angle, mat_final);
      }
    }
    else {
      TransDataArgs_Trackball data{};
      data.t = t;
      data.tc = tc;
      copy_v3_v3(data.axis, axis);
      data.angle = angle;
      copy_m3_m3(data.mat_final, mat_final);

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_trackball_fn, &settings);
    }
  }
}

static void applyTrackball(TransInfo *t, const int[2] /*mval*/)
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float phi[2];

  copy_v2_v2(phi, t->values);

  transform_snap_increment(t, phi);

  applyNumInput(&t->num, phi);

  copy_v2_v2(t->values_final, phi);

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN * 2];

    outputNumInput(&(t->num), c, &t->scene->unit);

    ofs += BLI_snprintf_rlen(str + ofs,
                             sizeof(str) - ofs,
                             TIP_("Trackball: %s %s %s"),
                             &c[0],
                             &c[NUM_STR_REP_LEN],
                             t->proptext);
  }
  else {
    ofs += BLI_snprintf_rlen(str + ofs,
                             sizeof(str) - ofs,
                             TIP_("Trackball: %.2f %.2f %s"),
                             RAD2DEGF(phi[0]),
                             RAD2DEGF(phi[1]),
                             t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_rlen(
        str + ofs, sizeof(str) - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }

  float axis_final[3], angle_final;
  applyTrackballValue_calc_axis_angle(t, phi, axis_final, &angle_final);
  applyTrackballValue(t, axis_final, angle_final);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

static void applyTrackballMatrix(TransInfo *t, float mat_xform[4][4])
{
  const float phi[2] = {UNPACK2(t->values_final)};

  float axis_final[3], angle_final;
  applyTrackballValue_calc_axis_angle(t, phi, axis_final, &angle_final);

  float mat3[3][3], mat4[4][4];
  axis_angle_normalized_to_mat3(mat3, axis_final, angle_final);

  copy_m4_m3(mat4, mat3);
  transform_pivot_set_m4(mat4, t->center_global);
  mul_m4_m4m4(mat_xform, mat4, mat_xform);
}

static void initTrackball(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_TRACKBALL;

  initMouseInputMode(t, &t->mouse, INPUT_TRACKBALL);

  t->idx_max = 1;
  t->num.idx_max = 1;
  t->snap[0] = DEG2RAD(5.0);
  t->snap[1] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
  t->num.unit_type[1] = B_UNIT_ROTATION;
}

/** \} */

TransModeInfo TransMode_trackball = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initTrackball,
    /*transform_fn*/ applyTrackball,
    /*transform_matrix_fn*/ applyTrackballMatrix,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
