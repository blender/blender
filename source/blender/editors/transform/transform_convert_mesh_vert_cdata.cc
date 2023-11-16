/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"

#include "DEG_depsgraph.hh"

#include "transform.hh"
#include "transform_orientations.hh"

#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Bevel Weight and Crease Transform Creation
 * \{ */

static float *mesh_cdata_transdata_center(const TransIslandData *island_data,
                                          const int island_index,
                                          BMVert *eve)
{
  if (island_data->center && island_index != -1) {
    return island_data->center[island_index];
  }
  return eve->co;
}

static void mesh_cdata_transdata_create(TransDataBasic *td,
                                        BMVert *eve,
                                        float *weight,
                                        const TransIslandData *island_data,
                                        const int island_index)
{
  BLI_assert(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) == 0);

  td->val = weight;
  td->ival = *weight;

  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    td->flag |= TD_SELECTED;
  }

  copy_v3_v3(td->center, mesh_cdata_transdata_center(island_data, island_index, eve));
  td->extra = eve;
}

static void createTransMeshVertCData(bContext * /*C*/, TransInfo *t)
{
  BLI_assert(ELEM(t->mode, TFM_BWEIGHT, TFM_VERT_CREASE));
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;
    float mtx[3][3], smtx[3][3];
    int a;
    const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;

    TransIslandData island_data = {nullptr};
    TransMeshDataCrazySpace crazyspace_data = {nullptr};

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if ((!prop_mode || (prop_mode & T_PROP_CONNECTED)) && (bm->totvertsel == 0)) {
      continue;
    }

    int cd_offset = -1;
    if (t->mode == TFM_BWEIGHT) {
      if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert")) {
        BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
      }
      cd_offset = CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
    }
    else {
      if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert")) {
        BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLOAT, "crease_vert");
      }
      cd_offset = CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert");
    }

    if (cd_offset == -1) {
      continue;
    }

    int data_len = 0;
    if (prop_mode) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          data_len++;
        }
      }
    }
    else {
      data_len = bm->totvertsel;
    }

    if (data_len == 0) {
      continue;
    }

    const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS);
    if (is_island_center) {
      /* In this specific case, near-by vertices will need to know
       * the island of the nearest connected vertex. */
      const bool calc_single_islands = ((prop_mode & T_PROP_CONNECTED) &&
                                        (t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                        (em->selectmode & SCE_SELECT_VERTEX));

      const bool calc_island_center = false;
      const bool calc_island_axismtx = false;

      transform_convert_mesh_islands_calc(
          em, calc_single_islands, calc_island_center, calc_island_axismtx, &island_data);
    }

    copy_m3_m4(mtx, tc->obedit->object_to_world);
    /* we use a pseudo-inverse so that when one of the axes is scaled to 0,
     * matrix inversion still works and we can still moving along the other */
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    /* Original index of our connected vertex when connected distances are calculated.
     * Optional, allocate if needed. */
    int *dists_index = nullptr;
    float *dists = nullptr;
    if (prop_mode & T_PROP_CONNECTED) {
      dists = static_cast<float *>(MEM_mallocN(bm->totvert * sizeof(float), __func__));
      if (is_island_center) {
        dists_index = static_cast<int *>(MEM_mallocN(bm->totvert * sizeof(int), __func__));
      }
      transform_convert_mesh_connectivity_distance(em->bm, mtx, dists, dists_index);
    }

    /* Detect CrazySpace [tm]. */
    transform_convert_mesh_crazyspace_detect(t, tc, em, &crazyspace_data);

    /* Create TransData. */
    BLI_assert(data_len >= 1);
    tc->data_len = data_len;
    tc->data = static_cast<TransData *>(
        MEM_callocN(data_len * sizeof(TransData), "TransObData(Mesh EditMode)"));

    TransData *td = tc->data;
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
      if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        continue;
      }

      int island_index = -1;
      if (island_data.island_vert_map) {
        const int connected_index = (dists_index && dists_index[a] != -1) ? dists_index[a] : a;
        island_index = island_data.island_vert_map[connected_index];
      }

      float *weight = static_cast<float *>(BM_ELEM_CD_GET_VOID_P(eve, cd_offset));
      if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        mesh_cdata_transdata_create((TransDataBasic *)td, eve, weight, &island_data, island_index);

        if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
          createSpaceNormal(td->axismtx, eve->no);
        }
        else {
          /* Setting normals */
          copy_v3_v3(td->axismtx[2], eve->no);
          td->axismtx[0][0] = td->axismtx[0][1] = td->axismtx[0][2] = td->axismtx[1][0] =
              td->axismtx[1][1] = td->axismtx[1][2] = 0.0f;
        }

        if (prop_mode) {
          if (prop_mode & T_PROP_CONNECTED) {
            td->dist = dists[a];
          }
          else {
            td->dist = FLT_MAX;
          }
        }

        /* CrazySpace */
        transform_convert_mesh_crazyspace_transdata_set(
            mtx,
            smtx,
            !crazyspace_data.defmats.is_empty() ? crazyspace_data.defmats[a].ptr() : nullptr,
            crazyspace_data.quats && BM_elem_flag_test(eve, BM_ELEM_TAG) ?
                crazyspace_data.quats[a] :
                nullptr,
            td);

        td++;
      }
    }

    transform_convert_mesh_islanddata_free(&island_data);
    transform_convert_mesh_crazyspace_free(&crazyspace_data);
    if (dists) {
      MEM_freeN(dists);
    }
    if (dists_index) {
      MEM_freeN(dists_index);
    }
  }
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Recalc Mesh Data
 * \{ */

static void recalcData_mesh_cdata(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_MeshVertCData = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransMeshVertCData,
    /*recalc_data*/ recalcData_mesh_cdata,
    /*special_aftertrans_update*/ nullptr,
};
