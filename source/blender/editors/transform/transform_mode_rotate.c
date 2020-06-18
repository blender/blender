/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "transform.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Rotation) */

/** \name Transform Rotation
 * \{ */

static float RotationBetween(TransInfo *t, const float p1[3], const float p2[3])
{
  float angle, start[3], end[3];

  sub_v3_v3v3(start, p1, t->center_global);
  sub_v3_v3v3(end, p2, t->center_global);

  // Angle around a constraint axis (error prone, will need debug)
  if (t->con.applyRot != NULL && (t->con.mode & CON_APPLY)) {
    float axis[3], tmp[3];

    t->con.applyRot(t, NULL, NULL, axis, NULL);

    project_v3_v3v3(tmp, end, axis);
    sub_v3_v3v3(end, end, tmp);

    project_v3_v3v3(tmp, start, axis);
    sub_v3_v3v3(start, start, tmp);

    normalize_v3(end);
    normalize_v3(start);

    cross_v3_v3v3(tmp, start, end);

    if (dot_v3v3(tmp, axis) < 0.0f) {
      angle = -acosf(dot_v3v3(start, end));
    }
    else {
      angle = acosf(dot_v3v3(start, end));
    }
  }
  else {
    float mtx[3][3];

    copy_m3_m4(mtx, t->viewmat);

    mul_m3_v3(mtx, end);
    mul_m3_v3(mtx, start);

    angle = atan2f(start[1], start[0]) - atan2f(end[1], end[0]);
  }

  if (angle > (float)M_PI) {
    angle = angle - 2 * (float)M_PI;
  }
  else if (angle < -((float)M_PI)) {
    angle = 2.0f * (float)M_PI + angle;
  }

  return angle;
}

static void ApplySnapRotation(TransInfo *t, float *value)
{
  float point[3];
  getSnapPoint(t, point);

  float dist = RotationBetween(t, t->tsnap.snapTarget, point);
  *value = dist;
}

static float large_rotation_limit(float angle)
{
  /* Limit rotation to 1001 turns max
   * (otherwise iterative handling of 'large' rotations would become too slow). */
  const float angle_max = (float)(M_PI * 2000.0);
  if (fabsf(angle) > angle_max) {
    const float angle_sign = angle < 0.0f ? -1.0f : 1.0f;
    angle = angle_sign * (fmodf(fabsf(angle), (float)(M_PI * 2.0)) + angle_max);
  }
  return angle;
}

static void applyRotationValue(TransInfo *t,
                               float angle,
                               float axis[3],
                               const bool is_large_rotation)
{
  float mat[3][3];
  int i;

  const float angle_sign = angle < 0.0f ? -1.0f : 1.0f;
  /* We cannot use something too close to 180°, or 'continuous' rotation may fail
   * due to computing error... */
  const float angle_step = angle_sign * (float)(0.9 * M_PI);

  if (is_large_rotation) {
    /* Just in case, calling code should have already done that in practice
     * (for UI feedback reasons). */
    angle = large_rotation_limit(angle);
  }

  axis_angle_normalized_to_mat3(mat, axis, angle);
  /* Counter for needed updates (when we need to update to non-default matrix,
   * we also need another update on next iteration to go back to default matrix,
   * hence the '2' value used here, instead of a mere boolean). */
  short do_update_matrix = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      float angle_final = angle;
      if (t->con.applyRot) {
        t->con.applyRot(t, tc, td, axis, NULL);
        angle_final = angle * td->factor;
        /* Even though final angle might be identical to orig value,
         * we have to update the rotation matrix in that case... */
        do_update_matrix = 2;
      }
      else if (t->flag & T_PROP_EDIT) {
        angle_final = angle * td->factor;
      }

      /* Rotation is very likely to be above 180°, we need to do rotation by steps.
       * Note that this is only needed when doing 'absolute' rotation
       * (i.e. from initial rotation again, typically when using numinput).
       * regular incremental rotation (from mouse/widget/...) will be called often enough,
       * hence steps are small enough to be properly handled without that complicated trick.
       * Note that we can only do that kind of stepped rotation if we have initial rotation values
       * (and access to some actual rotation value storage).
       * Otherwise, just assume it's useless (e.g. in case of mesh/UV/etc. editing).
       * Also need to be in Euler rotation mode, the others never allow more than one turn anyway.
       */
      if (is_large_rotation && td->ext != NULL && td->ext->rotOrder == ROT_MODE_EUL) {
        copy_v3_v3(td->ext->rot, td->ext->irot);
        for (float angle_progress = angle_step; fabsf(angle_progress) < fabsf(angle_final);
             angle_progress += angle_step) {
          axis_angle_normalized_to_mat3(mat, axis, angle_progress);
          ElementRotation(t, tc, td, mat, t->around);
        }
        do_update_matrix = 2;
      }
      else if (angle_final != angle) {
        do_update_matrix = 2;
      }

      if (do_update_matrix > 0) {
        axis_angle_normalized_to_mat3(mat, axis, angle_final);
        do_update_matrix--;
      }

      ElementRotation(t, tc, td, mat, t->around);
    }
  }
}

static void applyRotation(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];

  float final;

  final = t->values[0];

  snapGridIncrement(t, &final);

  float axis_final[3];
  /* Use the negative axis to match the default Z axis of the view matrix. */
  negate_v3_v3(axis_final, t->spacemtx[t->orient_axis]);

  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, NULL, NULL, axis_final, NULL);
  }

  applySnapping(t, &final);

  if (applyNumInput(&t->num, &final)) {
    /* We have to limit the amount of turns to a reasonable number here,
     * to avoid things getting *very* slow, see how applyRotationValue() handles those... */
    final = large_rotation_limit(final);
  }

  t->values_final[0] = final;

  headerRotation(t, str, final);

  const bool is_large_rotation = hasNumInput(&t->num);
  applyRotationValue(t, final, axis_final, is_large_rotation);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initRotation(TransInfo *t)
{
  t->mode = TFM_ROTATION;
  t->transform = applyRotation;
  t->tsnap.applySnap = ApplySnapRotation;
  t->tsnap.distance = RotationBetween;

  setInputPostFct(&t->mouse, postInputRotation);
  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[2]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  if (t->flag & T_2D_EDIT) {
    t->flag |= T_NO_CONSTRAINT;
  }
}
/** \} */
