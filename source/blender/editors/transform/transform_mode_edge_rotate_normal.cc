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

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_unit.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"
/* -------------------------------------------------------------------- */
/** \name Transform (Normal Rotation)
 * \{ */

static void storeCustomLNorValue(TransDataContainer *tc, BMesh *bm)
{
  BMLoopNorEditDataArray *lnors_ed_arr = BM_loop_normal_editdata_array_init(bm, false);
  // BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata;

  tc->custom.mode.data = lnors_ed_arr;
  tc->custom.mode.free_cb = freeCustomNormalArray;
}

void freeCustomNormalArray(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  BMLoopNorEditDataArray *lnors_ed_arr = static_cast<BMLoopNorEditDataArray *>(custom_data->data);

  if (t->state == TRANS_CANCEL) {
    BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    /* Restore custom loop normal on cancel */
    for (int i = 0; i < lnors_ed_arr->totloop; i++, lnor_ed++) {
      BKE_lnor_space_custom_normal_to_data(
          bm->lnor_spacearr->lspacearr[lnor_ed->loop_index], lnor_ed->niloc, lnor_ed->clnors_data);
    }
  }

  BM_loop_normal_editdata_array_free(lnors_ed_arr);

  tc->custom.mode.data = nullptr;
  tc->custom.mode.free_cb = nullptr;
}

/* Works by getting custom normal from clnor_data, transform, then store */
static void applyNormalRotation(TransInfo *t)
{
  char str[UI_MAX_DRAW_STR];

  float axis_final[3];
  copy_v3_v3(axis_final, t->spacemtx[t->orient_axis]);

  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, nullptr, nullptr, axis_final, nullptr);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    BMLoopNorEditDataArray *lnors_ed_arr = static_cast<BMLoopNorEditDataArray *>(
        tc->custom.mode.data);
    BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata;

    float axis[3];
    float mat[3][3];
    float angle = t->values[0] + t->values_modal_offset[0];
    copy_v3_v3(axis, axis_final);

    transform_snap_increment(t, &angle);

    transform_snap_mixed_apply(t, &angle);

    applyNumInput(&t->num, &angle);

    headerRotation(t, str, sizeof(str), angle);

    axis_angle_normalized_to_mat3(mat, axis, angle);

    for (int i = 0; i < lnors_ed_arr->totloop; i++, lnor_ed++) {
      mul_v3_m3v3(lnor_ed->nloc, mat, lnor_ed->niloc);

      BKE_lnor_space_custom_normal_to_data(
          bm->lnor_spacearr->lspacearr[lnor_ed->loop_index], lnor_ed->nloc, lnor_ed->clnors_data);
    }

    t->values_final[0] = angle;
  }

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void initNormalRotation(TransInfo *t, wmOperator * /*op*/)
{
  t->mode = TFM_NORMAL_ROTATION;

  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = DEG2RAD(5.0);
  t->snap[1] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    BKE_editmesh_lnorspace_update(em);

    storeCustomLNorValue(tc, bm);
  }

  transform_mode_default_modal_orientation_set(t, V3D_ORIENT_VIEW);
}

/** \} */

TransModeInfo TransMode_rotatenormal = {
    /*flags*/ 0,
    /*init_fn*/ initNormalRotation,
    /*transform_fn*/ applyNormalRotation,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ nullptr,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
