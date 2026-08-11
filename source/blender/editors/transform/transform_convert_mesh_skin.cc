/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"

#include "transform.hh"
#include "transform_orientations.hh"

#include "transform_convert.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Skin Modifier Radius Transform Creation
 * \{ */

static float *mesh_skin_transdata_center(const TransIslandData *island_data,
                                         const int island_index,
                                         BMVert *eve)
{
  if (island_data->center && island_index != -1) {
    return island_data->center[island_index];
  }
  return eve->co;
}

static void mesh_skin_transdata_create(TransDataBasic *td,
                                       BMVert *eve,
                                       const TransIslandData *island_data,
                                       const int island_index,
                                       const int cd_skin_radius_offset)
{
  BLI_assert(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) == 0);
  td->flag = 0;
  if (cd_skin_radius_offset != -1) {
    float2 *radius = static_cast<float2 *>(BM_ELEM_CD_GET_VOID_P(eve, cd_skin_radius_offset));
    copy_v2_v2(td->iloc, *radius);
    td->loc = *radius;
  }
  else {
    td->flag |= TD_SKIP;
  }

  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    td->flag |= TD_SELECTED;
  }

  copy_v3_v3(td->center, mesh_skin_transdata_center(island_data, island_index, eve));
  td->extra = eve;
}

static void createTransMeshSkin(bContext * /*C*/, TransInfo *t)
{
  BLI_assert(t->mode == TFM_SKIN_RESIZE);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    Mesh *mesh = id_cast<Mesh *>(tc->obedit->data);
    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;
    float mtx[3][3], smtx[3][3];
    int a;
    const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;

    TransIslandData island_data = {nullptr};
    TransMirrorData mirror_data = {nullptr};
    TransMeshDataCrazySpace crazyspace_data = {nullptr};

    /**
     * Quick check if we can transform.
     *
     * \note ignore modes here, even in edge/face modes,
     * transform data is created by selected vertices.
     */

    const int cd_skin_radius_offset = CustomData_get_offset_named(
        &bm->vdata, CD_PROP_FLOAT2, "skin_modifier_radius");
    if (cd_skin_radius_offset == -1) {
      continue;
    }

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if ((!prop_mode || (prop_mode & T_PROP_CONNECTED)) && (bm->totvertsel == 0)) {
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

    copy_m3_m4(mtx, tc->obedit->object_to_world().ptr());
    /* We use a pseudo-inverse so that when one of the axes is scaled to 0,
     * matrix inversion still works and we can still moving along the other. */
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    /* Original index of our connected vertex when connected distances are calculated.
     * Optional, allocate if needed. */
    int *dists_index = nullptr;
    float *dists = nullptr;
    if (prop_mode & T_PROP_CONNECTED) {
      dists = MEM_new_array_uninitialized<float>(bm->totvert, __func__);
      if (is_island_center) {
        dists_index = MEM_new_array_uninitialized<int>(bm->totvert, __func__);
      }
      transform_convert_mesh_connectivity_distance(em->bm, mtx, dists, dists_index);
    }

    /* Create TransDataMirror. */
    if (tc->use_mirror_axis_any) {
      bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;
      bool use_select = (t->flag & T_PROP_EDIT) == 0;
      const bool mirror_axis[3] = {
          bool(tc->use_mirror_axis_x), bool(tc->use_mirror_axis_y), bool(tc->use_mirror_axis_z)};
      transform_convert_mesh_mirrordata_calc(
          em, use_select, use_topology, mirror_axis, &mirror_data);

      if (mirror_data.vert_map) {
        tc->data_mirror_len = mirror_data.mirror_elem_len;
        tc->data_mirror = MEM_new_array_zeroed<TransDataMirror>(mirror_data.mirror_elem_len,
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

    /* Detect CrazySpace [TM]. */
    transform_convert_mesh_crazyspace_detect(t, tc, em, &crazyspace_data);

    /* Create TransData. */
    BLI_assert(data_len >= 1);
    tc->data_len = data_len;
    tc->data = MEM_new_array_zeroed<TransData>(data_len, "TransObData(Mesh EditMode)");

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

      if (mirror_data.vert_map && mirror_data.vert_map[a].index != -1) {
        mesh_skin_transdata_create(static_cast<TransDataBasic *>(td_mirror),
                                   eve,
                                   &island_data,
                                   island_index,
                                   cd_skin_radius_offset);

        int elem_index = mirror_data.vert_map[a].index;
        BMVert *v_src = BM_vert_at_index(bm, elem_index);
        float2 *radius_src = static_cast<float2 *>(
            BM_ELEM_CD_GET_VOID_P(v_src, cd_skin_radius_offset));

        td_mirror->flag |= mirror_data.vert_map[a].flag;
        td_mirror->loc_src = *radius_src;
        td_mirror++;
      }
      else if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        mesh_skin_transdata_create(static_cast<TransDataBasic *>(td),
                                   eve,
                                   &island_data,
                                   island_index,
                                   cd_skin_radius_offset);

        if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
          createSpaceNormal(td->axismtx, eve->no);
        }
        else {
          /* Setting normals. */
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

        /* CrazySpace. */
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
    transform_convert_mesh_mirrordata_free(&mirror_data);
    transform_convert_mesh_crazyspace_free(&crazyspace_data);
    if (dists) {
      MEM_delete(dists);
    }
    if (dists_index) {
      MEM_delete(dists_index);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Mesh Data
 * \{ */

static void mesh_skin_apply_to_mirror(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->use_mirror_axis_any) {
      TransDataMirror *td_mirror = tc->data_mirror;
      for (int i = 0; i < tc->data_mirror_len; i++, td_mirror++) {
        copy_v3_v3(td_mirror->loc, td_mirror->loc_src);
      }
    }
  }
}

static void recalcData_mesh_skin(TransInfo *t)
{
  bool is_canceling = t->state == TRANS_CANCEL;
  /* Mirror modifier clipping? */
  if (!is_canceling) {
    if (!(t->flag & T_NO_MIRROR)) {
      mesh_skin_apply_to_mirror(t);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    DEG_id_tag_update(tc->obedit->data, ID_RECALC_GEOMETRY);
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BKE_editmesh_looptris_and_normals_calc(em);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_MeshSkin = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransMeshSkin,
    /*recalc_data*/ recalcData_mesh_skin,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform
