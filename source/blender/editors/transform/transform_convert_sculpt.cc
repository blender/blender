/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "ED_sculpt.hh"

#include "transform.hh"
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Sculpt Transform Creation
 * \{ */

static void createTransSculpt(bContext *C, TransInfo *t)
{
  using namespace blender::ed;
  TransData *td;

  Scene *scene = t->scene;
  if (!BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
    BKE_report(t->reports, RPT_ERROR, "Linked data can't text-space transform");
    return;
  }

  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(t->view_layer);
  SculptSession *ss = ob->sculpt;

  /* Avoid editing locked shapes. */
  if (t->mode != TFM_DUMMY && ED_sculpt_report_if_shape_key_is_locked(ob, t->reports)) {
    return;
  }

  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    tc->is_active = true;
    td = tc->data = MEM_cnew<TransData>(__func__);
    td->ext = tc->data_ext = MEM_cnew<TransDataExtension>(__func__);
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, ss->pivot_pos);
  mul_m4_v3(ob->object_to_world().ptr(), td->center);
  td->ob = ob;

  td->loc = ss->pivot_pos;
  copy_v3_v3(td->iloc, ss->pivot_pos);

  if (is_zero_v4(ss->pivot_rot)) {
    ss->pivot_rot[3] = 1.0f;
  }

  float obmat_inv[3][3];
  copy_m3_m4(obmat_inv, ob->object_to_world().ptr());
  invert_m3(obmat_inv);

  td->ext->rot = nullptr;
  td->ext->rotAxis = nullptr;
  td->ext->rotAngle = nullptr;
  td->ext->quat = ss->pivot_rot;
  copy_m4_m4(td->ext->obmat, ob->object_to_world().ptr());
  copy_m3_m3(td->ext->l_smtx, obmat_inv);
  copy_m3_m4(td->ext->r_mtx, ob->object_to_world().ptr());
  copy_m3_m3(td->ext->r_smtx, obmat_inv);

  copy_qt_qt(td->ext->iquat, ss->pivot_rot);
  td->ext->rotOrder = ROT_MODE_QUAT;

  ss->pivot_scale[0] = 1.0f;
  ss->pivot_scale[1] = 1.0f;
  ss->pivot_scale[2] = 1.0f;
  td->ext->size = ss->pivot_scale;
  copy_v3_v3(ss->init_pivot_scale, ss->pivot_scale);
  copy_v3_v3(td->ext->isize, ss->init_pivot_scale);

  copy_m3_m3(td->smtx, obmat_inv);
  copy_m3_m4(td->mtx, ob->object_to_world().ptr());
  copy_m3_m4(td->axismtx, ob->object_to_world().ptr());

  BLI_assert(!(t->options & CTX_PAINT_CURVE));
  sculpt_paint::init_transform(C, ob, t->mval, t->undo_name);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data object
 * \{ */

static void recalcData_sculpt(TransInfo *t)
{
  using namespace blender::ed;
  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(t->view_layer);
  sculpt_paint::update_modal_transform(t->context, ob);
}

static void special_aftertrans_update__sculpt(bContext *C, TransInfo *t)
{
  using namespace blender::ed;
  Scene *scene = t->scene;
  if (!BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
    /* `sculpt_paint::init_transform` was not called in this case. */
    return;
  }

  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(t->view_layer);
  BLI_assert(!(t->options & CTX_PAINT_CURVE));
  sculpt_paint::end_transform(C, ob);
}

/** \} */

TransConvertTypeInfo TransConvertType_Sculpt = {
    /*flags*/ 0,
    /*create_trans_data*/ createTransSculpt,
    /*recalc_data*/ recalcData_sculpt,
    /*special_aftertrans_update*/ special_aftertrans_update__sculpt,
};
