/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_editmesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "ED_mesh.h"

#include "DEG_depsgraph_query.h"

#include "transform.h"
#include "transform_orientations.h"

#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Edit Mesh #CD_BWEIGHT and #CD_CREASE Transform Creation
 * \{ */

static float *tc_mesh_cdata_transdata_center(const struct TransIslandData *island_data,
                                             const int island_index,
                                             BMVert *eve)
{
  if (island_data->center && island_index != -1) {
    return island_data->center[island_index];
  }
  return eve->co;
}

static void tc_mesh_cdata_transdata_create(TransDataBasic *td,
                                           BMVert *eve,
                                           float *weight,
                                           const struct TransIslandData *island_data,
                                           const int island_index)
{
  BLI_assert(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) == 0);

  td->val = weight;
  td->ival = *weight;

  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    td->flag |= TD_SELECTED;
  }

  copy_v3_v3(td->center, tc_mesh_cdata_transdata_center(island_data, island_index, eve));
  td->extra = eve;
}

static void createTransMeshVertCData(bContext *UNUSED(C), TransInfo *t)
{
  BLI_assert(ELEM(t->mode, TFM_BWEIGHT, TFM_VERT_CREASE));
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    Mesh *me = tc->obedit->data;
    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;
    float mtx[3][3], smtx[3][3];
    int a;
    const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;

    struct TransIslandData island_data = {NULL};
    struct TransMirrorData mirror_data = {NULL};
    struct TransMeshDataCrazySpace crazyspace_data = {NULL};

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if ((!prop_mode || (prop_mode & T_PROP_CONNECTED)) && (bm->totvertsel == 0)) {
      continue;
    }

    int cd_offset = -1;
    if (t->mode == TFM_BWEIGHT) {
      if (!CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
        BM_data_layer_add(bm, &bm->vdata, CD_BWEIGHT);
      }
      cd_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
    }
    else {
      if (!CustomData_has_layer(&bm->vdata, CD_CREASE)) {
        BM_data_layer_add(bm, &bm->vdata, CD_CREASE);
      }
      cd_offset = CustomData_get_offset(&bm->vdata, CD_CREASE);
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
    int *dists_index = NULL;
    float *dists = NULL;
    if (prop_mode & T_PROP_CONNECTED) {
      dists = MEM_mallocN(bm->totvert * sizeof(float), __func__);
      if (is_island_center) {
        dists_index = MEM_mallocN(bm->totvert * sizeof(int), __func__);
      }
      transform_convert_mesh_connectivity_distance(em->bm, mtx, dists, dists_index);
    }

    /* Create TransDataMirror. */
    if (tc->use_mirror_axis_any) {
      bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
      bool use_select = (t->flag & T_PROP_EDIT) == 0;
      const bool mirror_axis[3] = {
          tc->use_mirror_axis_x, tc->use_mirror_axis_y, tc->use_mirror_axis_z};
      transform_convert_mesh_mirrordata_calc(
          em, use_select, use_topology, mirror_axis, &mirror_data);

      if (mirror_data.vert_map) {
        tc->data_mirror_len = mirror_data.mirror_elem_len;
        tc->data_mirror = MEM_mallocN(mirror_data.mirror_elem_len * sizeof(*tc->data_mirror),
                                      __func__);

        BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
          if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            if (mirror_data.vert_map[a].index != -1) {
              data_len--;
            }
          }
        }
      }
    }

    /* Detect CrazySpace [tm]. */
    transform_convert_mesh_crazyspace_detect(t, tc, em, &crazyspace_data);

    /* Create TransData. */
    BLI_assert(data_len >= 1);
    tc->data_len = data_len;
    tc->data = MEM_callocN(data_len * sizeof(TransData), "TransObData(Mesh EditMode)");

    TransData *td = tc->data;
    TransDataMirror *td_mirror = tc->data_mirror;
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
      if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        continue;
      }

      int island_index = -1;
      if (island_data.island_vert_map) {
        const int connected_index = (dists_index && dists_index[a] != -1) ? dists_index[a] : a;
        island_index = island_data.island_vert_map[connected_index];
      }

      float *weight = BM_ELEM_CD_GET_VOID_P(eve, cd_offset);
      if (mirror_data.vert_map && mirror_data.vert_map[a].index != -1) {
        tc_mesh_cdata_transdata_create(
            (TransDataBasic *)td_mirror, eve, weight, &island_data, island_index);

        int elem_index = mirror_data.vert_map[a].index;
        BMVert *v_src = BM_vert_at_index(bm, elem_index);

        td_mirror->flag |= mirror_data.vert_map[a].flag;
        td_mirror->loc_src = BM_ELEM_CD_GET_VOID_P(v_src, cd_offset);
        td_mirror++;
      }
      else if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        tc_mesh_cdata_transdata_create(
            (TransDataBasic *)td, eve, weight, &island_data, island_index);

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
            td->flag |= TD_NOTCONNECTED;
            td->dist = FLT_MAX;
          }
        }

        /* CrazySpace */
        transform_convert_mesh_crazyspace_transdata_set(
            mtx,
            smtx,
            crazyspace_data.defmats ? crazyspace_data.defmats[a] : NULL,
            crazyspace_data.quats && BM_elem_flag_test(eve, BM_ELEM_TAG) ?
                crazyspace_data.quats[a] :
                NULL,
            td);

        td++;
      }
    }

    transform_convert_mesh_islanddata_free(&island_data);
    transform_convert_mesh_mirrordata_free(&mirror_data);
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

static void tc_mesh_cdata_apply_to_mirror(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->use_mirror_axis_any) {
      TransDataMirror *td_mirror = tc->data_mirror;
      for (int i = 0; i < tc->data_mirror_len; i++, td_mirror++) {
        *td_mirror->val = td_mirror->loc_src[0];
      }
    }
  }
}

static void recalcData_mesh_cdata(TransInfo *t)
{
  bool is_canceling = t->state == TRANS_CANCEL;
  /* mirror modifier clipping? */
  if (!is_canceling) {
    if (!(t->flag & T_NO_MIRROR)) {
      tc_mesh_cdata_apply_to_mirror(t);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    DEG_id_tag_update(tc->obedit->data, ID_RECALC_GEOMETRY);
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BKE_editmesh_looptri_and_normals_calc(em);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_MeshVertCData = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*createTransData*/ createTransMeshVertCData,
    /*recalcData*/ recalcData_mesh_cdata,
    /*special_aftertrans_update*/ NULL,
};
