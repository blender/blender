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

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Bend) */

/** \name Transform Bend
 * \{ */

struct BendCustomData {
  /* All values are in global space. */
  float warp_sta[3];
  float warp_end[3];

  float warp_nor[3];
  float warp_tan[3];

  /* for applying the mouse distance */
  float warp_init_dist;
};

static eRedrawFlag handleEventBend(TransInfo *UNUSED(t), const wmEvent *event)
{
  eRedrawFlag status = TREDRAW_NOTHING;

  if (event->type == MIDDLEMOUSE && event->val == KM_PRESS) {
    status = TREDRAW_HARD;
  }

  return status;
}

static void Bend(TransInfo *t, const int UNUSED(mval[2]))
{
  float vec[3];
  float pivot_global[3];
  float warp_end_radius_global[3];
  int i;
  char str[UI_MAX_DRAW_STR];
  const struct BendCustomData *data = t->custom.mode.data;
  const bool is_clamp = (t->flag & T_ALT_TRANSFORM) == 0;

  union {
    struct {
      float angle, scale;
    };
    float vector[2];
  } values;

  /* amount of radians for bend */
  copy_v2_v2(values.vector, t->values);

#if 0
  snapGrid(t, angle_rad);
#else
  /* hrmf, snapping radius is using 'angle' steps, need to convert to something else
   * this isnt essential but nicer to give reasonable snapping values for radius */
  if (t->tsnap.mode & SCE_SNAP_MODE_INCREMENT) {
    const float radius_snap = 0.1f;
    const float snap_hack = (t->snap[1] * data->warp_init_dist) / radius_snap;
    values.scale *= snap_hack;
    snapGridIncrement(t, values.vector);
    values.scale /= snap_hack;
  }
#endif

  if (applyNumInput(&t->num, values.vector)) {
    values.scale = values.scale / data->warp_init_dist;
  }

  copy_v2_v2(t->values_final, values.vector);

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN * 2];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str,
                 sizeof(str),
                 TIP_("Bend Angle: %s Radius: %s Alt, Clamp %s"),
                 &c[0],
                 &c[NUM_STR_REP_LEN],
                 WM_bool_as_string(is_clamp));
  }
  else {
    /* default header print */
    BLI_snprintf(str,
                 sizeof(str),
                 TIP_("Bend Angle: %.3f Radius: %.4f, Alt, Clamp %s"),
                 RAD2DEGF(values.angle),
                 values.scale * data->warp_init_dist,
                 WM_bool_as_string(is_clamp));
  }

  values.angle *= -1.0f;
  values.scale *= data->warp_init_dist;

  /* calc 'data->warp_end' from 'data->warp_end_init' */
  copy_v3_v3(warp_end_radius_global, data->warp_end);
  dist_ensure_v3_v3fl(warp_end_radius_global, data->warp_sta, values.scale);
  /* done */

  /* calculate pivot */
  copy_v3_v3(pivot_global, data->warp_sta);
  if (values.angle > 0.0f) {
    madd_v3_v3fl(pivot_global,
                 data->warp_tan,
                 -values.scale * shell_angle_to_dist((float)M_PI_2 - values.angle));
  }
  else {
    madd_v3_v3fl(pivot_global,
                 data->warp_tan,
                 +values.scale * shell_angle_to_dist((float)M_PI_2 + values.angle));
  }

  /* TODO(campbell): xform, compensate object center. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;

    float warp_sta_local[3];
    float warp_end_local[3];
    float warp_end_radius_local[3];
    float pivot_local[3];

    if (tc->use_local_mat) {
      sub_v3_v3v3(warp_sta_local, data->warp_sta, tc->mat[3]);
      sub_v3_v3v3(warp_end_local, data->warp_end, tc->mat[3]);
      sub_v3_v3v3(warp_end_radius_local, warp_end_radius_global, tc->mat[3]);
      sub_v3_v3v3(pivot_local, pivot_global, tc->mat[3]);
    }
    else {
      copy_v3_v3(warp_sta_local, data->warp_sta);
      copy_v3_v3(warp_end_local, data->warp_end);
      copy_v3_v3(warp_end_radius_local, warp_end_radius_global);
      copy_v3_v3(pivot_local, pivot_global);
    }

    for (i = 0; i < tc->data_len; i++, td++) {
      float mat[3][3];
      float delta[3];
      float fac, fac_scaled;

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (UNLIKELY(values.angle == 0.0f)) {
        copy_v3_v3(td->loc, td->iloc);
        continue;
      }

      copy_v3_v3(vec, td->iloc);
      mul_m3_v3(td->mtx, vec);

      fac = line_point_factor_v3(vec, warp_sta_local, warp_end_radius_local);
      if (is_clamp) {
        CLAMP(fac, 0.0f, 1.0f);
      }

      if (t->options & CTX_GPENCIL_STROKES) {
        /* grease pencil multiframe falloff */
        bGPDstroke *gps = (bGPDstroke *)td->extra;
        if (gps != NULL) {
          fac_scaled = fac * td->factor * gps->runtime.multi_frame_falloff;
        }
        else {
          fac_scaled = fac * td->factor;
        }
      }
      else {
        fac_scaled = fac * td->factor;
      }

      axis_angle_normalized_to_mat3(mat, data->warp_nor, values.angle * fac_scaled);
      interp_v3_v3v3(delta, warp_sta_local, warp_end_radius_local, fac_scaled);
      sub_v3_v3(delta, warp_sta_local);

      /* delta is subtracted, rotation adds back this offset */
      sub_v3_v3(vec, delta);

      sub_v3_v3(vec, pivot_local);
      mul_m3_v3(mat, vec);
      add_v3_v3(vec, pivot_local);

      mul_m3_v3(td->smtx, vec);

      /* rotation */
      if ((t->flag & T_POINTS) == 0) {
        ElementRotation(t, tc, td, mat, V3D_AROUND_LOCAL_ORIGINS);
      }

      /* location */
      copy_v3_v3(td->loc, vec);
    }
  }

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initBend(TransInfo *t)
{
  const float mval_fl[2] = {UNPACK2(t->mval)};
  const float *curs;
  float tvec[3];
  struct BendCustomData *data;

  t->mode = TFM_BEND;
  t->transform = Bend;
  t->handleEvent = handleEventBend;

  setInputPostFct(&t->mouse, postInputRotation);
  initMouseInputMode(t, &t->mouse, INPUT_ANGLE_SPRING);

  t->idx_max = 1;
  t->num.idx_max = 1;
  t->snap[0] = 0.0f;
  t->snap[1] = SNAP_INCREMENTAL_ANGLE;
  t->snap[2] = t->snap[1] * 0.2;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
  t->num.unit_type[1] = B_UNIT_LENGTH;

  t->flag |= T_NO_CONSTRAINT;

  // copy_v3_v3(t->center, ED_view3d_cursor3d_get(t->scene, t->view));
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    calculateCenterCursor(t, t->center_global);
  }
  calculateCenterLocal(t, t->center_global);

  t->val = 0.0f;

  data = MEM_callocN(sizeof(*data), __func__);

  curs = t->scene->cursor.location;
  copy_v3_v3(data->warp_sta, curs);
  ED_view3d_win_to_3d(
      (View3D *)t->area->spacedata.first, t->region, curs, mval_fl, data->warp_end);

  copy_v3_v3(data->warp_nor, t->viewinv[2]);
  normalize_v3(data->warp_nor);

  /* tangent */
  sub_v3_v3v3(tvec, data->warp_end, data->warp_sta);
  cross_v3_v3v3(data->warp_tan, tvec, data->warp_nor);
  normalize_v3(data->warp_tan);

  data->warp_init_dist = len_v3v3(data->warp_end, data->warp_sta);

  t->custom.mode.data = data;
  t->custom.mode.use_free = true;
}
/** \} */
