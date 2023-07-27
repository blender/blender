/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_lattice.h"

#include "transform.hh"
#include "transform_snap.hh"

/* Own include. */
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Curve/Surfaces Transform Creation
 * \{ */

static void createTransLatticeVerts(bContext * /*C*/, TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    Lattice *latt = ((Lattice *)tc->obedit->data)->editlatt->latt;
    TransData *td = nullptr;
    BPoint *bp;
    float mtx[3][3], smtx[3][3];
    int a;
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;

    bp = latt->def;
    a = latt->pntsu * latt->pntsv * latt->pntsw;
    while (a--) {
      if (bp->hide == 0) {
        if (bp->f1 & SELECT) {
          countsel++;
        }
        if (is_prop_edit) {
          count++;
        }
      }
      bp++;
    }

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if (((is_prop_edit && !is_prop_connected) ? count : countsel) == 0) {
      tc->data_len = 0;
      continue;
    }

    if (is_prop_edit) {
      tc->data_len = count;
    }
    else {
      tc->data_len = countsel;
    }
    tc->data = static_cast<TransData *>(
        MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Lattice EditMode)"));

    copy_m3_m4(mtx, tc->obedit->object_to_world);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    td = tc->data;
    bp = latt->def;
    a = latt->pntsu * latt->pntsv * latt->pntsw;
    while (a--) {
      if (is_prop_edit || (bp->f1 & SELECT)) {
        if (bp->hide == 0) {
          copy_v3_v3(td->iloc, bp->vec);
          td->loc = bp->vec;
          copy_v3_v3(td->center, td->loc);
          if (bp->f1 & SELECT) {
            td->flag = TD_SELECTED;
          }
          else {
            td->flag = 0;
          }
          copy_m3_m3(td->smtx, smtx);
          copy_m3_m3(td->mtx, mtx);

          td->ext = nullptr;
          td->val = nullptr;

          td++;
        }
      }
      bp++;
    }
  }
}

static void recalcData_lattice(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Lattice *la = static_cast<Lattice *>(tc->obedit->data);
    DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);
    if (la->editlatt->latt->flag & LT_OUTSIDE) {
      outside_lattice(la->editlatt->latt);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_Lattice = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransLatticeVerts,
    /*recalc_data*/ recalcData_lattice,
    /*special_aftertrans_update*/ nullptr,
};
