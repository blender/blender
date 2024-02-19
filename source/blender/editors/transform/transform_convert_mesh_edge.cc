/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_mesh_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"

#include "transform.hh"
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Edge (for crease) Transform Creation
 * \{ */

static void createTransEdge(bContext * /*C*/, TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    TransData *td = nullptr;
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

    td = tc->data = static_cast<TransData *>(
        MEM_callocN(tc->data_len * sizeof(TransData), "TransCrease"));

    copy_m3_m4(mtx, tc->obedit->object_to_world);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    /* create data we need */
    if (t->mode == TFM_BWEIGHT) {
      if (!CustomData_has_layer_named(&em->bm->edata, CD_PROP_FLOAT, "bevel_weight_edge")) {
        BM_data_layer_add_named(em->bm, &em->bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
      }
      cd_edge_float_offset = CustomData_get_offset_named(
          &em->bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
    }
    else { /* if (t->mode == TFM_EDGE_CREASE) { */
      BLI_assert(t->mode == TFM_EDGE_CREASE);
      if (!CustomData_has_layer_named(&em->bm->edata, CD_PROP_FLOAT, "crease_edge")) {
        BM_data_layer_add_named(em->bm, &em->bm->edata, CD_PROP_FLOAT, "crease_edge");
      }
      cd_edge_float_offset = CustomData_get_offset_named(
          &em->bm->edata, CD_PROP_FLOAT, "crease_edge");
    }

    BLI_assert(cd_edge_float_offset != -1);

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) &&
          (BM_elem_flag_test(eed, BM_ELEM_SELECT) || is_prop_edit))
      {
        float *fl_ptr;
        /* need to set center for center calculations */
        mid_v3_v3v3(td->center, eed->v1->co, eed->v2->co);

        td->loc = nullptr;
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          td->flag = TD_SELECTED;
        }
        else {
          td->flag = 0;
        }

        copy_m3_m3(td->smtx, smtx);
        copy_m3_m3(td->mtx, mtx);

        td->ext = nullptr;

        fl_ptr = static_cast<float *>(BM_ELEM_CD_GET_VOID_P(eed, cd_edge_float_offset));
        td->val = fl_ptr;
        td->ival = *fl_ptr;

        td++;
      }
    }
  }
}

static void recalcData_mesh_edge(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_MeshEdge = {
    /*flags*/ T_EDIT,
    /*create_trans_data*/ createTransEdge,
    /*recalc_data*/ recalcData_mesh_edge,
    /*special_aftertrans_update*/ nullptr,
};
