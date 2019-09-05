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
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_linklist_stack.h"

#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "WM_api.h" /* for WM_event_add_notifier to deal with stabilization nodes */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "transform.h"
#include "transform_convert.h"
#include "bmesh.h"

/* when transforming islands */
struct TransIslandData {
  float co[3];
  float axismtx[3][3];
};

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Verts Transform Creation
 *
 * \{ */

static bool bmesh_test_dist_add(BMVert *v,
                                BMVert *v_other,
                                float *dists,
                                const float *dists_prev,
                                /* optionally track original index */
                                int *index,
                                const int *index_prev,
                                float mtx[3][3])
{
  if ((BM_elem_flag_test(v_other, BM_ELEM_SELECT) == 0) &&
      (BM_elem_flag_test(v_other, BM_ELEM_HIDDEN) == 0)) {
    const int i = BM_elem_index_get(v);
    const int i_other = BM_elem_index_get(v_other);
    float vec[3];
    float dist_other;
    sub_v3_v3v3(vec, v->co, v_other->co);
    mul_m3_v3(mtx, vec);

    dist_other = dists_prev[i] + len_v3(vec);
    if (dist_other < dists[i_other]) {
      dists[i_other] = dist_other;
      if (index != NULL) {
        index[i_other] = index_prev[i];
      }
      return true;
    }
  }

  return false;
}

/**
 * \param mtx: Measure distance in this space.
 * \param dists: Store the closest connected distance to selected vertices.
 * \param index: Optionally store the original index we're measuring the distance to (can be NULL).
 */
static void editmesh_set_connectivity_distance(BMesh *bm,
                                               float mtx[3][3],
                                               float *dists,
                                               int *index)
{
  BLI_LINKSTACK_DECLARE(queue, BMVert *);

  /* any BM_ELEM_TAG'd vertex is in 'queue_next', so we don't add in twice */
  BLI_LINKSTACK_DECLARE(queue_next, BMVert *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  {
    BMIter viter;
    BMVert *v;
    int i;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      float dist;
      BM_elem_index_set(v, i); /* set_inline */
      BM_elem_flag_disable(v, BM_ELEM_TAG);

      if (BM_elem_flag_test(v, BM_ELEM_SELECT) == 0 || BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        dist = FLT_MAX;
        if (index != NULL) {
          index[i] = i;
        }
      }
      else {
        BLI_LINKSTACK_PUSH(queue, v);
        dist = 0.0f;
        if (index != NULL) {
          index[i] = i;
        }
      }

      dists[i] = dist;
    }
    bm->elem_index_dirty &= ~BM_VERT;
  }

  /* need to be very careful of feedback loops here, store previous dist's to avoid feedback */
  float *dists_prev = MEM_dupallocN(dists);
  int *index_prev = MEM_dupallocN(index); /* may be NULL */

  do {
    BMVert *v;
    LinkNode *lnk;

    /* this is correct but slow to do each iteration,
     * instead sync the dist's while clearing BM_ELEM_TAG (below) */
#if 0
    memcpy(dists_prev, dists, sizeof(float) * bm->totvert);
#endif

    while ((v = BLI_LINKSTACK_POP(queue))) {
      BLI_assert(dists[BM_elem_index_get(v)] != FLT_MAX);

      /* connected edge-verts */
      if (v->e != NULL) {
        BMEdge *e_iter, *e_first;

        e_iter = e_first = v->e;

        /* would normally use BM_EDGES_OF_VERT, but this runs so often,
         * its faster to iterate on the data directly */
        do {

          if (BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN) == 0) {

            /* edge distance */
            {
              BMVert *v_other = BM_edge_other_vert(e_iter, v);
              if (bmesh_test_dist_add(v, v_other, dists, dists_prev, index, index_prev, mtx)) {
                if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == 0) {
                  BM_elem_flag_enable(v_other, BM_ELEM_TAG);
                  BLI_LINKSTACK_PUSH(queue_next, v_other);
                }
              }
            }

            /* face distance */
            if (e_iter->l) {
              BMLoop *l_iter_radial, *l_first_radial;
              /**
               * imaginary edge diagonally across quad,
               * \note, this takes advantage of the rules of winding that we
               * know 2 or more of a verts edges wont reference the same face twice.
               * Also, if the edge is hidden, the face will be hidden too.
               */
              l_iter_radial = l_first_radial = e_iter->l;

              do {
                if ((l_iter_radial->v == v) && (l_iter_radial->f->len == 4) &&
                    (BM_elem_flag_test(l_iter_radial->f, BM_ELEM_HIDDEN) == 0)) {
                  BMVert *v_other = l_iter_radial->next->next->v;
                  if (bmesh_test_dist_add(v, v_other, dists, dists_prev, index, index_prev, mtx)) {
                    if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == 0) {
                      BM_elem_flag_enable(v_other, BM_ELEM_TAG);
                      BLI_LINKSTACK_PUSH(queue_next, v_other);
                    }
                  }
                }
              } while ((l_iter_radial = l_iter_radial->radial_next) != l_first_radial);
            }
          }
        } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);
      }
    }

    /* clear for the next loop */
    for (lnk = queue_next; lnk; lnk = lnk->next) {
      BMVert *v_link = lnk->link;
      const int i = BM_elem_index_get(v_link);

      BM_elem_flag_disable(v_link, BM_ELEM_TAG);

      /* keep in sync, avoid having to do full memcpy each iteration */
      dists_prev[i] = dists[i];
      if (index != NULL) {
        index_prev[i] = index[i];
      }
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

    /* none should be tagged now since 'queue_next' is empty */
    BLI_assert(BM_iter_mesh_count_flag(BM_VERTS_OF_MESH, bm, BM_ELEM_TAG, true) == 0);

  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);

  MEM_freeN(dists_prev);
  if (index_prev != NULL) {
    MEM_freeN(index_prev);
  }
}

static struct TransIslandData *editmesh_islands_info_calc(BMEditMesh *em,
                                                          int *r_island_tot,
                                                          int **r_island_vert_map,
                                                          bool calc_single_islands)
{
  BMesh *bm = em->bm;
  struct TransIslandData *trans_islands;
  char htype;
  char itype;
  int i;

  /* group vars */
  int *groups_array;
  int(*group_index)[2];
  int group_tot;
  void **ele_array;

  int *vert_map;

  if (em->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
    groups_array = MEM_mallocN(sizeof(*groups_array) * bm->totedgesel, __func__);
    group_tot = BM_mesh_calc_edge_groups(
        bm, groups_array, &group_index, NULL, NULL, BM_ELEM_SELECT);

    htype = BM_EDGE;
    itype = BM_VERTS_OF_EDGE;
  }
  else { /* (bm->selectmode & SCE_SELECT_FACE) */
    groups_array = MEM_mallocN(sizeof(*groups_array) * bm->totfacesel, __func__);
    group_tot = BM_mesh_calc_face_groups(
        bm, groups_array, &group_index, NULL, NULL, BM_ELEM_SELECT, BM_VERT);

    htype = BM_FACE;
    itype = BM_VERTS_OF_FACE;
  }

  trans_islands = MEM_mallocN(sizeof(*trans_islands) * group_tot, __func__);

  vert_map = MEM_mallocN(sizeof(*vert_map) * bm->totvert, __func__);
  /* we shouldn't need this, but with incorrect selection flushing
   * its possible we have a selected vertex that's not in a face,
   * for now best not crash in that case. */
  copy_vn_i(vert_map, bm->totvert, -1);

  BM_mesh_elem_table_ensure(bm, htype);
  ele_array = (htype == BM_FACE) ? (void **)bm->ftable : (void **)bm->etable;

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  /* may be an edge OR a face array */
  for (i = 0; i < group_tot; i++) {
    BMEditSelection ese = {NULL};

    const int fg_sta = group_index[i][0];
    const int fg_len = group_index[i][1];
    float co[3], no[3], tangent[3];
    int j;

    zero_v3(co);
    zero_v3(no);
    zero_v3(tangent);

    ese.htype = htype;

    /* loop on each face in this group:
     * - assign r_vert_map
     * - calculate (co, no)
     */
    for (j = 0; j < fg_len; j++) {
      float tmp_co[3], tmp_no[3], tmp_tangent[3];

      ese.ele = ele_array[groups_array[fg_sta + j]];

      BM_editselection_center(&ese, tmp_co);
      BM_editselection_normal(&ese, tmp_no);
      BM_editselection_plane(&ese, tmp_tangent);

      add_v3_v3(co, tmp_co);
      add_v3_v3(no, tmp_no);
      add_v3_v3(tangent, tmp_tangent);

      {
        /* setup vertex map */
        BMIter iter;
        BMVert *v;

        /* connected edge-verts */
        BM_ITER_ELEM (v, &iter, ese.ele, itype) {
          vert_map[BM_elem_index_get(v)] = i;
        }
      }
    }

    mul_v3_v3fl(trans_islands[i].co, co, 1.0f / (float)fg_len);

    if (createSpaceNormalTangent(trans_islands[i].axismtx, no, tangent)) {
      /* pass */
    }
    else {
      if (normalize_v3(no) != 0.0f) {
        axis_dominant_v3_to_m3(trans_islands[i].axismtx, no);
        invert_m3(trans_islands[i].axismtx);
      }
      else {
        unit_m3(trans_islands[i].axismtx);
      }
    }
  }

  MEM_freeN(groups_array);
  MEM_freeN(group_index);

  /* for PET we need islands of 1 so connected vertices can use it with V3D_AROUND_LOCAL_ORIGINS */
  if (calc_single_islands) {
    BMIter viter;
    BMVert *v;
    int group_tot_single = 0;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (vert_map[i] == -1)) {
        group_tot_single += 1;
      }
    }

    if (group_tot_single != 0) {
      trans_islands = MEM_reallocN(trans_islands,
                                   sizeof(*trans_islands) * (group_tot + group_tot_single));

      BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (vert_map[i] == -1)) {
          struct TransIslandData *v_island = &trans_islands[group_tot];
          vert_map[i] = group_tot;

          copy_v3_v3(v_island->co, v->co);

          if (is_zero_v3(v->no) != 0.0f) {
            axis_dominant_v3_to_m3(v_island->axismtx, v->no);
            invert_m3(v_island->axismtx);
          }
          else {
            unit_m3(v_island->axismtx);
          }

          group_tot += 1;
        }
      }
    }
  }

  *r_island_tot = group_tot;
  *r_island_vert_map = vert_map;

  return trans_islands;
}

/* way to overwrite what data is edited with transform */
static void VertsToTransData(TransInfo *t,
                             TransData *td,
                             TransDataExtension *tx,
                             BMEditMesh *em,
                             BMVert *eve,
                             float *bweight,
                             struct TransIslandData *v_island,
                             const bool no_island_center)
{
  float *no, _no[3];
  BLI_assert(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) == 0);

  td->flag = 0;
  // if (key)
  //  td->loc = key->co;
  // else
  td->loc = eve->co;
  copy_v3_v3(td->iloc, td->loc);

  if ((t->mode == TFM_SHRINKFATTEN) && (em->selectmode & SCE_SELECT_FACE) &&
      BM_elem_flag_test(eve, BM_ELEM_SELECT) &&
      (BM_vert_calc_normal_ex(eve, BM_ELEM_SELECT, _no))) {
    no = _no;
  }
  else {
    no = eve->no;
  }

  if (v_island) {
    if (no_island_center) {
      copy_v3_v3(td->center, td->loc);
    }
    else {
      copy_v3_v3(td->center, v_island->co);
    }
    copy_m3_m3(td->axismtx, v_island->axismtx);
  }
  else if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
    copy_v3_v3(td->center, td->loc);
    createSpaceNormal(td->axismtx, no);
  }
  else {
    copy_v3_v3(td->center, td->loc);

    /* Setting normals */
    copy_v3_v3(td->axismtx[2], no);
    td->axismtx[0][0] = td->axismtx[0][1] = td->axismtx[0][2] = td->axismtx[1][0] =
        td->axismtx[1][1] = td->axismtx[1][2] = 0.0f;
  }

  td->ext = NULL;
  td->val = NULL;
  td->extra = NULL;
  if (t->mode == TFM_BWEIGHT) {
    td->val = bweight;
    td->ival = *bweight;
  }
  else if (t->mode == TFM_SKIN_RESIZE) {
    MVertSkin *vs = CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_MVERT_SKIN);
    if (vs) {
      /* skin node size */
      td->ext = tx;
      copy_v3_v3(tx->isize, vs->radius);
      tx->size = vs->radius;
      td->val = vs->radius;
    }
    else {
      td->flag |= TD_SKIP;
    }
  }
  else if (t->mode == TFM_SHRINKFATTEN) {
    td->ext = tx;
    tx->isize[0] = BM_vert_calc_shell_factor_ex(eve, no, BM_ELEM_SELECT);
  }
}

void createTransEditVerts(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *tob = NULL;
    TransDataExtension *tx = NULL;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    Mesh *me = tc->obedit->data;
    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;
    float(*mappedcos)[3] = NULL, (*quats)[4] = NULL;
    float mtx[3][3], smtx[3][3], (*defmats)[3][3] = NULL, (*defcos)[3] = NULL;
    float *dists = NULL;
    int a;
    const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;
    int mirror = 0;
    int cd_vert_bweight_offset = -1;
    bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

    struct TransIslandData *island_info = NULL;
    int island_info_tot;
    int *island_vert_map = NULL;

    /* Snap rotation along normal needs a common axis for whole islands,
     * otherwise one get random crazy results, see T59104.
     * However, we do not want to use the island center for the pivot/translation reference. */
    const bool is_snap_rotate = ((t->mode == TFM_TRANSLATION) &&
                                 /* There is not guarantee that snapping
                                  * is initialized yet at this point... */
                                 (usingSnappingNormal(t) ||
                                  (t->settings->snap_flag & SCE_SNAP_ROTATE) != 0) &&
                                 (t->around != V3D_AROUND_LOCAL_ORIGINS));
    /* Even for translation this is needed because of island-orientation, see: T51651. */
    const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS) || is_snap_rotate;
    /* Original index of our connected vertex when connected distances are calculated.
     * Optional, allocate if needed. */
    int *dists_index = NULL;

    if (tc->mirror.axis_flag) {
      EDBM_verts_mirror_cache_begin(em, 0, false, (t->flag & T_PROP_EDIT) == 0, use_topology);
      mirror = 1;
    }

    /**
     * Quick check if we can transform.
     *
     * \note ignore modes here, even in edge/face modes,
     * transform data is created by selected vertices.
     * \note in prop mode we need at least 1 selected.
     */
    if (bm->totvertsel == 0) {
      goto cleanup;
    }

    if (t->mode == TFM_BWEIGHT) {
      BM_mesh_cd_flag_ensure(bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_VERT_BWEIGHT);
      cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
    }

    if (prop_mode) {
      unsigned int count = 0;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          count++;
        }
      }

      tc->data_len = count;

      /* allocating scratch arrays */
      if (prop_mode & T_PROP_CONNECTED) {
        dists = MEM_mallocN(em->bm->totvert * sizeof(float), __func__);
        if (is_island_center) {
          dists_index = MEM_mallocN(em->bm->totvert * sizeof(int), __func__);
        }
      }
    }
    else {
      tc->data_len = bm->totvertsel;
    }

    tob = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Mesh EditMode)");
    if (ELEM(t->mode, TFM_SKIN_RESIZE, TFM_SHRINKFATTEN)) {
      /* warning, this is overkill, we only need 2 extra floats,
       * but this stores loads of extra stuff, for TFM_SHRINKFATTEN its even more overkill
       * since we may not use the 'alt' transform mode to maintain shell thickness,
       * but with generic transform code its hard to lazy init vars */
      tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                      "TransObData ext");
    }

    copy_m3_m4(mtx, tc->obedit->obmat);
    /* we use a pseudo-inverse so that when one of the axes is scaled to 0,
     * matrix inversion still works and we can still moving along the other */
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    if (prop_mode & T_PROP_CONNECTED) {
      editmesh_set_connectivity_distance(em->bm, mtx, dists, dists_index);
    }

    if (is_island_center) {
      /* In this specific case, near-by vertices will need to know
       * the island of the nearest connected vertex. */
      const bool calc_single_islands = ((prop_mode & T_PROP_CONNECTED) &&
                                        (t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                        (em->selectmode & SCE_SELECT_VERTEX));

      island_info = editmesh_islands_info_calc(
          em, &island_info_tot, &island_vert_map, calc_single_islands);
    }

    /* detect CrazySpace [tm] */
    if (modifiers_getCageIndex(t->scene, tc->obedit, NULL, 1) != -1) {
      int totleft = -1;
      if (modifiers_isCorrectableDeformed(t->scene, tc->obedit)) {
        BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

        /* Use evaluated state because we need b-bone cache. */
        Scene *scene_eval = (Scene *)DEG_get_evaluated_id(t->depsgraph, &t->scene->id);
        Object *obedit_eval = (Object *)DEG_get_evaluated_id(t->depsgraph, &tc->obedit->id);
        BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
        /* check if we can use deform matrices for modifier from the
         * start up to stack, they are more accurate than quats */
        totleft = BKE_crazyspace_get_first_deform_matrices_editbmesh(
            t->depsgraph, scene_eval, obedit_eval, em_eval, &defmats, &defcos);
      }

      /* if we still have more modifiers, also do crazyspace
       * correction with quats, relative to the coordinates after
       * the modifiers that support deform matrices (defcos) */

#if 0 /* TODO, fix crazyspace+extrude so it can be enabled for general use - campbell */
      if ((totleft > 0) || (totleft == -1))
#else
      if (totleft > 0)
#endif
      {
        mappedcos = BKE_crazyspace_get_mapped_editverts(t->depsgraph, tc->obedit);
        quats = MEM_mallocN(em->bm->totvert * sizeof(*quats), "crazy quats");
        BKE_crazyspace_set_quats_editmesh(em, defcos, mappedcos, quats, !prop_mode);
        if (mappedcos) {
          MEM_freeN(mappedcos);
        }
      }

      if (defcos) {
        MEM_freeN(defcos);
      }
    }

    /* find out which half we do */
    if (mirror) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && eve->co[0] != 0.0f) {
          if (eve->co[0] < 0.0f) {
            tc->mirror.sign = -1.0f;
            mirror = -1;
          }
          break;
        }
      }
    }

    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
      if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          struct TransIslandData *v_island = NULL;
          float *bweight = (cd_vert_bweight_offset != -1) ?
                               BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset) :
                               NULL;

          if (island_info) {
            const int connected_index = (dists_index && dists_index[a] != -1) ? dists_index[a] : a;
            v_island = (island_vert_map[connected_index] != -1) ?
                           &island_info[island_vert_map[connected_index]] :
                           NULL;
          }

          /* Do not use the island center in case we are using islands
           * only to get axis for snap/rotate to normal... */
          VertsToTransData(t, tob, tx, em, eve, bweight, v_island, is_snap_rotate);
          if (tx) {
            tx++;
          }

          /* selected */
          if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            tob->flag |= TD_SELECTED;
          }

          if (prop_mode) {
            if (prop_mode & T_PROP_CONNECTED) {
              tob->dist = dists[a];
            }
            else {
              tob->flag |= TD_NOTCONNECTED;
              tob->dist = FLT_MAX;
            }
          }

          /* CrazySpace */
          const bool use_quats = quats && BM_elem_flag_test(eve, BM_ELEM_TAG);
          if (use_quats || defmats) {
            float mat[3][3], qmat[3][3], imat[3][3];

            /* Use both or either quat and defmat correction. */
            if (use_quats) {
              quat_to_mat3(qmat, quats[BM_elem_index_get(eve)]);

              if (defmats) {
                mul_m3_series(mat, defmats[a], qmat, mtx);
              }
              else {
                mul_m3_m3m3(mat, mtx, qmat);
              }
            }
            else {
              mul_m3_m3m3(mat, mtx, defmats[a]);
            }

            invert_m3_m3(imat, mat);

            copy_m3_m3(tob->smtx, imat);
            copy_m3_m3(tob->mtx, mat);
          }
          else {
            copy_m3_m3(tob->smtx, smtx);
            copy_m3_m3(tob->mtx, mtx);
          }

          /* Mirror? */
          if ((mirror > 0 && tob->iloc[0] > 0.0f) || (mirror < 0 && tob->iloc[0] < 0.0f)) {
            BMVert *vmir = EDBM_verts_mirror_get(em, eve);  // t->obedit, em, eve, tob->iloc, a);
            if (vmir && vmir != eve) {
              tob->extra = vmir;
            }
          }
          tob++;
        }
      }
    }

    if (island_info) {
      MEM_freeN(island_info);
      MEM_freeN(island_vert_map);
    }

    if (mirror != 0) {
      tob = tc->data;
      for (a = 0; a < tc->data_len; a++, tob++) {
        if (ABS(tob->loc[0]) <= 0.00001f) {
          tob->flag |= TD_MIRROR_EDGE;
        }
      }
    }

  cleanup:
    /* crazy space free */
    if (quats) {
      MEM_freeN(quats);
    }
    if (defmats) {
      MEM_freeN(defmats);
    }
    if (dists) {
      MEM_freeN(dists);
    }
    if (dists_index) {
      MEM_freeN(dists_index);
    }

    if (tc->mirror.axis_flag) {
      EDBM_verts_mirror_cache_end(em);
    }
  }
}

/** \} */

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

    if (countsel == 0) {
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

/* -------------------------------------------------------------------- */
/** \name UVs Transform Creation
 *
 * \{ */

static void UVsToTransData(const float aspect[2],
                           TransData *td,
                           TransData2D *td2d,
                           float *uv,
                           const float *center,
                           bool selected)
{
  /* uv coords are scaled by aspects. this is needed for rotations and
   * proportional editing to be consistent with the stretched uv coords
   * that are displayed. this also means that for display and numinput,
   * and when the uv coords are flushed, these are converted each time */
  td2d->loc[0] = uv[0] * aspect[0];
  td2d->loc[1] = uv[1] * aspect[1];
  td2d->loc[2] = 0.0f;
  td2d->loc2d = uv;

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v2_v2(td->center, center ? center : td->loc);
  td->center[2] = 0.0f;
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  if (selected) {
    td->flag |= TD_SELECTED;
    td->dist = 0.0;
  }
  else {
    td->dist = FLT_MAX;
  }
  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

void createTransUVs(bContext *C, TransInfo *t)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = CTX_data_edit_image(C);
  Scene *scene = t->scene;
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = NULL;
    TransData2D *td2d = NULL;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMFace *efa;
    BMIter iter, liter;
    UvElementMap *elementmap = NULL;
    BLI_bitmap *island_enabled = NULL;
    struct {
      float co[2];
      int co_num;
    } *island_center = NULL;
    int count = 0, countsel = 0, count_rejected = 0;
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (!ED_space_image_show_uvedit(sima, tc->obedit)) {
      continue;
    }

    /* count */
    if (is_prop_connected || is_island_center) {
      /* create element map with island information */
      const bool use_facesel = (ts->uv_flag & UV_SYNC_SELECTION) == 0;
      elementmap = BM_uv_element_map_create(em->bm, use_facesel, false, true);
      if (elementmap == NULL) {
        return;
      }

      if (is_prop_connected) {
        island_enabled = BLI_BITMAP_NEW(elementmap->totalIslands, "TransIslandData(UV Editing)");
      }

      if (is_island_center) {
        island_center = MEM_callocN(sizeof(*island_center) * elementmap->totalIslands, __func__);
      }
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!uvedit_face_visible_test(scene, tc->obedit, ima, efa)) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        continue;
      }

      BM_elem_flag_enable(efa, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          countsel++;

          if (is_prop_connected || island_center) {
            UvElement *element = BM_uv_element_get(elementmap, efa, l);

            if (is_prop_connected) {
              BLI_BITMAP_ENABLE(island_enabled, element->island);
            }

            if (is_island_center) {
              if (element->flag == false) {
                MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
                add_v2_v2(island_center[element->island].co, luv->uv);
                island_center[element->island].co_num++;
                element->flag = true;
              }
            }
          }
        }

        if (is_prop_edit) {
          count++;
        }
      }
    }

    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
      goto finally;
    }

    if (is_island_center) {
      int i;

      for (i = 0; i < elementmap->totalIslands; i++) {
        mul_v2_fl(island_center[i].co, 1.0f / island_center[i].co_num);
        mul_v2_v2(island_center[i].co, t->aspect);
      }
    }

    tc->data_len = (is_prop_edit) ? count : countsel;
    tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(UV Editing)");
    /* for each 2d uv coord a 3d vector is allocated, so that they can be
     * treated just as if they were 3d verts */
    tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransObData2D(UV Editing)");

    if (sima->flag & SI_CLIP_UV) {
      t->flag |= T_CLIP_UV;
    }

    td = tc->data;
    td2d = tc->data_2d;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const bool selected = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
        MLoopUV *luv;
        const float *center = NULL;

        if (!is_prop_edit && !selected) {
          continue;
        }

        if (is_prop_connected || is_island_center) {
          UvElement *element = BM_uv_element_get(elementmap, efa, l);

          if (is_prop_connected) {
            if (!BLI_BITMAP_TEST(island_enabled, element->island)) {
              count_rejected++;
              continue;
            }
          }

          if (is_island_center) {
            center = island_center[element->island].co;
          }
        }

        BM_elem_flag_enable(l, BM_ELEM_TAG);
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        UVsToTransData(t->aspect, td++, td2d++, luv->uv, center, selected);
      }
    }

    if (is_prop_connected) {
      tc->data_len -= count_rejected;
    }

    if (sima->flag & SI_LIVE_UNWRAP) {
      ED_uvedit_live_unwrap_begin(t->scene, tc->obedit);
    }

  finally:
    if (is_prop_connected || is_island_center) {
      BM_uv_element_map_free(elementmap);

      if (is_prop_connected) {
        MEM_freeN(island_enabled);
      }

      if (island_center) {
        MEM_freeN(island_center);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 *
 * \{ */

void flushTransUVs(TransInfo *t)
{
  SpaceImage *sima = t->sa->spacedata.first;
  const bool use_pixel_snap = ((sima->pixel_snap_mode != SI_PIXEL_SNAP_DISABLED) &&
                               (t->state != TRANS_CANCEL));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData2D *td;
    int a;
    float aspect_inv[2], size[2];

    aspect_inv[0] = 1.0f / t->aspect[0];
    aspect_inv[1] = 1.0f / t->aspect[1];

    if (use_pixel_snap) {
      int size_i[2];
      ED_space_image_get_size(sima, &size_i[0], &size_i[1]);
      size[0] = size_i[0];
      size[1] = size_i[1];
    }

    /* flush to 2d vector from internally used 3d vector */
    for (a = 0, td = tc->data_2d; a < tc->data_len; a++, td++) {
      td->loc2d[0] = td->loc[0] * aspect_inv[0];
      td->loc2d[1] = td->loc[1] * aspect_inv[1];

      if (use_pixel_snap) {
        td->loc2d[0] *= size[0];
        td->loc2d[1] *= size[1];

        switch (sima->pixel_snap_mode) {
          case SI_PIXEL_SNAP_CENTER:
            td->loc2d[0] = roundf(td->loc2d[0] - 0.5f) + 0.5f;
            td->loc2d[1] = roundf(td->loc2d[1] - 0.5f) + 0.5f;
            break;
          case SI_PIXEL_SNAP_CORNER:
            td->loc2d[0] = roundf(td->loc2d[0]);
            td->loc2d[1] = roundf(td->loc2d[1]);
            break;
        }

        td->loc2d[0] /= size[0];
        td->loc2d[1] /= size[1];
      }
    }
  }
}

/** \} */
