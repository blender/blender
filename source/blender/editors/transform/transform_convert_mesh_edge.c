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

#include "DNA_mesh_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Edge (for crease) Transform Creation
 *
 * \{ */

void createTransEdge(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    TransData *td = NULL;
    BMEdge *eed;
    BMIter iter;
    float mtx[3][3], smtx[3][3];
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
    int cd_edge_float_offset;

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          countsel++;
        }
        if (is_prop_edit) {
          count++;
        }
      }
    }

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

    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransCrease");

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    /* create data we need */
    if (t->mode == TFM_BWEIGHT) {
      BM_mesh_cd_flag_ensure(em->bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_EDGE_BWEIGHT);
      cd_edge_float_offset = CustomData_get_offset(&em->bm->edata, CD_BWEIGHT);
    }
    else {  // if (t->mode == TFM_CREASE) {
      BLI_assert(t->mode == TFM_CREASE);
      BM_mesh_cd_flag_ensure(em->bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_EDGE_CREASE);
      cd_edge_float_offset = CustomData_get_offset(&em->bm->edata, CD_CREASE);
    }

    BLI_assert(cd_edge_float_offset != -1);

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) &&
          (BM_elem_flag_test(eed, BM_ELEM_SELECT) || is_prop_edit)) {
        float *fl_ptr;
        /* need to set center for center calculations */
        mid_v3_v3v3(td->center, eed->v1->co, eed->v2->co);

        td->loc = NULL;
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          td->flag = TD_SELECTED;
        }
        else {
          td->flag = 0;
        }

        copy_m3_m3(td->smtx, smtx);
        copy_m3_m3(td->mtx, mtx);

        td->ext = NULL;

        fl_ptr = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_float_offset);
        td->val = fl_ptr;
        td->ival = *fl_ptr;

        td++;
      }
    }
  }
}

/** \} */
