/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"

#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "BLT_translation.hh"

#include "UI_interface_types.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Push/Pull)
 * \{ */

static void transdata_elem_push_pull(const TransInfo *t,
                                     const TransDataContainer *tc,
                                     TransData *td,
                                     const float distance,
                                     const float axis_global[3],
                                     const bool is_lock_constraint,
                                     const bool is_data_space)
{
  float vec[3];
  sub_v3_v3v3(vec, tc->center_local, td->center);
  if (t->con.applyRot && t->con.mode & CON_APPLY) {
    float axis[3];
    copy_v3_v3(axis, axis_global);
    t->con.applyRot(t, tc, td, axis);

    mul_m3_v3(td->smtx, axis);
    if (is_lock_constraint) {
      float dvec[3];
      project_v3_v3v3(dvec, vec, axis);
      sub_v3_v3(vec, dvec);
    }
    else {
      project_v3_v3v3(vec, vec, axis);
    }
  }
  normalize_v3_length(vec, distance * td->factor);
  if (is_data_space) {
    mul_m3_v3(td->smtx, vec);
  }

  add_v3_v3v3(td->loc, td->iloc, vec);
}

static void applyPushPull(TransInfo *t)
{
  float axis_global[3];
  float distance;
  char str[UI_MAX_DRAW_STR];

  distance = t->values[0] + t->values_modal_offset[0];

  transform_snap_increment(t, &distance);

  applyNumInput(&t->num, &distance);

  t->values_final[0] = distance;

  /* Header print for NumInput. */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, t->scene->unit);

    SNPRINTF_UTF8(str, IFACE_("Push/Pull: %s%s %s"), c, t->con.text, t->proptext);
  }
  else {
    /* Default header print. */
    SNPRINTF_UTF8(str, IFACE_("Push/Pull: %.4f%s %s"), distance, t->con.text, t->proptext);
  }

  if (t->con.applyRot && t->con.mode & CON_APPLY) {
    t->con.applyRot(t, nullptr, nullptr, axis_global);
  }

  const bool is_lock_constraint = isLockConstraint(t);
  const bool is_data_space = (t->options & CTX_POSE_BONE) != 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    threading::parallel_for(IndexRange(tc->data_len), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        TransData *td = &tc->data[i];
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_push_pull(
            t, tc, td, distance, axis_global, is_lock_constraint, is_data_space);
      }
    });
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initPushPull(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_PUSHPULL;

  initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->increment[0] = 1.0f;
  t->increment_precision = 0.1f;

  copy_v3_fl(t->num.val_inc, t->increment[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_LENGTH;
}

/** \} */

TransModeInfo TransMode_pushpull = {
    /*flags*/ 0,
    /*init_fn*/ initPushPull,
    /*transform_fn*/ applyPushPull,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};

}  // namespace blender::ed::transform
