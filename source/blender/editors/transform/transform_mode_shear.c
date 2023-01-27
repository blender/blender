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

static void apply_shear_value(TransInfo *t, const float value)
{
  float smat[3][3];
  unit_m3(smat);
  smat[1][0] = value;

  float axismat_inv[3][3];
  copy_v3_v3(axismat_inv[0], t->spacemtx[t->orient_axis_ortho]);
  copy_v3_v3(axismat_inv[2], t->spacemtx[t->orient_axis]);
  cross_v3_v3v3(axismat_inv[1], axismat_inv[0], axismat_inv[2]);
  float axismat[3][3];
  invert_m3_m3(axismat, axismat_inv);

  float mat_final[3][3];
  mul_m3_series(mat_final, axismat_inv, smat, axismat);

  const bool is_local_center = transdata_check_local_center(t, t->around);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
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
}

static bool uv_shear_in_clip_bounds_test(const TransInfo *t, const float value)
{
  const int axis = t->orient_axis_ortho;
  if (axis < 0 || 1 < axis) {
    return true; /* Non standard axis, nothing to do. */
  }
  const float *center = t->center_global;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }
      if (td->factor < 1.0f) {
        continue; /* Proportional edit, will get picked up in next phase. */
      }

      float uv[2];
      sub_v2_v2v2(uv, td->iloc, center);
      uv[axis] = uv[axis] + value * uv[1 - axis] * (2 * axis - 1);
      add_v2_v2(uv, center);
      /* TODO: UDIM support. */
      if (uv[axis] < 0.0f || 1.0f < uv[axis]) {
        return false;
      }
    }
  }
  return true;
}

static bool clip_uv_transform_shear(const TransInfo *t, float *vec, float *vec_inside_bounds)
{
  float value = vec[0];
  if (uv_shear_in_clip_bounds_test(t, value)) {
    vec_inside_bounds[0] = value; /* Store for next iteration. */
    return false;                 /* Nothing to do. */
  }
  float value_inside_bounds = vec_inside_bounds[0];
  if (!uv_shear_in_clip_bounds_test(t, value_inside_bounds)) {
    return false; /* No known way to fix, may as well shear anyway. */
  }
  const int max_i = 32; /* Limit iteration, mainly for debugging. */
  for (int i = 0; i < max_i; i++) {
    /* Binary search. */
    const float value_mid = (value_inside_bounds + value) / 2.0f;
    if (ELEM(value_mid, value_inside_bounds, value)) {
      break; /* float precision reached. */
    }
    if (uv_shear_in_clip_bounds_test(t, value_mid)) {
      value_inside_bounds = value_mid;
    }
    else {
      value = value_mid;
    }
  }

  vec_inside_bounds[0] = value_inside_bounds; /* Store for next iteration. */
  vec[0] = value_inside_bounds;               /* Update shear value. */
  return true;
}

static void apply_shear(TransInfo *t, const int UNUSED(mval[2]))
{
  float value = t->values[0] + t->values_modal_offset[0];
  transform_snap_increment(t, &value);
  applyNumInput(&t->num, &value);
  t->values_final[0] = value;

  apply_shear_value(t, value);

  if (t->flag & T_CLIP_UV) {
    if (clip_uv_transform_shear(t, t->values_final, t->values_inside_constraints)) {
      apply_shear_value(t, t->values_final[0]);
    }

    /* In proportional edit it can happen that */
    /* vertices in the radius of the brush end */
    /* outside the clipping area               */
    /* XXX HACK - dg */
    if (t->flag & T_PROP_EDIT) {
      clipUVData(t);
    }
  }

  recalcData(t);

  char str[UI_MAX_DRAW_STR];
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

  ED_area_status_text(t->area, str);
}

void initShear(TransInfo *t)
{
  t->mode = TFM_SHEAR;
  t->transform = apply_shear;
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
