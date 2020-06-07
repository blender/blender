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

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_lattice.h"

#include "transform.h"
#include "transform_snap.h"

/* Own include. */
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Curve/Surfaces Transform Creation
 *
 * \{ */

void createTransLatticeVerts(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    Lattice *latt = ((Lattice *)tc->obedit->data)->editlatt->latt;
    TransData *td = NULL;
    BPoint *bp;
    float mtx[3][3], smtx[3][3];
    int a;
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

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

    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
      return;
    }

    if (is_prop_edit) {
      tc->data_len = count;
    }
    else {
      tc->data_len = countsel;
    }
    tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Lattice EditMode)");

    copy_m3_m4(mtx, tc->obedit->obmat);
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

          td->ext = NULL;
          td->val = NULL;

          td++;
        }
      }
      bp++;
    }
  }
}

void recalcData_lattice(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    applyProject(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Lattice *la = tc->obedit->data;
    DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */
    if (la->editlatt->latt->flag & LT_OUTSIDE) {
      outside_lattice(la->editlatt->latt);
    }
  }
}

/** \} */
