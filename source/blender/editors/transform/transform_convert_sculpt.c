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

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "ED_sculpt.h"

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Sculpt Transform Creation
 *
 * \{ */

void createTransSculpt(bContext *C, TransInfo *t)
{
  TransData *td;

  Scene *scene = t->scene;
  if (ID_IS_LINKED(scene)) {
    BKE_report(t->reports, RPT_ERROR, "Linked data can't text-space transform");
    return;
  }

  Object *ob = CTX_data_active_object(t->context);
  SculptSession *ss = ob->sculpt;

  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    tc->is_active = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransSculpt");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransSculpt");
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, ss->pivot_pos);
  mul_m4_v3(ob->obmat, td->center);
  td->ob = ob;

  td->loc = ss->pivot_pos;
  copy_v3_v3(td->iloc, ss->pivot_pos);

  if (is_zero_v4(ss->pivot_rot)) {
    ss->pivot_rot[3] = 1.0f;
  }

  float obmat_inv[3][3];
  copy_m3_m4(obmat_inv, ob->obmat);
  invert_m3(obmat_inv);

  td->ext->rot = NULL;
  td->ext->rotAxis = NULL;
  td->ext->rotAngle = NULL;
  td->ext->quat = ss->pivot_rot;
  copy_m4_m4(td->ext->obmat, ob->obmat);
  copy_m3_m3(td->ext->l_smtx, obmat_inv);
  copy_m3_m4(td->ext->r_mtx, ob->obmat);
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
  copy_m3_m4(td->mtx, ob->obmat);
  copy_m3_m4(td->axismtx, ob->obmat);

  BLI_assert(!(t->options & CTX_PAINT_CURVE));
  ED_sculpt_init_transform(C);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data object
 *
 * \{ */

void recalcData_sculpt(TransInfo *t)
{
  ED_sculpt_update_modal_transform(t->context);
}

void special_aftertrans_update__sculpt(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  if (ID_IS_LINKED(scene)) {
    /* `ED_sculpt_init_transform` was not called in this case. */
    return;
  }

  BLI_assert(!(t->options & CTX_PAINT_CURVE));
  ED_sculpt_end_transform(C);
}

/** \} */
