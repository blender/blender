/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "DNA_gpencil_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "WM_types.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_snap.h"

#include "transform_mode.h"

/* -------------------------------------------------------------------- */
/** \name Transform (Shear) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_Shear {
  const TransInfo *t;
  const TransDataContainer *tc;
  float mat_final[3][3];
  bool is_local_center;
};

static void transdata_elem_shear(const TransInfo *t,
                                 const TransDataContainer *tc,
                                 TransData *td,
                                 const float mat_final[3][3],
                                 const bool is_local_center)
{
  float tmat[3][3];
  const float *center;
  if (t->flag & T_EDIT) {
    mul_m3_series(tmat, td->smtx, mat_final, td->mtx);
  }
  else {
    copy_m3_m3(tmat, mat_final);
  }

  if (is_local_center) {
    center = td->center;
  }
  else {
    center = tc->center_local;
  }

  float vec[3];
  sub_v3_v3v3(vec, td->iloc, center);
  mul_m3_v3(tmat, vec);
  add_v3_v3(vec, center);
  sub_v3_v3(vec, td->iloc);

  if (t->options & CTX_GPENCIL_STROKES) {
    /* Grease pencil multi-frame falloff. */
    bGPDstroke *gps = (bGPDstroke *)td->extra;
    if (gps != NULL) {
      mul_v3_fl(vec, td->factor * gps->runtime.multi_frame_falloff);
    }
    else {
      mul_v3_fl(vec, td->factor);
    }
  }
  else {
    mul_v3_fl(vec, td->factor);
  }

  add_v3_v3v3(td->loc, td->iloc, vec);
}

static void transdata_elem_shear_fn(void *__restrict iter_data_v,
                                    const int iter,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  struct TransDataArgs_Shear *data = iter_data_v;
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_shear(data->t, data->tc, td, data->mat_final, data->is_local_center);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Shear)
 * \{ */

static void initShear_mouseInputMode(TransInfo *t)
{
  float dir[3];
  bool dir_flip = false;
  copy_v3_v3(dir, t->spacemtx[t->orient_axis_ortho]);

  /* Needed for axis aligned view gizmo. */
  if (t->orient[t->orient_curr].type == V3D_ORIENT_VIEW) {
    if (t->orient_axis_ortho == 0) {
      if (t->center2d[1] > t->mouse.imval[1]) {
        dir_flip = !dir_flip;
      }
    }
    else if (t->orient_axis_ortho == 1) {
      if (t->center2d[0] > t->mouse.imval[0]) {
        dir_flip = !dir_flip;
      }
    }
  }

  /* Without this, half the gizmo handles move in the opposite direction. */
  if ((t->orient_axis_ortho + 1) % 3 != t->orient_axis) {
    dir_flip = !dir_flip;
  }

  if (dir_flip) {
    negate_v3(dir);
  }

  mul_mat3_m4_v3(t->viewmat, dir);
  if (normalize_v2(dir) == 0.0f) {
    dir[0] = 1.0f;
  }
  setCustomPointsFromDirection(t, &t->mouse, dir);

  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);
}

static eRedrawFlag handleEventShear(TransInfo *t, const wmEvent *event)
{
  eRedrawFlag status = TREDRAW_NOTHING;

  if (event->type == MIDDLEMOUSE && event->val == KM_PRESS) {
    /* Use custom.mode.data pointer to signal Shear direction */
    do {
      t->orient_axis_ortho = (t->orient_axis_ortho + 1) % 3;
    } while (t->orient_axis_ortho == t->orient_axis);

    initShear_mouseInputMode(t);

    status = TREDRAW_HARD;
  }
  else if (event->type == EVT_XKEY && event->val == KM_PRESS) {
    t->orient_axis_ortho = (t->orient_axis + 1) % 3;
    initShear_mouseInputMode(t);

    status = TREDRAW_HARD;
  }
  else if (event->type == EVT_YKEY && event->val == KM_PRESS) {
    t->orient_axis_ortho = (t->orient_axis + 2) % 3;
    initShear_mouseInputMode(t);

    status = TREDRAW_HARD;
  }

  return status;
}

static void applyShear(TransInfo *t, const int UNUSED(mval[2]))
{
  float smat[3][3], axismat[3][3], axismat_inv[3][3], mat_final[3][3];
  float value;
  int i;
  char str[UI_MAX_DRAW_STR];
  const bool is_local_center = transdata_check_local_center(t, t->around);

  value = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &value);

  applyNumInput(&t->num, &value);

  t->values_final[0] = value;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("Shear: %s %s"), c, t->proptext);
  }
  else {
    /* default header print */
    BLI_snprintf(str,
                 sizeof(str),
                 TIP_("Shear: %.3f %s (Press X or Y to set shear axis)"),
                 value,
                 t->proptext);
  }

  unit_m3(smat);
  smat[1][0] = value;

  copy_v3_v3(axismat_inv[0], t->spacemtx[t->orient_axis_ortho]);
  copy_v3_v3(axismat_inv[2], t->spacemtx[t->orient_axis]);
  cross_v3_v3v3(axismat_inv[1], axismat_inv[0], axismat_inv[2]);
  invert_m3_m3(axismat, axismat_inv);

  mul_m3_series(mat_final, axismat_inv, smat, axismat);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_shear(t, tc, td, mat_final, is_local_center);
      }
    }
    else {
      struct TransDataArgs_Shear data = {
          .t = t,
          .tc = tc,
          .is_local_center = is_local_center,
      };
      copy_m3_m3(data.mat_final, mat_final);

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_shear_fn, &settings);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initShear(TransInfo *t)
{
  t->mode = TFM_SHEAR;
  t->transform = applyShear;
  t->handleEvent = handleEventShear;

  if (t->orient_axis == t->orient_axis_ortho) {
    t->orient_axis = 2;
    t->orient_axis_ortho = 1;
  }

  initShear_mouseInputMode(t);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE; /* Don't think we have any unit here? */

  t->flag |= T_NO_CONSTRAINT;

  transform_mode_default_modal_orientation_set(t, V3D_ORIENT_VIEW);
}

/** \} */
