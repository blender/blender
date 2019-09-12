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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 *
 * Utility functions for merging geometry once transform has finished:
 *
 * - #EDBM_automerge
 * - #EDBM_automerge_and_split
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_sort.h"

#include "BKE_bvhutils.h"
#include "BKE_editmesh.h"

#include "WM_api.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "DNA_object_types.h"

#include "DEG_depsgraph.h"

/* use bmesh operator flags for a few operators */
#define BMO_ELE_TAG 1

/* -------------------------------------------------------------------- */
/** \name Auto-Merge Selection
 *
 * Used after transform operations.
 * \{ */

void EDBM_automerge(Object *obedit, bool update, const char hflag, const float dist)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  int totvert_prev = bm->totvert;

  BMOperator findop, weldop;

  /* Search for doubles among all vertices, but only merge non-VERT_KEEP
   * vertices into VERT_KEEP vertices. */
  BMO_op_initf(bm,
               &findop,
               BMO_FLAG_DEFAULTS,
               "find_doubles verts=%av keep_verts=%Hv dist=%f",
               hflag,
               dist);

  BMO_op_exec(bm, &findop);

  /* weld the vertices */
  BMO_op_init(bm, &weldop, BMO_FLAG_DEFAULTS, "weld_verts");
  BMO_slot_copy(&findop, slots_out, "targetmap.out", &weldop, slots_in, "targetmap");
  BMO_op_exec(bm, &weldop);

  BMO_op_finish(bm, &findop);
  BMO_op_finish(bm, &weldop);

  if ((totvert_prev != bm->totvert) && update) {
    EDBM_update_generic(em, true, true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Merge & Split Selection
 *
 * Used after transform operations.
 * \{ */

struct EDBMSplitEdge {
  BMVert *v;
  BMEdge *e;
  float lambda;
};

struct EDBMSplitBestFaceData {
  BMEdge **edgenet;
  int edgenet_len;

  /**
   * Track the range of vertices in edgenet along the faces normal,
   * find the lowest since it's most likely to be most co-planar with the face.
   */
  float best_face_range_on_normal_axis;
  BMFace *r_best_face;
};

struct EDBMSplitEdgeData {
  BMesh *bm;

  BMEdge *r_edge;
  float r_lambda;
};

static bool edbm_vert_pair_share_best_splittable_face_cb(BMFace *f,
                                                         BMLoop *l_a,
                                                         BMLoop *l_b,
                                                         void *userdata)
{
  struct EDBMSplitBestFaceData *data = userdata;
  float no[3];
  copy_v3_v3(no, f->no);

  float min = dot_v3v3(l_a->v->co, no);
  float max = dot_v3v3(l_b->v->co, no);
  if (min > max) {
    SWAP(float, min, max);
  }

  BMVert *v_test = l_b->v;
  BMEdge **e_iter = &data->edgenet[0];
  int verts_len = data->edgenet_len - 1;
  for (int i = verts_len; i--; e_iter++) {
    v_test = BM_edge_other_vert(*e_iter, v_test);
    if (!BM_face_point_inside_test(f, v_test->co)) {
      return false;
    }
    float dot = dot_v3v3(v_test->co, no);
    if (dot < min) {
      min = dot;
    }
    if (dot > max) {
      max = dot;
    }
  }

  const float test_face_range_on_normal_axis = max - min;
  if (test_face_range_on_normal_axis < data->best_face_range_on_normal_axis) {
    data->best_face_range_on_normal_axis = test_face_range_on_normal_axis;
    data->r_best_face = f;
  }

  return false;
}

/* find the best splittable face between the two vertices. */
static bool edbm_vert_pair_share_splittable_face_cb(BMFace *UNUSED(f),
                                                    BMLoop *l_a,
                                                    BMLoop *l_b,
                                                    void *userdata)
{
  float(*data)[3] = userdata;
  float *v_a_co = data[0];
  float *v_a_b_dir = data[1];

  float lambda;
  if (isect_ray_seg_v3(v_a_co, v_a_b_dir, l_a->prev->v->co, l_a->next->v->co, &lambda)) {
    if (IN_RANGE(lambda, 0.0f, 1.0f)) {
      return true;
    }
    else if (isect_ray_seg_v3(v_a_co, v_a_b_dir, l_b->prev->v->co, l_b->next->v->co, &lambda)) {
      return IN_RANGE(lambda, 0.0f, 1.0f);
    }
  }
  return false;
}

static void edbm_automerge_weld_linked_wire_edges_into_linked_faces(
    BMesh *bm, BMVert *v, const float epsilon, BMEdge **r_edgenet[], int *r_edgenet_alloc_len)
{
  BMEdge **edgenet = *r_edgenet;
  int edgenet_alloc_len = *r_edgenet_alloc_len;

  BMIter iter;
  BMEdge *e;
  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    int edgenet_len = 0;
    BMVert *v_other = v;
    while (BM_edge_is_wire(e)) {
      if (edgenet_alloc_len == edgenet_len) {
        edgenet_alloc_len = (edgenet_alloc_len + 1) * 2;
        edgenet = MEM_reallocN(edgenet, (edgenet_alloc_len) * sizeof(*edgenet));
      }
      edgenet[edgenet_len++] = e;
      v_other = BM_edge_other_vert(e, v_other);
      if (v_other == v) {
        /* Endless loop. */
        break;
      }

      BMEdge *e_next = BM_DISK_EDGE_NEXT(e, v_other);
      if (e_next == e) {
        /* Vert is wire_endpoint. */
        edgenet_len = 0;
        break;
      }

      BMEdge *e_test = e_next;
      while ((e_test = BM_DISK_EDGE_NEXT(e_test, v_other)) != e) {
        if (e_test->l) {
          /* Vert is linked to a face. */
          goto l_break;
        }
      }

      e = e_next;
    }

    BMLoop *dummy;
    BMFace *best_face;

  l_break:
    if (edgenet_len == 0) {
      /* Nothing to do. */
      continue;
    }
    if (edgenet_len == 1) {
      float data[2][3];
      copy_v3_v3(data[0], v_other->co);
      sub_v3_v3v3(data[1], v->co, data[0]);
      best_face = BM_vert_pair_shared_face_cb(
          v_other, v, true, edbm_vert_pair_share_splittable_face_cb, &data, &dummy, &dummy);
    }
    else {
      struct EDBMSplitBestFaceData data = {
          .edgenet = edgenet,
          .edgenet_len = edgenet_len,
          .best_face_range_on_normal_axis = FLT_MAX,
          .r_best_face = NULL,
      };
      BM_vert_pair_shared_face_cb(
          v_other, v, true, edbm_vert_pair_share_best_splittable_face_cb, &data, &dummy, &dummy);

      if (data.r_best_face) {
        float no[3], min = FLT_MAX, max = -FLT_MAX;
        copy_v3_v3(no, data.r_best_face->no);
        BMVert *v_test;
        BMIter f_iter;
        BM_ITER_ELEM (v_test, &f_iter, data.r_best_face, BM_VERTS_OF_FACE) {
          float dot = dot_v3v3(v_test->co, no);
          if (dot < min) {
            min = dot;
          }
          if (dot > max) {
            max = dot;
          }
        }
        float range = max - min + 2 * epsilon;
        if (range < data.best_face_range_on_normal_axis) {
          data.r_best_face = NULL;
        }
      }
      best_face = data.r_best_face;
    }

    if (best_face) {
      BM_face_split_edgenet(bm, best_face, edgenet, edgenet_len, NULL, NULL);
    }
  }

  *r_edgenet = edgenet;
  *r_edgenet_alloc_len = edgenet_alloc_len;
}

static void ebbm_automerge_and_split_find_duplicate_cb(void *userdata,
                                                       int index,
                                                       const float co[3],
                                                       BVHTreeNearest *nearest)
{
  struct EDBMSplitEdgeData *data = userdata;
  BMEdge *e = BM_edge_at_index(data->bm, index);
  float lambda = line_point_factor_v3_ex(co, e->v1->co, e->v2->co, 0.0f, -1.0f);
  if (IN_RANGE(lambda, 0.0f, 1.0f)) {
    float near_co[3];
    interp_v3_v3v3(near_co, e->v1->co, e->v2->co, lambda);
    float dist_sq = len_squared_v3v3(near_co, co);
    if (dist_sq < nearest->dist_sq) {
      nearest->dist_sq = dist_sq;
      nearest->index = index;

      data->r_edge = e;
      data->r_lambda = lambda;
    }
  }
}

static int edbm_automerge_and_split_sort_cmp_by_keys_cb(const void *index1_v,
                                                        const void *index2_v,
                                                        void *keys_v)
{
  const struct EDBMSplitEdge *cuts = keys_v;
  const int *index1 = (int *)index1_v;
  const int *index2 = (int *)index2_v;

  if (cuts[*index1].lambda > cuts[*index2].lambda) {
    return 1;
  }
  else {
    return -1;
  }
}

void EDBM_automerge_and_split(Object *obedit,
                              bool split_edges,
                              bool split_faces,
                              bool update,
                              const char hflag,
                              const float dist)
{
  bool ok = false;

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  BMOperator findop, weldop;
  BMOpSlot *slot_targetmap;
  BMIter iter;
  BMVert *v;

  /* tag and count the verts to be tested. */
  BM_mesh_elem_toolflags_ensure(bm);
  int verts_len = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, hflag)) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
      BMO_vert_flag_enable(bm, v, BMO_ELE_TAG);
      verts_len++;
    }
    else {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
    }
  }

  /* Search for doubles among all vertices, but only merge non-BMO_ELE_TAG
   * vertices into BMO_ELE_TAG vertices. */
  BMO_op_initf(bm, &findop, 0, "find_doubles verts=%av keep_verts=%Fv dist=%f", BMO_ELE_TAG, dist);
  BMO_op_exec(bm, &findop);

  /* Init weld_verts operator to later fill the targetmap. */
  BMO_op_init(bm, &weldop, 0, "weld_verts");
  BMO_slot_copy(&findop, slots_out, "targetmap.out", &weldop, slots_in, "targetmap");

  slot_targetmap = BMO_slot_get(weldop.slots_in, "targetmap");

  /* Remove duplicate vertices from the split edge test and check and split faces. */
  GHashIterator gh_iter;
  GHash *ghash_targetmap = BMO_SLOT_AS_GHASH(slot_targetmap);
  GHASH_ITER (gh_iter, ghash_targetmap) {
    v = BLI_ghashIterator_getKey(&gh_iter);
    BMVert *v_dst = BLI_ghashIterator_getValue(&gh_iter);
    if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
      /* Should this happen? */
      SWAP(BMVert *, v, v_dst);
    }
    BLI_assert(BM_elem_flag_test(v, BM_ELEM_TAG));
    BM_elem_flag_disable(v, BM_ELEM_TAG);

    ok = true;
    verts_len--;
  }

  int totedge = bm->totedge;
  if (totedge == 0 || verts_len == 0) {
    split_edges = false;
  }

  if (split_edges) {
    /* Count and tag edges. */
    BMEdge *e;
    int edges_len = 0;
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN) && !BM_elem_flag_test(e->v1, BM_ELEM_TAG) &&
          !BM_elem_flag_test(e->v2, BM_ELEM_TAG)) {
        BM_elem_flag_enable(e, BM_ELEM_TAG);
        edges_len++;
      }
      else {
        BM_elem_flag_disable(e, BM_ELEM_TAG);
      }
    }

    if (edges_len) {
      /* Use `e->head.index` to count intersections. */
      bm->elem_index_dirty &= ~BM_EDGE;

      /* Create a BVHTree of edges with `dist` as epsilon. */
      BVHTree *tree_edges = BLI_bvhtree_new(edges_len, dist, 2, 6);
      int i;
      BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
        if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
          float co[2][3];
          copy_v3_v3(co[0], e->v1->co);
          copy_v3_v3(co[1], e->v2->co);

          BLI_bvhtree_insert(tree_edges, i, co[0], 2);

          e->head.index = 0;
        }
      }
      BLI_bvhtree_balance(tree_edges);

      struct EDBMSplitEdge *cuts_iter, *cuts;

      /* Store all intersections in this array. */
      cuts = MEM_mallocN(verts_len * sizeof(*cuts), __func__);
      cuts_iter = &cuts[0];

      int cuts_len = 0;
      int cut_edges_len = 0;
      float dist_sq = SQUARE(dist);
      struct EDBMSplitEdgeData data = {bm};

      /* Start the search for intersections. */
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
          float co[3];
          copy_v3_v3(co, v->co);
          int e_index = BLI_bvhtree_find_nearest_first(
              tree_edges, co, dist_sq, ebbm_automerge_and_split_find_duplicate_cb, &data);

          if (e_index != -1) {
            e = data.r_edge;
            e->head.index++;

            cuts_iter->v = v;
            cuts_iter->e = e;
            cuts_iter->lambda = data.r_lambda;
            cuts_iter++;
            cuts_len++;

            if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
              BM_elem_flag_disable(e, BM_ELEM_TAG);
              cut_edges_len++;
            }
          }
        }
      }
      BLI_bvhtree_free(tree_edges);

      if (cuts_len) {
        /* Map intersections per edge. */
        union {
          struct {
            int cuts_len;
            int cuts_index[];
          };
          int as_int[0];
        } * e_map_iter, *e_map;

        e_map = MEM_mallocN((cut_edges_len * sizeof(*e_map)) +
                                (cuts_len * sizeof(*(e_map->cuts_index))),
                            __func__);

        int map_len = 0;
        cuts_iter = &cuts[0];
        for (i = 0; i < cuts_len; i++, cuts_iter++) {
          e = cuts_iter->e;
          if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
            BM_elem_flag_enable(e, BM_ELEM_TAG);
            int e_cuts_len = e->head.index;

            e_map_iter = (void *)&e_map->as_int[map_len];
            e_map_iter->cuts_len = e_cuts_len;
            e_map_iter->cuts_index[0] = i;

            /* Use `e->head.index` to indicate which slot to fill with the `cuts` index. */
            e->head.index = map_len + 1;
            map_len += 1 + e_cuts_len;
          }
          else {
            e_map->as_int[++e->head.index] = i;
          }
        }

        /* Split Edges and Faces. */
        for (i = 0; i < map_len;
             e_map_iter = (void *)&e_map->as_int[i], i += 1 + e_map_iter->cuts_len) {

          /* sort by lambda. */
          BLI_qsort_r(e_map_iter->cuts_index,
                      e_map_iter->cuts_len,
                      sizeof(*(e_map->cuts_index)),
                      edbm_automerge_and_split_sort_cmp_by_keys_cb,
                      cuts);

          float lambda, lambda_prev = 0.0f;
          for (int j = 0; j < e_map_iter->cuts_len; j++) {
            cuts_iter = &cuts[e_map_iter->cuts_index[j]];
            lambda = (cuts_iter->lambda - lambda_prev) / (1.0f - lambda_prev);
            lambda_prev = cuts_iter->lambda;
            v = cuts_iter->v;
            e = cuts_iter->e;

            BMVert *v_new = BM_edge_split(bm, e, e->v1, NULL, lambda);

            BMO_slot_map_elem_insert(&weldop, slot_targetmap, v_new, v);
          }
        }

        ok = true;
        MEM_freeN(e_map);
      }

      MEM_freeN(cuts);
    }
  }

  BMO_op_exec(bm, &weldop);

  BMEdge **edgenet = NULL;
  int edgenet_alloc_len = 0;
  if (split_faces) {
    GHASH_ITER (gh_iter, ghash_targetmap) {
      v = BLI_ghashIterator_getValue(&gh_iter);
      BLI_assert(BM_elem_flag_test(v, hflag) || hflag == BM_ELEM_TAG);
      edbm_automerge_weld_linked_wire_edges_into_linked_faces(
          bm, v, dist, &edgenet, &edgenet_alloc_len);
    }
  }

  if (edgenet) {
    MEM_freeN(edgenet);
  }

  BMO_op_finish(bm, &findop);
  BMO_op_finish(bm, &weldop);

  if (LIKELY(ok) && update) {
    EDBM_update_generic(em, true, true);
  }
}

/** \} */
