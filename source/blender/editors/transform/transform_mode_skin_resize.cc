/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface_types.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Skin)
 * \{ */

static void transdata_elem_skin_resize(const TransInfo *t,
                                       const TransDataContainer * /*tc*/,
                                       TransData *td,
                                       const float mat[3][3])
{
  float tmat[3][3], smat[3][3];
  float fscale[3];

  if (t->flag & T_EDIT) {
    mul_m3_m3m3(smat, mat, td->mtx);
    mul_m3_m3m3(tmat, td->smtx, smat);
  }
  else {
    copy_m3_m3(tmat, mat);
  }

  if (t->con.applySize) {
    t->con.applySize(t, nullptr, nullptr, tmat);
  }

  mat3_to_size(fscale, tmat);
  td->loc[0] = td->iloc[0] * (1 + (fscale[0] - 1) * td->factor);
  td->loc[1] = td->iloc[1] * (1 + (fscale[1] - 1) * td->factor);
}

static void applySkinResize(TransInfo *t)
{
  float mat_final[3][3];
  char str[UI_MAX_DRAW_STR];

  if (t->flag & T_INPUT_IS_VALUES_FINAL) {
    copy_v3_v3(t->values_final, t->values);
  }
  else {
    copy_v3_fl(t->values_final, t->values[0]);
    add_v3_v3(t->values_final, t->values_modal_offset);

    transform_snap_increment(t, t->values_final);

    if (applyNumInput(&t->num, t->values_final)) {
      constraintNumInput(t, t->values_final);
    }

    transform_snap_mixed_apply(t, t->values_final);
  }

  size_to_mat3(mat_final, t->values_final);

  headerResize(t, t->values_final, str, sizeof(str));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    threading::parallel_for(IndexRange(tc->data_len), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        TransData *td = &tc->data[i];
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_skin_resize(t, tc, td, mat_final);
      }
    });
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initSkinResize(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_SKIN_RESIZE;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->flag |= T_NULL_ONE;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  if ((t->flag & T_EDIT) == 0) {
#ifdef USE_NUM_NO_ZERO
    t->num.val_flag[0] |= NUM_NO_ZERO;
    t->num.val_flag[1] |= NUM_NO_ZERO;
    t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
  }

  t->idx_max = 2;
  t->num.idx_max = 2;
  t->increment = float3(0.1f);
  t->increment_precision = 0.1f;

  copy_v3_fl(t->num.val_inc, t->increment[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
  t->num.unit_type[2] = B_UNIT_NONE;
}

/** \} */

TransModeInfo TransMode_skinresize = {
    /*flags*/ 0,
    /*init_fn*/ initSkinResize,
    /*transform_fn*/ applySkinResize,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};

}  // namespace blender::ed::transform
