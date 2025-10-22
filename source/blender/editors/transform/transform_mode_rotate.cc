/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_context.hh"
#include "BKE_report.hh"
#include "BKE_unit.hh"

#include "BLT_translation.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation) Matrix Cache
 * \{ */

struct RotateMatrixCache {
  /**
   * Counter for needed updates (when we need to update to non-default matrix,
   * we also need another update on next iteration to go back to default matrix,
   * hence the '2' value used here, instead of a mere boolean).
   */
  short do_update_matrix;
  float mat[3][3];
};

static void rmat_cache_init(RotateMatrixCache *rmc, const float angle, const float axis[3])
{
  axis_angle_normalized_to_mat3(rmc->mat, axis, angle);
  rmc->do_update_matrix = 0;
}

static void rmat_cache_reset(RotateMatrixCache *rmc)
{
  rmc->do_update_matrix = 2;
}

static void rmat_cache_update(RotateMatrixCache *rmc, const float axis[3], const float angle)
{
  if (rmc->do_update_matrix > 0) {
    axis_angle_normalized_to_mat3(rmc->mat, axis, angle);
    rmc->do_update_matrix--;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation) Element
 * \{ */

static void transdata_elem_rotate(const TransInfo *t,
                                  const TransDataContainer *tc,
                                  TransData *td,
                                  TransDataExtension *td_ext,
                                  const float axis[3],
                                  const float angle,
                                  const float angle_step,
                                  const bool is_large_rotation,
                                  RotateMatrixCache *rmc)
{
  float axis_buffer[3];
  const float *axis_final = axis;

  float angle_final = angle;
  if (t->con.applyRot) {
    copy_v3_v3(axis_buffer, axis);
    axis_final = axis_buffer;
    t->con.applyRot(t, tc, td, axis_buffer);
    angle_final = angle * td->factor;
    /* Even though final angle might be identical to orig value,
     * we have to update the rotation matrix in that case... */
    rmat_cache_reset(rmc);
  }
  else if (t->flag & T_PROP_EDIT) {
    angle_final = angle * td->factor;
  }

  /* Rotation is very likely to be above 180 degrees we need to do rotation by steps.
   * Note that this is only needed when doing 'absolute' rotation
   * (i.e. from initial rotation again, typically when using numinput).
   * regular incremental rotation (from mouse/widget/...) will be called often enough,
   * hence steps are small enough to be properly handled without that complicated trick.
   * Note that we can only do that kind of stepped rotation if we have initial rotation values
   * (and access to some actual rotation value storage).
   * Otherwise, just assume it's useless (e.g. in case of mesh/UV/etc. editing).
   * Also need to be in Euler rotation mode, the others never allow more than one turn anyway.
   */
  if (is_large_rotation && td_ext != nullptr && td_ext->rotOrder == ROT_MODE_EUL) {
    copy_v3_v3(td_ext->rot, td_ext->irot);
    for (float angle_progress = angle_step; fabsf(angle_progress) < fabsf(angle_final);
         angle_progress += angle_step)
    {
      axis_angle_normalized_to_mat3(rmc->mat, axis_final, angle_progress);
      ElementRotation(t, tc, td, td_ext, rmc->mat, t->around);
    }
    rmat_cache_reset(rmc);
  }
  else if (angle_final != angle) {
    rmat_cache_reset(rmc);
  }

  rmat_cache_update(rmc, axis_final, angle_final);

  ElementRotation(t, tc, td, td_ext, rmc->mat, t->around);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation)
 * \{ */

static float RotationBetween(TransInfo *t, const float p1[3], const float p2[3])
{
  float angle, start[3], end[3];

  sub_v3_v3v3(start, p1, t->center_global);
  sub_v3_v3v3(end, p2, t->center_global);

  /* Angle around a constraint axis (error prone, will need debug). */
  if (t->con.applyRot != nullptr && (t->con.mode & CON_APPLY)) {
    float axis[3];

    t->con.applyRot(t, nullptr, nullptr, axis);

    angle = -angle_signed_on_axis_v3v3_v3(start, end, axis);
  }
  else {
    float mtx[3][3];

    copy_m3_m4(mtx, t->viewmat);

    mul_m3_v3(mtx, end);
    mul_m3_v3(mtx, start);

    angle = -(atan2f(start[1], start[0]) - atan2f(end[1], end[0]));
  }

  if (angle > float(M_PI)) {
    angle = angle - 2 * float(M_PI);
  }
  else if (angle < -float(M_PI)) {
    angle = 2.0f * float(M_PI) + angle;
  }

  return angle;
}

static void ApplySnapRotation(TransInfo *t, float *value)
{
  float point[3];
  getSnapPoint(t, point);

  float dist = RotationBetween(t, t->tsnap.snap_source, point);
  *value = dist;
}

static float large_rotation_limit(float angle)
{
  /* Limit rotation to 1001 turns max
   * (otherwise iterative handling of 'large' rotations would become too slow). */
  const float angle_max = float(M_PI * 2000.0);
  if (fabsf(angle) > angle_max) {
    const float angle_sign = angle < 0.0f ? -1.0f : 1.0f;
    angle = angle_sign * (fmodf(fabsf(angle), float(M_PI * 2.0)) + angle_max);
  }
  return angle;
}

static void applyRotationValue(TransInfo *t,
                               float angle,
                               const float axis[3],
                               const bool is_large_rotation)
{
  const float angle_sign = angle < 0.0f ? -1.0f : 1.0f;
  /* We cannot use something too close to 180 degrees, or 'continuous' rotation may fail
   * due to computing error. */
  const float angle_step = angle_sign * float(0.9 * M_PI);

  if (is_large_rotation) {
    /* Just in case, calling code should have already done that in practice
     * (for UI feedback reasons). */
    angle = large_rotation_limit(angle);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    threading::parallel_for(IndexRange(tc->data_len), 1024, [&](const IndexRange range) {
      RotateMatrixCache rmc = {0};
      rmat_cache_init(&rmc, angle, axis);
      for (const int i : range) {
        TransData *td = &tc->data[i];
        TransDataExtension *td_ext = tc->data_ext ? &tc->data_ext[i] : nullptr;
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_rotate(t, tc, td, td_ext, axis, angle, angle_step, is_large_rotation, &rmc);
      }
    });
  }
}

static bool uv_rotation_in_clip_bounds_test(const TransInfo *t, const float angle)
{
  const float cos_angle = cosf(angle);
  const float sin_angle = sinf(angle);
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
      float pr[2];
      pr[0] = cos_angle * uv[0] + sin_angle * uv[1];
      pr[1] = -sin_angle * uv[0] + cos_angle * uv[1];
      add_v2_v2(pr, center);
      /* TODO: UDIM support. */
      if (pr[0] < 0.0f || 1.0f < pr[0]) {
        return false;
      }
      if (pr[1] < 0.0f || 1.0f < pr[1]) {
        return false;
      }
    }
  }
  return true;
}

static bool clip_uv_transform_rotate(const TransInfo *t, float *vec, float *vec_inside_bounds)
{
  float angle = vec[0];
  if (uv_rotation_in_clip_bounds_test(t, angle)) {
    vec_inside_bounds[0] = angle; /* Store for next iteration. */
    return false;                 /* Nothing to do. */
  }
  float angle_inside_bounds = vec_inside_bounds[0];
  if (!uv_rotation_in_clip_bounds_test(t, angle_inside_bounds)) {
    return false; /* No known way to fix, may as well rotate anyway. */
  }
  const int max_i = 32; /* Limit iteration, mainly for debugging. */
  for (int i = 0; i < max_i; i++) {
    /* Binary search. */
    const float angle_mid = (angle_inside_bounds + angle) / 2.0f;
    if (ELEM(angle_mid, angle_inside_bounds, angle)) {
      break; /* Float precision reached. */
    }
    if (uv_rotation_in_clip_bounds_test(t, angle_mid)) {
      angle_inside_bounds = angle_mid;
    }
    else {
      angle = angle_mid;
    }
  }

  vec_inside_bounds[0] = angle_inside_bounds; /* Store for next iteration. */
  vec[0] = angle_inside_bounds;               /* Update rotation angle. */
  return true;
}

static void applyRotation(TransInfo *t)
{
  float3 axis_final;
  transform_mode_rotation_axis_get(t, axis_final);

  float final;
  if (applyNumInput(&t->num, &final)) {
    /* We have to limit the amount of turns to a reasonable number here,
     * to avoid things getting *very* slow, see how applyRotationValue() handles those... */
    final = large_rotation_limit(final);
  }
  else {
    final = t->values[0] + t->values_modal_offset[0];
    if (!(t->flag & T_INPUT_IS_VALUES_FINAL) &&
        transform_mode_is_axis_pointing_to_screen(t, axis_final))
    {
      /* Flip rotation direction if axis is pointing to screen. */
      final = -final;
    }
    transform_snap_mixed_apply(t, &final);
    if (!(transform_snap_is_active(t) && validSnap(t))) {
      transform_snap_increment(t, &final);
    }
  }

  t->values_final[0] = final;

  const bool is_large_rotation = hasNumInput(&t->num);
  applyRotationValue(t, final, axis_final, is_large_rotation);

  if (t->flag & T_CLIP_UV) {
    if (clip_uv_transform_rotate(t, t->values_final, t->values_inside_constraints)) {
      applyRotationValue(t, t->values_final[0], axis_final, is_large_rotation);
    }

    /* Not ideal, see #clipUVData code-comment. */
    if (t->flag & T_PROP_EDIT) {
      clipUVData(t);
    }
  }

  recalc_data(t);

  char str[UI_MAX_DRAW_STR];
  headerRotation(t, str, sizeof(str), t->values_final[0]);
  ED_area_status_text(t->area, str);
}

static void applyRotationMatrix(TransInfo *t, float mat_xform[4][4])
{
  float3 axis_final;
  transform_mode_rotation_axis_get(t, axis_final);
  const float angle_final = t->values_final[0];

  float mat3[3][3];
  float mat4[4][4];
  axis_angle_normalized_to_mat3(mat3, axis_final, angle_final);
  copy_m4_m3(mat4, mat3);
  transform_pivot_set_m4(mat4, t->center_global);
  mul_m4_m4m4(mat_xform, mat4, mat_xform);
}

static void initRotation(TransInfo *t, wmOperator * /*op*/)
{
  if (t->spacetype == SPACE_ACTION) {
    BKE_report(t->reports, RPT_ERROR, "Rotation is not supported in the Dope Sheet Editor");
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_ROTATION;

  if (transform_mode_affect_only_locations(t)) {
    WorkspaceStatus status(t->context);
    status.item(TIP_("Transform is set to only affect location"), ICON_ERROR);
    initMouseInputMode(t, &t->mouse, INPUT_ERROR_DASH);
  }
  else {
    initMouseInputMode(t, &t->mouse, INPUT_ANGLE);
  }

  t->idx_max = 0;
  t->num.idx_max = 0;
  initSnapAngleIncrements(t);

  copy_v3_fl(t->num.val_inc, t->increment[0] * t->increment_precision);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  if (t->flag & T_2D_EDIT) {
    t->flag |= T_NO_CONSTRAINT;
  }

  transform_mode_default_modal_orientation_set(t, V3D_ORIENT_VIEW);
}

/** \} */

TransModeInfo TransMode_rotate = {
    /*flags*/ 0,
    /*init_fn*/ initRotation,
    /*transform_fn*/ applyRotation,
    /*transform_matrix_fn*/ applyRotationMatrix,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ RotationBetween,
    /*snap_apply_fn*/ ApplySnapRotation,
    /*draw_fn*/ nullptr,
};

}  // namespace blender::ed::transform
