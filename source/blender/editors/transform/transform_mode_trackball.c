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
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Rotation - Trackball) */

/** \name Transform Rotation - Trackball
 * \{ */

static void applyTrackballValue(TransInfo *t,
                                const float axis1[3],
                                const float axis2[3],
                                const float angles[2])
{
  float mat[3][3];
  float axis[3];
  float angle;
  int i;

  mul_v3_v3fl(axis, axis1, angles[0]);
  madd_v3_v3fl(axis, axis2, angles[1]);
  angle = normalize_v3(axis);
  axis_angle_normalized_to_mat3(mat, axis, angle);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if (t->flag & T_PROP_EDIT) {
        axis_angle_normalized_to_mat3(mat, axis, td->factor * angle);
      }

      ElementRotation(t, tc, td, mat, t->around);
    }
  }
}

static void applyTrackball(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float axis1[3], axis2[3];
#if 0 /* UNUSED */
  float mat[3][3], totmat[3][3], smat[3][3];
#endif
  float phi[2];

  copy_v3_v3(axis1, t->persinv[0]);
  copy_v3_v3(axis2, t->persinv[1]);
  normalize_v3(axis1);
  normalize_v3(axis2);

  copy_v2_v2(phi, t->values);

  snapGridIncrement(t, phi);

  applyNumInput(&t->num, phi);

  copy_v2_v2(t->values_final, phi);

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN * 2];

    outputNumInput(&(t->num), c, &t->scene->unit);

    ofs += BLI_snprintf(str + ofs,
                        sizeof(str) - ofs,
                        TIP_("Trackball: %s %s %s"),
                        &c[0],
                        &c[NUM_STR_REP_LEN],
                        t->proptext);
  }
  else {
    ofs += BLI_snprintf(str + ofs,
                        sizeof(str) - ofs,
                        TIP_("Trackball: %.2f %.2f %s"),
                        RAD2DEGF(phi[0]),
                        RAD2DEGF(phi[1]),
                        t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, sizeof(str) - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }

#if 0 /* UNUSED */
  axis_angle_normalized_to_mat3(smat, axis1, phi[0]);
  axis_angle_normalized_to_mat3(totmat, axis2, phi[1]);

  mul_m3_m3m3(mat, smat, totmat);

  // TRANSFORM_FIX_ME
  //copy_m3_m3(t->mat, mat);  // used in gizmo
#endif

  applyTrackballValue(t, axis1, axis2, phi);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initTrackball(TransInfo *t)
{
  t->mode = TFM_TRACKBALL;
  t->transform = applyTrackball;

  initMouseInputMode(t, &t->mouse, INPUT_TRACKBALL);

  t->idx_max = 1;
  t->num.idx_max = 1;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[2]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
  t->num.unit_type[1] = B_UNIT_ROTATION;

  t->flag |= T_NO_CONSTRAINT;
}
/** \} */
