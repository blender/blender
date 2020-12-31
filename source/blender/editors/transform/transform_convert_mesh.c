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

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_memarena.h"

#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "ED_mesh.h"

#include "DEG_depsgraph_query.h"

#include "transform.h"
#include "transform_orientations.h"
#include "transform_snap.h"

#include "transform_convert.h"

#define USE_FACE_SUBSTITUTE

/* -------------------------------------------------------------------- */
/** \name Island Creation
 *
 * \{ */

void transform_convert_mesh_islands_calc(struct BMEditMesh *em,
                                         const bool calc_single_islands,
                                         const bool calc_island_center,
                                         const bool calc_island_axismtx,
                                         struct TransIslandData *r_island_data)
{
  BMesh *bm = em->bm;
  char htype;
  char itype;
  int i;

  /* group vars */
  float(*center)[3] = NULL;
  float(*axismtx)[3][3] = NULL;
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
        bm, groups_array, &group_index, NULL, NULL, NULL, BM_ELEM_SELECT, BM_VERT);

    htype = BM_FACE;
    itype = BM_VERTS_OF_FACE;
  }

  if (calc_island_center) {
    center = MEM_mallocN(sizeof(*center) * group_tot, __func__);
  }

  if (calc_island_axismtx) {
    axismtx = MEM_mallocN(sizeof(*axismtx) * group_tot, __func__);
  }

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

    /* loop on each face or edge in this group:
     * - assign r_vert_map
     * - calculate (co, no)
     */
    for (j = 0; j < fg_len; j++) {
      ese.ele = ele_array[groups_array[fg_sta + j]];

      if (center) {
        float tmp_co[3];
        BM_editselection_center(&ese, tmp_co);
        add_v3_v3(co, tmp_co);
      }

      if (axismtx) {
        float tmp_no[3], tmp_tangent[3];
        BM_editselection_normal(&ese, tmp_no);
        BM_editselection_plane(&ese, tmp_tangent);
        add_v3_v3(no, tmp_no);
        add_v3_v3(tangent, tmp_tangent);
      }

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

    if (center) {
      mul_v3_v3fl(center[i], co, 1.0f / (float)fg_len);
    }

    if (axismtx) {
      if (createSpaceNormalTangent(axismtx[i], no, tangent)) {
        /* pass */
      }
      else {
        if (normalize_v3(no) != 0.0f) {
          axis_dominant_v3_to_m3(axismtx[i], no);
          invert_m3(axismtx[i]);
        }
        else {
          unit_m3(axismtx[i]);
        }
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
      if (center) {
        center = MEM_reallocN(center, sizeof(*center) * (group_tot + group_tot_single));
      }
      if (axismtx) {
        axismtx = MEM_reallocN(axismtx, sizeof(*axismtx) * (group_tot + group_tot_single));
      }

      BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (vert_map[i] == -1)) {
          vert_map[i] = group_tot;
          if (center) {
            copy_v3_v3(center[group_tot], v->co);
          }
          if (axismtx) {
            if (is_zero_v3(v->no) != 0.0f) {
              axis_dominant_v3_to_m3(axismtx[group_tot], v->no);
              invert_m3(axismtx[group_tot]);
            }
            else {
              unit_m3(axismtx[group_tot]);
            }
          }

          group_tot += 1;
        }
      }
    }
  }

  r_island_data->axismtx = axismtx;
  r_island_data->center = center;
  r_island_data->island_tot = group_tot;
  r_island_data->island_vert_map = vert_map;
}

void transform_convert_mesh_islanddata_free(struct TransIslandData *island_data)
{
  if (island_data->center) {
    MEM_freeN(island_data->center);
  }
  if (island_data->axismtx) {
    MEM_freeN(island_data->axismtx);
  }
  if (island_data->island_vert_map) {
    MEM_freeN(island_data->island_vert_map);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Connectivity Distance for Proportional Editing
 *
 * \{ */

/* Propagate distance from v1 and v2 to v0. */
static bool bmesh_test_dist_add(BMVert *v0,
                                BMVert *v1,
                                BMVert *v2,
                                float *dists,
                                /* optionally track original index */
                                int *index,
                                const float mtx[3][3])
{
  if ((BM_elem_flag_test(v0, BM_ELEM_SELECT) == 0) &&
      (BM_elem_flag_test(v0, BM_ELEM_HIDDEN) == 0)) {
    const int i0 = BM_elem_index_get(v0);
    const int i1 = BM_elem_index_get(v1);

    BLI_assert(dists[i1] != FLT_MAX);
    if (dists[i0] <= dists[i1]) {
      return false;
    }

    float dist0;

    if (v2) {
      /* Distance across triangle. */
      const int i2 = BM_elem_index_get(v2);
      BLI_assert(dists[i2] != FLT_MAX);
      if (dists[i0] <= dists[i2]) {
        return false;
      }

      float vm0[3], vm1[3], vm2[3];
      mul_v3_m3v3(vm0, mtx, v0->co);
      mul_v3_m3v3(vm1, mtx, v1->co);
      mul_v3_m3v3(vm2, mtx, v2->co);

      dist0 = geodesic_distance_propagate_across_triangle(vm0, vm1, vm2, dists[i1], dists[i2]);
    }
    else {
      /* Distance along edge. */
      float vec[3];
      sub_v3_v3v3(vec, v1->co, v0->co);
      mul_m3_v3(mtx, vec);

      dist0 = dists[i1] + len_v3(vec);
    }

    if (dist0 < dists[i0]) {
      dists[i0] = dist0;
      if (index != NULL) {
        index[i0] = index[i1];
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
void transform_convert_mesh_connectivity_distance(struct BMesh *bm,
                                                  const float mtx[3][3],
                                                  float *dists,
                                                  int *index)
{
  BLI_LINKSTACK_DECLARE(queue, BMEdge *);

  /* any BM_ELEM_TAG'd edge is in 'queue_next', so we don't add in twice */
  BLI_LINKSTACK_DECLARE(queue_next, BMEdge *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  {
    /* Set indexes and initial distances for selected vertices. */
    BMIter viter;
    BMVert *v;
    int i;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      float dist;
      BM_elem_index_set(v, i); /* set_inline */

      if (BM_elem_flag_test(v, BM_ELEM_SELECT) == 0 || BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        dist = FLT_MAX;
        if (index != NULL) {
          index[i] = i;
        }
      }
      else {
        dist = 0.0f;
        if (index != NULL) {
          index[i] = i;
        }
      }

      dists[i] = dist;
    }
    bm->elem_index_dirty &= ~BM_VERT;
  }

  {
    /* Add edges with at least one selected vertex to the queue. */
    BMIter eiter;
    BMEdge *e;

    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
      BMVert *v1 = e->v1;
      BMVert *v2 = e->v2;
      int i1 = BM_elem_index_get(v1);
      int i2 = BM_elem_index_get(v2);

      if (dists[i1] != FLT_MAX || dists[i2] != FLT_MAX) {
        BLI_LINKSTACK_PUSH(queue, e);
      }
      BM_elem_flag_disable(e, BM_ELEM_TAG);
    }
  }

  do {
    BMEdge *e;

    while ((e = BLI_LINKSTACK_POP(queue))) {
      BMVert *v1 = e->v1;
      BMVert *v2 = e->v2;
      int i1 = BM_elem_index_get(v1);
      int i2 = BM_elem_index_get(v2);

      if (e->l == NULL || (dists[i1] == FLT_MAX || dists[i2] == FLT_MAX)) {
        /* Propagate along edge from vertex with smallest to largest distance. */
        if (dists[i1] > dists[i2]) {
          SWAP(int, i1, i2);
          SWAP(BMVert *, v1, v2);
        }

        if (bmesh_test_dist_add(v2, v1, NULL, dists, index, mtx)) {
          /* Add adjacent loose edges to the queue, or all edges if this is a loose edge.
           * Other edges are handled by propagation across edges below. */
          BMEdge *e_other;
          BMIter eiter;
          BM_ITER_ELEM (e_other, &eiter, v2, BM_EDGES_OF_VERT) {
            if (e_other != e && BM_elem_flag_test(e_other, BM_ELEM_TAG) == 0 &&
                (e->l == NULL || e_other->l == NULL)) {
              BM_elem_flag_enable(e_other, BM_ELEM_TAG);
              BLI_LINKSTACK_PUSH(queue_next, e_other);
            }
          }
        }
      }

      if (e->l != NULL) {
        /* Propagate across edge to vertices in adjacent faces. */
        BMLoop *l;
        BMIter liter;
        BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
          for (BMLoop *l_other = l->next->next; l_other != l; l_other = l_other->next) {
            BMVert *v_other = l_other->v;
            BLI_assert(!ELEM(v_other, v1, v2));

            if (bmesh_test_dist_add(v_other, v1, v2, dists, index, mtx)) {
              /* Add adjacent edges to the queue, if they are ready to propagate across/along.
               * Always propagate along loose edges, and for other edges only propagate across
               * if both vertices have a known distances. */
              BMEdge *e_other;
              BMIter eiter;
              BM_ITER_ELEM (e_other, &eiter, v_other, BM_EDGES_OF_VERT) {
                if (e_other != e && BM_elem_flag_test(e_other, BM_ELEM_TAG) == 0 &&
                    (e_other->l == NULL ||
                     dists[BM_elem_index_get(BM_edge_other_vert(e_other, v_other))] != FLT_MAX)) {
                  BM_elem_flag_enable(e_other, BM_ELEM_TAG);
                  BLI_LINKSTACK_PUSH(queue_next, e_other);
                }
              }
            }
          }
        }
      }
    }

    /* Clear for the next loop. */
    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      BMEdge *e_link = lnk->link;

      BM_elem_flag_disable(e_link, BM_ELEM_TAG);
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

    /* None should be tagged now since 'queue_next' is empty. */
    BLI_assert(BM_iter_mesh_count_flag(BM_EDGES_OF_MESH, bm, BM_ELEM_TAG, true) == 0);
  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TransDataMirror Creation
 *
 * \{ */

/* Used for both mirror epsilon and TD_MIRROR_EDGE_ */
#define TRANSFORM_MAXDIST_MIRROR 0.00002f

static bool is_in_quadrant_v3(const float co[3], const int quadrant[3], const float epsilon)
{
  if (quadrant[0] && ((co[0] * quadrant[0]) < -epsilon)) {
    return false;
  }
  if (quadrant[1] && ((co[1] * quadrant[1]) < -epsilon)) {
    return false;
  }
  if (quadrant[2] && ((co[2] * quadrant[2]) < -epsilon)) {
    return false;
  }
  return true;
}

void transform_convert_mesh_mirrordata_calc(struct BMEditMesh *em,
                                            const bool use_select,
                                            const bool use_topology,
                                            const bool mirror_axis[3],
                                            struct TransMirrorData *r_mirror_data)
{
  struct MirrorDataVert *vert_map;

  BMesh *bm = em->bm;
  BMVert *eve;
  BMIter iter;
  int i, flag, totvert = bm->totvert;

  vert_map = MEM_callocN(totvert * sizeof(*vert_map), __func__);

  float select_sum[3] = {0};
  BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
    vert_map[i] = (struct MirrorDataVert){-1, 0};
    if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
      add_v3_v3(select_sum, eve->co);
    }
  }

  /* Tag only elements that will be transformed within the quadrant. */
  int quadrant[3];
  for (int a = 0; a < 3; a++) {
    if (mirror_axis[a]) {
      quadrant[a] = select_sum[a] >= 0.0f ? 1 : -1;
    }
    else {
      quadrant[a] = 0;
    }
  }

  uint mirror_elem_len = 0;
  int *index[3] = {NULL, NULL, NULL};
  bool is_single_mirror_axis = (mirror_axis[0] + mirror_axis[1] + mirror_axis[2]) == 1;
  bool test_selected_only = use_select && is_single_mirror_axis;
  for (int a = 0; a < 3; a++) {
    if (!mirror_axis[a]) {
      continue;
    }

    index[a] = MEM_mallocN(totvert * sizeof(*index[a]), __func__);
    EDBM_verts_mirror_cache_begin_ex(
        em, a, false, test_selected_only, true, use_topology, TRANSFORM_MAXDIST_MIRROR, index[a]);

    flag = TD_MIRROR_X << a;
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      int i_mirr = index[a][i];
      if (i_mirr < 0) {
        continue;
      }
      if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (use_select && !BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        continue;
      }
      if (!is_in_quadrant_v3(eve->co, quadrant, TRANSFORM_MAXDIST_MIRROR)) {
        continue;
      }
      if (vert_map[i_mirr].flag != 0) {
        /* One mirror per element.
         * It can happen when vertices occupy the same position. */
        continue;
      }

      vert_map[i_mirr] = (struct MirrorDataVert){i, flag};
      mirror_elem_len++;
    }
  }

  if (!mirror_elem_len) {
    MEM_freeN(vert_map);
    vert_map = NULL;
  }
  else if (!is_single_mirror_axis) {
    /* Adjustment for elements that are mirrors of mirrored elements. */
    for (int a = 0; a < 3; a++) {
      if (!mirror_axis[a]) {
        continue;
      }

      flag = TD_MIRROR_X << a;
      for (i = 0; i < totvert; i++) {
        int i_mirr = index[a][i];
        if (i_mirr < 0) {
          continue;
        }
        if (vert_map[i].index != -1 && !(vert_map[i].flag & flag)) {
          if (vert_map[i_mirr].index == -1) {
            mirror_elem_len++;
          }
          vert_map[i_mirr].index = vert_map[i].index;
          vert_map[i_mirr].flag |= vert_map[i].flag | flag;
        }
      }
    }
  }

  MEM_SAFE_FREE(index[0]);
  MEM_SAFE_FREE(index[1]);
  MEM_SAFE_FREE(index[2]);

  r_mirror_data->vert_map = vert_map;
  r_mirror_data->mirror_elem_len = mirror_elem_len;
}

void transform_convert_mesh_mirrordata_free(struct TransMirrorData *mirror_data)
{
  if (mirror_data->vert_map) {
    MEM_freeN(mirror_data->vert_map);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Crazy Space
 *
 * \{ */

/* Detect CrazySpace [tm].
 * Vertices with space affected by quats are marked with #BM_ELEM_TAG */
void transform_convert_mesh_crazyspace_detect(TransInfo *t,
                                              struct TransDataContainer *tc,
                                              struct BMEditMesh *em,
                                              struct TransMeshDataCrazySpace *r_crazyspace_data)
{
  float(*quats)[4] = NULL;
  float(*defmats)[3][3] = NULL;
  const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;
  if (BKE_modifiers_get_cage_index(t->scene, tc->obedit, NULL, 1) != -1) {
    float(*defcos)[3] = NULL;
    int totleft = -1;
    if (BKE_modifiers_is_correctable_deformed(t->scene, tc->obedit)) {
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

    /* If we still have more modifiers, also do crazy-space
     * correction with \a quats, relative to the coordinates after
     * the modifiers that support deform matrices \a defcos. */

#if 0 /* TODO, fix crazy-space & extrude so it can be enabled for general use - campbell */
      if ((totleft > 0) || (totleft == -1))
#else
    if (totleft > 0)
#endif
    {
      float(*mappedcos)[3] = NULL;
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
  r_crazyspace_data->quats = quats;
  r_crazyspace_data->defmats = defmats;
}

void transform_convert_mesh_crazyspace_transdata_set(const float mtx[3][3],
                                                     const float smtx[3][3],
                                                     const float defmat[3][3],
                                                     const float quat[4],
                                                     struct TransData *r_td)
{
  /* CrazySpace */
  if (quat || defmat) {
    float mat[3][3], qmat[3][3], imat[3][3];

    /* Use both or either quat and defmat correction. */
    if (quat) {
      quat_to_mat3(qmat, quat);

      if (defmat) {
        mul_m3_series(mat, defmat, qmat, mtx);
      }
      else {
        mul_m3_m3m3(mat, mtx, qmat);
      }
    }
    else {
      mul_m3_m3m3(mat, mtx, defmat);
    }

    invert_m3_m3(imat, mat);

    copy_m3_m3(r_td->smtx, imat);
    copy_m3_m3(r_td->mtx, mat);
  }
  else {
    copy_m3_m3(r_td->smtx, smtx);
    copy_m3_m3(r_td->mtx, mtx);
  }
}

void transform_convert_mesh_crazyspace_free(struct TransMeshDataCrazySpace *r_crazyspace_data)
{
  if (r_crazyspace_data->quats) {
    MEM_freeN(r_crazyspace_data->quats);
  }
  if (r_crazyspace_data->defmats) {
    MEM_freeN(r_crazyspace_data->defmats);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Verts Transform Creation
 *
 * \{ */

static void transdata_center_get(const struct TransIslandData *island_data,
                                 const int island_index,
                                 const float iloc[3],
                                 float r_center[3])
{
  if (island_data->center && island_index != -1) {
    copy_v3_v3(r_center, island_data->center[island_index]);
  }
  else {
    copy_v3_v3(r_center, iloc);
  }
}

/* way to overwrite what data is edited with transform */
static void VertsToTransData(TransInfo *t,
                             TransData *td,
                             TransDataExtension *tx,
                             BMEditMesh *em,
                             BMVert *eve,
                             float *bweight,
                             const struct TransIslandData *island_data,
                             const int island_index)
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

  transdata_center_get(island_data, island_index, td->iloc, td->center);

  if ((island_index != -1) && island_data->axismtx) {
    copy_m3_m3(td->axismtx, island_data->axismtx[island_index]);
  }
  else if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
    createSpaceNormal(td->axismtx, no);
  }
  else {
    /* Setting normals */
    copy_v3_v3(td->axismtx[2], no);
    td->axismtx[0][0] = td->axismtx[0][1] = td->axismtx[0][2] = td->axismtx[1][0] =
        td->axismtx[1][1] = td->axismtx[1][2] = 0.0f;
  }

  td->ext = NULL;
  td->val = NULL;
  td->extra = eve;
  if (t->mode == TFM_BWEIGHT) {
    td->val = bweight;
    td->ival = *bweight;
  }
  else if (t->mode == TFM_SHRINKFATTEN) {
    td->ext = tx;
    tx->isize[0] = BM_vert_calc_shell_factor_ex(eve, no, BM_ELEM_SELECT);
  }
}

void createTransEditVerts(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransDataExtension *tx = NULL;
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

    /**
     * Quick check if we can transform.
     *
     * \note ignore modes here, even in edge/face modes,
     * transform data is created by selected vertices.
     */

    /* Support other objects using PET to adjust these, unless connected is enabled. */
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
    if (is_island_center) {
      /* In this specific case, near-by vertices will need to know
       * the island of the nearest connected vertex. */
      const bool calc_single_islands = ((prop_mode & T_PROP_CONNECTED) &&
                                        (t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                        (em->selectmode & SCE_SELECT_VERTEX));

      const bool calc_island_center = !is_snap_rotate;
      /* The island axismtx is only necessary in some modes.
       * TODO(Germano): Extend the list to exclude other modes. */
      const bool calc_island_axismtx = !ELEM(t->mode, TFM_SHRINKFATTEN);

      transform_convert_mesh_islands_calc(
          em, calc_single_islands, calc_island_center, calc_island_axismtx, &island_data);
    }

    copy_m3_m4(mtx, tc->obedit->obmat);
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
    if (t->mode == TFM_SHRINKFATTEN) {
      /* warning, this is overkill, we only need 2 extra floats,
       * but this stores loads of extra stuff, for TFM_SHRINKFATTEN its even more overkill
       * since we may not use the 'alt' transform mode to maintain shell thickness,
       * but with generic transform code its hard to lazy init vars */
      tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                      "TransObData ext");
    }

    int cd_vert_bweight_offset = -1;
    if (t->mode == TFM_BWEIGHT) {
      BM_mesh_cd_flag_ensure(bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_VERT_BWEIGHT);
      cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
    }

    TransData *tob = tc->data;
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
        int elem_index = mirror_data.vert_map[a].index;
        BMVert *v_src = BM_vert_at_index(bm, elem_index);

        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          mirror_data.vert_map[a].flag |= TD_SELECTED;
        }

        td_mirror->extra = eve;
        td_mirror->loc = eve->co;
        copy_v3_v3(td_mirror->iloc, eve->co);
        td_mirror->flag = mirror_data.vert_map[a].flag;
        td_mirror->loc_src = v_src->co;
        transdata_center_get(&island_data, island_index, td_mirror->iloc, td_mirror->center);

        td_mirror++;
      }
      else if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        float *bweight = (cd_vert_bweight_offset != -1) ?
                             BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset) :
                             NULL;

        /* Do not use the island center in case we are using islands
         * only to get axis for snap/rotate to normal... */
        VertsToTransData(t, tob, tx, em, eve, bweight, &island_data, island_index);
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
        transform_convert_mesh_crazyspace_transdata_set(
            mtx,
            smtx,
            crazyspace_data.defmats ? crazyspace_data.defmats[a] : NULL,
            crazyspace_data.quats && BM_elem_flag_test(eve, BM_ELEM_TAG) ?
                crazyspace_data.quats[a] :
                NULL,
            tob);

        if (tc->use_mirror_axis_any) {
          if (tc->use_mirror_axis_x && fabsf(tob->loc[0]) < TRANSFORM_MAXDIST_MIRROR) {
            tob->flag |= TD_MIRROR_EDGE_X;
          }
          if (tc->use_mirror_axis_y && fabsf(tob->loc[1]) < TRANSFORM_MAXDIST_MIRROR) {
            tob->flag |= TD_MIRROR_EDGE_Y;
          }
          if (tc->use_mirror_axis_z && fabsf(tob->loc[2]) < TRANSFORM_MAXDIST_MIRROR) {
            tob->flag |= TD_MIRROR_EDGE_Z;
          }
        }

        tob++;
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
/** \name CustomData Layer Correction
 *
 * \{ */

struct TransCustomDataMergeGroup {
  /** map {BMVert: TransCustomDataLayerVert} */
  struct LinkNode **cd_loop_groups;
};

struct TransCustomDataLayer {
  BMesh *bm;
  struct MemArena *arena;

  struct GHash *origfaces;
  struct BMesh *bm_origfaces;

  /* Special handle for multi-resolution. */
  int cd_loop_mdisp_offset;

  /* Optionally merge custom-data groups (this keeps UVs connected for example). */
  struct {
    /** map {BMVert: TransDataBasic} */
    struct GHash *origverts;
    struct TransCustomDataMergeGroup *data;
    int data_len;
    /** Array size of 'layer_math_map_len'
     * maps #TransCustomDataLayerVert.cd_group index to absolute #CustomData layer index */
    int *customdatalayer_map;
    /** Number of math BMLoop layers. */
    int customdatalayer_map_len;
  } merge_group;

  bool use_merge_group;
};

static void mesh_customdatacorrect_free_cb(struct TransInfo *UNUSED(t),
                                           struct TransDataContainer *UNUSED(tc),
                                           struct TransCustomData *custom_data)
{
  struct TransCustomDataLayer *tcld = custom_data->data;
  bmesh_edit_end(tcld->bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);

  if (tcld->bm_origfaces) {
    BM_mesh_free(tcld->bm_origfaces);
  }
  if (tcld->origfaces) {
    BLI_ghash_free(tcld->origfaces, NULL, NULL);
  }
  if (tcld->merge_group.origverts) {
    BLI_ghash_free(tcld->merge_group.origverts, NULL, NULL);
  }
  if (tcld->arena) {
    BLI_memarena_free(tcld->arena);
  }
  if (tcld->merge_group.customdatalayer_map) {
    MEM_freeN(tcld->merge_group.customdatalayer_map);
  }

  MEM_freeN(tcld);
  custom_data->data = NULL;
}

#ifdef USE_FACE_SUBSTITUTE

#  define FACE_SUBSTITUTE_INDEX INT_MIN

/**
 * Search for a neighboring face with area and preferably without selected vertex.
 * Used to replace area-less faces in custom-data correction.
 */
static BMFace *mesh_customdatacorrect_find_best_face_substitute(BMFace *f)
{
  BMFace *best_face = NULL;
  BMLoop *l;
  BMIter liter;
  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    BMLoop *l_radial_next = l->radial_next;
    BMFace *f_test = l_radial_next->f;
    if (f_test == f) {
      continue;
    }
    if (is_zero_v3(f_test->no)) {
      continue;
    }

    /* Check the loops edge isn't selected. */
    if (!BM_elem_flag_test(l_radial_next->v, BM_ELEM_SELECT) &&
        !BM_elem_flag_test(l_radial_next->next->v, BM_ELEM_SELECT)) {
      /* Prefer edges with unselected vertices.
       * Useful for extrude. */
      best_face = f_test;
      break;
    }
    if (best_face == NULL) {
      best_face = f_test;
    }
  }
  return best_face;
}

static void mesh_customdatacorrect_face_substitute_set(struct TransCustomDataLayer *tcld,
                                                       BMFace *f,
                                                       BMFace *f_copy)
{
  BLI_assert(is_zero_v3(f->no));
  BMesh *bm = tcld->bm;
  /* It is impossible to calculate the loops weights of a face without area.
   * Find a substitute. */
  BMFace *f_substitute = mesh_customdatacorrect_find_best_face_substitute(f);
  if (f_substitute) {
    /* Copy the custom-data from the substitute face. */
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_interp_from_face(bm, l_iter, f_substitute, false, false);
    } while ((l_iter = l_iter->next) != l_first);

    /* Use the substitute face as the reference during the transformation. */
    BMFace *f_substitute_copy = BM_face_copy(tcld->bm_origfaces, bm, f_substitute, true, true);

    /* Hack: reference substitute face in `f_copy->no`.
     * `tcld->origfaces` is already used to restore the initial value. */
    BM_elem_index_set(f_copy, FACE_SUBSTITUTE_INDEX);
    *((BMFace **)&f_copy->no[0]) = f_substitute_copy;
  }
}

static BMFace *mesh_customdatacorrect_face_substitute_get(BMFace *f_copy)
{
  BLI_assert(BM_elem_index_get(f_copy) == FACE_SUBSTITUTE_INDEX);
  return *((BMFace **)&f_copy->no[0]);
}

#endif /* USE_FACE_SUBSTITUTE */

static void mesh_customdatacorrect_init_vert(struct TransCustomDataLayer *tcld,
                                             struct TransDataBasic *td,
                                             const int index)
{
  BMesh *bm = tcld->bm;
  BMVert *v = td->extra;
  BMIter liter;
  int j, l_num;
  float *loop_weights;

  // BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT) {
  BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, v);
  l_num = liter.count;
  loop_weights = tcld->use_merge_group ? BLI_array_alloca(loop_weights, l_num) : NULL;
  for (j = 0; j < l_num; j++) {
    BMLoop *l = BM_iter_step(&liter);
    BMLoop *l_prev, *l_next;

    /* Generic custom-data correction. Copy face data. */
    void **val_p;
    if (!BLI_ghash_ensure_p(tcld->origfaces, l->f, &val_p)) {
      BMFace *f_copy = BM_face_copy(tcld->bm_origfaces, bm, l->f, true, true);
      *val_p = f_copy;
#ifdef USE_FACE_SUBSTITUTE
      if (is_zero_v3(l->f->no)) {
        mesh_customdatacorrect_face_substitute_set(tcld, l->f, f_copy);
      }
#endif
    }

    if (tcld->use_merge_group) {
      if ((l_prev = BM_loop_find_prev_nodouble(l, l->next, FLT_EPSILON)) &&
          (l_next = BM_loop_find_next_nodouble(l, l_prev, FLT_EPSILON))) {
        loop_weights[j] = angle_v3v3v3(l_prev->v->co, l->v->co, l_next->v->co);
      }
      else {
        loop_weights[j] = 0.0f;
      }
    }
  }

  if (tcld->use_merge_group) {
    /* Store cd_loop_groups. */
    struct TransCustomDataMergeGroup *merge_data = &tcld->merge_group.data[index];
    if (l_num != 0) {
      merge_data->cd_loop_groups = BLI_memarena_alloc(
          tcld->arena, tcld->merge_group.customdatalayer_map_len * sizeof(void *));
      for (j = 0; j < tcld->merge_group.customdatalayer_map_len; j++) {
        const int layer_nr = tcld->merge_group.customdatalayer_map[j];
        merge_data->cd_loop_groups[j] = BM_vert_loop_groups_data_layer_create(
            bm, v, layer_nr, loop_weights, tcld->arena);
      }
    }
    else {
      merge_data->cd_loop_groups = NULL;
    }

    BLI_ghash_insert(tcld->merge_group.origverts, v, td);
  }
}

static void mesh_customdatacorrect_init_container_generic(TransDataContainer *UNUSED(tc),
                                                          struct TransCustomDataLayer *tcld)
{
  BMesh *bm = tcld->bm;

  struct GHash *origfaces = BLI_ghash_ptr_new(__func__);
  struct BMesh *bm_origfaces = BM_mesh_create(&bm_mesh_allocsize_default,
                                              &((struct BMeshCreateParams){
                                                  .use_toolflags = false,
                                              }));

  /* We need to have matching loop custom-data. */
  BM_mesh_copy_init_customdata_all_layers(bm_origfaces, bm, BM_LOOP, NULL);

  tcld->origfaces = origfaces;
  tcld->bm_origfaces = bm_origfaces;

  bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
  tcld->cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
}

static void mesh_customdatacorrect_init_container_merge_group(TransDataContainer *tc,
                                                              struct TransCustomDataLayer *tcld)
{
  BMesh *bm = tcld->bm;
  BLI_assert(CustomData_has_math(&bm->ldata));

  /* TODO: We don't need `layer_math_map` when there are no loops linked
   * to one of the sliding vertices. */

  /* Over allocate, only 'math' layers are indexed. */
  int *customdatalayer_map = MEM_mallocN(sizeof(int) * bm->ldata.totlayer, __func__);
  int layer_math_map_len = 0;
  for (int i = 0; i < bm->ldata.totlayer; i++) {
    if (CustomData_layer_has_math(&bm->ldata, i)) {
      customdatalayer_map[layer_math_map_len++] = i;
    }
  }
  BLI_assert(layer_math_map_len != 0);

  tcld->merge_group.data_len = tc->data_len + tc->data_mirror_len;
  tcld->merge_group.customdatalayer_map = customdatalayer_map;
  tcld->merge_group.customdatalayer_map_len = layer_math_map_len;
  tcld->merge_group.origverts = BLI_ghash_ptr_new_ex(__func__, tcld->merge_group.data_len);
  tcld->merge_group.data = BLI_memarena_alloc(
      tcld->arena, tcld->merge_group.data_len * sizeof(*tcld->merge_group.data));
}

static void mesh_customdatacorrect_init_container(TransDataContainer *tc,
                                                  const bool use_merge_group)
{
  if (tc->custom.type.data) {
    /* The custom-data correction has been initiated before.
     * Free since some modes have different settings. */
    mesh_customdatacorrect_free_cb(NULL, tc, &tc->custom.type);
  }

  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;

  if (bm->shapenr > 1) {
    /* Don't do this at all for non-basis shape keys, too easy to
     * accidentally break uv maps or vertex colors then */
    /* create copies of faces for custom-data projection. */
    return;
  }
  if (!CustomData_has_math(&bm->ldata) && !CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
    /* There is no custom-data to correct. */
    return;
  }

  struct TransCustomDataLayer *tcld = MEM_callocN(sizeof(*tcld), __func__);
  tcld->bm = bm;
  tcld->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  /* Init `cd_loop_mdisp_offset` to -1 to avoid problems with a valid index. */
  tcld->cd_loop_mdisp_offset = -1;
  tcld->use_merge_group = use_merge_group;

  mesh_customdatacorrect_init_container_generic(tc, tcld);

  if (tcld->use_merge_group) {
    mesh_customdatacorrect_init_container_merge_group(tc, tcld);
  }

  {
    /* Setup Verts. */
    int i = 0;

    TransData *tob = tc->data;
    for (int j = tc->data_len; j--; tob++, i++) {
      mesh_customdatacorrect_init_vert(tcld, (TransDataBasic *)tob, i);
    }

    TransDataMirror *td_mirror = tc->data_mirror;
    for (int j = tc->data_mirror_len; j--; td_mirror++, i++) {
      mesh_customdatacorrect_init_vert(tcld, (TransDataBasic *)td_mirror, i);
    }
  }

  tc->custom.type.data = tcld;
  tc->custom.type.free_cb = mesh_customdatacorrect_free_cb;
}

void mesh_customdatacorrect_init(TransInfo *t)
{
  bool use_merge_group = false;
  if (ELEM(t->mode, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {
    if (!(t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT_SLIDE)) {
      /* No custom-data correction. */
      return;
    }
    use_merge_group = true;
  }
  else if (ELEM(t->mode,
                TFM_TRANSLATION,
                TFM_ROTATION,
                TFM_RESIZE,
                TFM_TOSPHERE,
                TFM_SHEAR,
                TFM_BEND,
                TFM_SHRINKFATTEN,
                TFM_TRACKBALL,
                TFM_PUSHPULL,
                TFM_ALIGN)) {
    {
      if (!(t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT)) {
        /* No custom-data correction. */
        return;
      }
      use_merge_group = (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT_KEEP_CONNECTED) != 0;
    }
  }
  else {
    return;
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    mesh_customdatacorrect_init_container(tc, use_merge_group);
  }
}

/**
 * If we're sliding the vert, return its original location, if not, the current location is good.
 */
static const float *trans_vert_orig_co_get(struct TransCustomDataLayer *tcld, BMVert *v)
{
  TransDataBasic *td = BLI_ghash_lookup(tcld->merge_group.origverts, v);
  return td ? td->iloc : v->co;
}

static void mesh_customdatacorrect_apply_vert(struct TransCustomDataLayer *tcld,
                                              struct TransDataBasic *td,
                                              struct TransCustomDataMergeGroup *merge_data,
                                              bool do_loop_mdisps)
{
  BMesh *bm = tcld->bm;
  BMVert *v = td->extra;
  const float *co_orig_3d = td->iloc;

  BMIter liter;
  int j, l_num;
  float *loop_weights;
  const bool is_moved = (len_squared_v3v3(v->co, co_orig_3d) > FLT_EPSILON);
  const bool do_loop_weight = is_moved && tcld->merge_group.customdatalayer_map_len;
  const float *v_proj_axis = v->no;
  /* original (l->prev, l, l->next) projections for each loop ('l' remains unchanged) */
  float v_proj[3][3];

  if (do_loop_weight) {
    project_plane_normalized_v3_v3v3(v_proj[1], co_orig_3d, v_proj_axis);
  }

  // BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT)
  BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, v);
  l_num = liter.count;
  loop_weights = do_loop_weight ? BLI_array_alloca(loop_weights, l_num) : NULL;
  for (j = 0; j < l_num; j++) {
    BMFace *f_copy; /* the copy of 'f' */
    BMLoop *l = BM_iter_step(&liter);

    f_copy = BLI_ghash_lookup(tcld->origfaces, l->f);

#ifdef USE_FACE_SUBSTITUTE
    /* In some faces it is not possible to calculate interpolation,
     * so we use a substitute. */
    if (BM_elem_index_get(f_copy) == FACE_SUBSTITUTE_INDEX) {
      f_copy = mesh_customdatacorrect_face_substitute_get(f_copy);
    }
#endif

    /* only loop data, no vertex data since that contains shape keys,
     * and we do not want to mess up other shape keys */
    BM_loop_interp_from_face(bm, l, f_copy, false, false);

    /* weight the loop */
    if (do_loop_weight) {
      const float eps = 1.0e-8f;
      const BMLoop *l_prev = l->prev;
      const BMLoop *l_next = l->next;
      const float *co_prev = trans_vert_orig_co_get(tcld, l_prev->v);
      const float *co_next = trans_vert_orig_co_get(tcld, l_next->v);
      bool co_prev_ok;
      bool co_next_ok;

      /* In the unlikely case that we're next to a zero length edge -
       * walk around the to the next.
       *
       * Since we only need to check if the vertex is in this corner,
       * its not important _which_ loop - as long as its not overlapping
       * 'sv->co_orig_3d', see: T45096. */
      project_plane_normalized_v3_v3v3(v_proj[0], co_prev, v_proj_axis);
      while (UNLIKELY(((co_prev_ok = (len_squared_v3v3(v_proj[1], v_proj[0]) > eps)) == false) &&
                      ((l_prev = l_prev->prev) != l->next))) {
        co_prev = trans_vert_orig_co_get(tcld, l_prev->v);
        project_plane_normalized_v3_v3v3(v_proj[0], co_prev, v_proj_axis);
      }
      project_plane_normalized_v3_v3v3(v_proj[2], co_next, v_proj_axis);
      while (UNLIKELY(((co_next_ok = (len_squared_v3v3(v_proj[1], v_proj[2]) > eps)) == false) &&
                      ((l_next = l_next->next) != l->prev))) {
        co_next = trans_vert_orig_co_get(tcld, l_next->v);
        project_plane_normalized_v3_v3v3(v_proj[2], co_next, v_proj_axis);
      }

      if (co_prev_ok && co_next_ok) {
        const float dist = dist_signed_squared_to_corner_v3v3v3(
            v->co, UNPACK3(v_proj), v_proj_axis);

        loop_weights[j] = (dist >= 0.0f) ? 1.0f : ((dist <= -eps) ? 0.0f : (1.0f + (dist / eps)));
        if (UNLIKELY(!isfinite(loop_weights[j]))) {
          loop_weights[j] = 0.0f;
        }
      }
      else {
        loop_weights[j] = 0.0f;
      }
    }
  }

  if (tcld->use_merge_group) {
    struct LinkNode **cd_loop_groups = merge_data->cd_loop_groups;
    if (tcld->merge_group.customdatalayer_map_len && cd_loop_groups) {
      if (do_loop_weight) {
        for (j = 0; j < tcld->merge_group.customdatalayer_map_len; j++) {
          BM_vert_loop_groups_data_layer_merge_weights(
              bm, cd_loop_groups[j], tcld->merge_group.customdatalayer_map[j], loop_weights);
        }
      }
      else {
        for (j = 0; j < tcld->merge_group.customdatalayer_map_len; j++) {
          BM_vert_loop_groups_data_layer_merge(
              bm, cd_loop_groups[j], tcld->merge_group.customdatalayer_map[j]);
        }
      }
    }
  }

  /* Special handling for multires
   *
   * Interpolate from every other loop (not ideal)
   * However values will only be taken from loops which overlap other mdisps.
   * */
  const bool update_loop_mdisps = is_moved && do_loop_mdisps && (tcld->cd_loop_mdisp_offset != -1);
  if (update_loop_mdisps) {
    float(*faces_center)[3] = BLI_array_alloca(faces_center, l_num);
    BMLoop *l;

    BM_ITER_ELEM_INDEX (l, &liter, v, BM_LOOPS_OF_VERT, j) {
      BM_face_calc_center_median(l->f, faces_center[j]);
    }

    BM_ITER_ELEM_INDEX (l, &liter, v, BM_LOOPS_OF_VERT, j) {
      BMFace *f_copy = BLI_ghash_lookup(tcld->origfaces, l->f);
      float f_copy_center[3];
      BMIter liter_other;
      BMLoop *l_other;
      int j_other;

      BM_face_calc_center_median(f_copy, f_copy_center);

      BM_ITER_ELEM_INDEX (l_other, &liter_other, v, BM_LOOPS_OF_VERT, j_other) {
        BM_face_interp_multires_ex(bm,
                                   l_other->f,
                                   f_copy,
                                   faces_center[j_other],
                                   f_copy_center,
                                   tcld->cd_loop_mdisp_offset);
      }
    }
  }
}

static void mesh_customdatacorrect_apply(TransInfo *t, bool is_final)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (!tc->custom.type.data) {
      continue;
    }
    struct TransCustomDataLayer *tcld = tc->custom.type.data;
    const bool use_merge_group = tcld->use_merge_group;

    struct TransCustomDataMergeGroup *merge_data = tcld->merge_group.data;
    TransData *tob = tc->data;
    for (int i = tc->data_len; i--; tob++) {
      mesh_customdatacorrect_apply_vert(tcld, (TransDataBasic *)tob, merge_data, is_final);

      if (use_merge_group) {
        merge_data++;
      }
    }

    TransDataMirror *td_mirror = tc->data_mirror;
    for (int i = tc->data_mirror_len; i--; td_mirror++) {
      mesh_customdatacorrect_apply_vert(tcld, (TransDataBasic *)td_mirror, merge_data, is_final);

      if (use_merge_group) {
        merge_data++;
      }
    }
  }
}

static void mesh_customdatacorrect_restore(struct TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    struct TransCustomDataLayer *tcld = tc->custom.type.data;
    if (!tcld) {
      continue;
    }

    BMesh *bm = tcld->bm;
    BMesh *bm_copy = tcld->bm_origfaces;

    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, tcld->origfaces) {
      BMFace *f = BLI_ghashIterator_getKey(&gh_iter);
      BMFace *f_copy = BLI_ghashIterator_getValue(&gh_iter);
      BLI_assert(f->len == f_copy->len);

      BMLoop *l_iter, *l_first, *l_copy;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      l_copy = BM_FACE_FIRST_LOOP(f_copy);
      do {
        /* TODO: Restore only the elements that transform. */
        BM_elem_attrs_copy(bm_copy, bm, l_copy, l_iter);
        l_copy = l_copy->next;
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Mesh Data
 *
 * \{ */

static void mesh_apply_to_mirror(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->use_mirror_axis_any) {
      int i;
      TransData *td;
      for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
        if (td->flag & (TD_MIRROR_EDGE_X | TD_MIRROR_EDGE_Y | TD_MIRROR_EDGE_Z)) {
          if (td->flag & TD_MIRROR_EDGE_X) {
            td->loc[0] = 0.0f;
          }
          if (td->flag & TD_MIRROR_EDGE_Y) {
            td->loc[1] = 0.0f;
          }
          if (td->flag & TD_MIRROR_EDGE_Z) {
            td->loc[2] = 0.0f;
          }
        }
      }

      TransDataMirror *td_mirror = tc->data_mirror;
      for (i = 0; i < tc->data_mirror_len; i++, td_mirror++) {
        copy_v3_v3(td_mirror->loc, td_mirror->loc_src);
        if (td_mirror->flag & TD_MIRROR_X) {
          td_mirror->loc[0] *= -1;
        }
        if (td_mirror->flag & TD_MIRROR_Y) {
          td_mirror->loc[1] *= -1;
        }
        if (td_mirror->flag & TD_MIRROR_Z) {
          td_mirror->loc[2] *= -1;
        }
      }
    }
  }
}

void recalcData_mesh(TransInfo *t)
{
  bool is_canceling = t->state == TRANS_CANCEL;
  /* mirror modifier clipping? */
  if (!is_canceling) {
    /* apply clipping after so we never project past the clip plane T25423. */
    applyProject(t);
    clipMirrorModifier(t);

    if ((t->flag & T_NO_MIRROR) == 0 && (t->options & CTX_NO_MIRROR) == 0) {
      mesh_apply_to_mirror(t);
    }

    mesh_customdatacorrect_apply(t, false);
  }
  else {
    mesh_customdatacorrect_restore(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    EDBM_mesh_normals_update(em);
    BKE_editmesh_looptri_calc(em);
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Mesh
 * \{ */

void special_aftertrans_update__mesh(bContext *UNUSED(C), TransInfo *t)
{
  const bool is_canceling = (t->state == TRANS_CANCEL);
  const bool use_automerge = !is_canceling && (t->flag & (T_AUTOMERGE | T_AUTOSPLIT)) != 0;

  if (!is_canceling && ELEM(t->mode, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {
    /* NOTE(joeedh): Handle multi-res re-projection,
     * done on transform completion since it's really slow. */
    mesh_customdatacorrect_apply(t, true);
  }

  if (use_automerge) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
      BMesh *bm = em->bm;
      char hflag;
      bool has_face_sel = (bm->totfacesel != 0);

      if (tc->use_mirror_axis_any) {
        /* Rather than adjusting the selection (which the user would notice)
         * tag all mirrored verts, then auto-merge those. */
        BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

        TransDataMirror *td_mirror = tc->data_mirror;
        for (int i = tc->data_mirror_len; i--; td_mirror++) {
          BM_elem_flag_enable((BMVert *)td_mirror->extra, BM_ELEM_TAG);
        }

        hflag = BM_ELEM_SELECT | BM_ELEM_TAG;
      }
      else {
        hflag = BM_ELEM_SELECT;
      }

      if (t->flag & T_AUTOSPLIT) {
        EDBM_automerge_and_split(
            tc->obedit, true, true, true, hflag, t->scene->toolsettings->doublimit);
      }
      else {
        EDBM_automerge(tc->obedit, true, hflag, t->scene->toolsettings->doublimit);
      }

      /* Special case, this is needed or faces won't re-select.
       * Flush selected edges to faces. */
      if (has_face_sel && (em->selectmode == SCE_SELECT_FACE)) {
        EDBM_selectmode_flush_ex(em, SCE_SELECT_EDGE);
      }
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    /* table needs to be created for each edit command, since vertices can move etc */
    ED_mesh_mirror_spatial_table_end(tc->obedit);
    /* TODO(campbell): xform: We need support for many mirror objects at once! */
    break;
  }
}
/** \} */
