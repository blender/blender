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
#include "BLI_string_utf8.h"
#include "BLI_task.hh"

#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Rotation - Trackball)
 * \{ */

static void transdata_elem_trackball(const TransInfo *t,
                                     const TransDataContainer *tc,
                                     TransData *td,
                                     TransDataExtension *td_ext,
                                     const float axis[3],
                                     const float angle,
                                     const float mat_final[3][3])
{
  float mat_buf[3][3];
  const float (*mat)[3] = mat_final;
  if (t->flag & T_PROP_EDIT) {
    axis_angle_normalized_to_mat3(mat_buf, axis, td->factor * angle);
    mat = mat_buf;
  }
  ElementRotation(t, tc, td, td_ext, mat, t->around);
}

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

  axis_angle_normalized_to_mat3(mat_final, axis, angle);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    threading::parallel_for(IndexRange(tc->data_len), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        TransData *td = &tc->data[i];
        TransDataExtension *td_ext = tc->data_ext ? &tc->data_ext[i] : nullptr;
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_trackball(t, tc, td, td_ext, axis, angle, mat_final);
      }
    });
  }
}

static void applyTrackball(TransInfo *t)
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

    outputNumInput(&(t->num), c, t->scene->unit);

    ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                  sizeof(str) - ofs,
                                  IFACE_("Trackball: %s %s %s"),
                                  &c[0],
                                  &c[NUM_STR_REP_LEN],
                                  t->proptext);
  }
  else {
    ofs += BLI_snprintf_utf8_rlen(str + ofs,
                                  sizeof(str) - ofs,
                                  IFACE_("Trackball: %.2f %.2f %s"),
                                  RAD2DEGF(phi[0]),
                                  RAD2DEGF(phi[1]),
                                  t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf_utf8_rlen(
        str + ofs, sizeof(str) - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
  }

  float axis_final[3], angle_final;
  applyTrackballValue_calc_axis_angle(t, phi, axis_final, &angle_final);
  applyTrackballValue(t, axis_final, angle_final);

  recalc_data(t);

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

  if (transform_mode_affect_only_locations(t)) {
    WorkspaceStatus status(t->context);
    status.item(TIP_("Transform is set to only affect location"), ICON_ERROR);
    initMouseInputMode(t, &t->mouse, INPUT_ERROR);
  }
  else {
    initMouseInputMode(t, &t->mouse, INPUT_TRACKBALL);
  }

  t->idx_max = 1;
  t->num.idx_max = 1;
  t->increment = float3(DEG2RAD(5.0));
  t->increment_precision = 0.2f;

  copy_v3_fl(t->num.val_inc, t->increment[0] * t->increment_precision);
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

}  // namespace blender::ed::transform
