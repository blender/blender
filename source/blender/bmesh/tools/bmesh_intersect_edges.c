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
 * \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_sort.h"
#include "BLI_stack.h"

#include "BKE_bvhutils.h"

#include "bmesh.h"

#include "bmesh_intersect_edges.h" /* own include */

#define KDOP_TREE_TYPE 4
#define KDOP_AXIS_LEN 14

/* -------------------------------------------------------------------- */
/** \name Weld Linked Wire Edges into Linked Faces
 *
 * Used with the merge vertices option.
 * \{ */

/* Callbacks for `BM_vert_pair_shared_face_cb` */

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

static bool bm_vert_pair_share_best_splittable_face_cb(BMFace *f,
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
static bool bm_vert_pair_share_splittable_face_cb(BMFace *UNUSED(f),
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

void BM_vert_weld_linked_wire_edges_into_linked_faces(
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
          v_other, v, true, bm_vert_pair_share_splittable_face_cb, &data, &dummy, &dummy);
    }
    else {
      struct EDBMSplitBestFaceData data = {
          .edgenet = edgenet,
          .edgenet_len = edgenet_len,
          .best_face_range_on_normal_axis = FLT_MAX,
          .r_best_face = NULL,
      };
      BM_vert_pair_shared_face_cb(
          v_other, v, true, bm_vert_pair_share_best_splittable_face_cb, &data, &dummy, &dummy);

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Merge & Split Selection
 *
 * Used after transform operations.
 * \{ */

struct EDBMSplitElem {
  union {
    BMElem *elem;
    BMVert *vert;
    struct {
      BMEdge *edge;
      float lambda;
    };
  };
};

/* -------------------------------------------------------------------- */
/* Overlap Callbacks */

struct EDBMSplitData {
  BMesh *bm;
  BLI_Stack **pair_stack;
  int cut_edges_len;
  float dist_sq;
  float dist_sq_sq;
};

/* Utils */

static void bm_vert_pair_elem_setup_ex(BMVert *v, struct EDBMSplitElem *r_pair_elem)
{
  r_pair_elem->vert = v;
}

static void bm_edge_pair_elem_setup(BMEdge *e,
                                    float lambda,
                                    int *r_data_cut_edges_len,
                                    struct EDBMSplitElem *r_pair_elem)
{
  r_pair_elem->edge = e;
  r_pair_elem->lambda = lambda;

  e->head.index++;
  /* Obs: Check Multithread. */
  if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
    BM_elem_flag_disable(e, BM_ELEM_TAG);
    (*r_data_cut_edges_len)++;
  }
}

/* Util for Vert x Edge and Edge x Edge callbacks */
static bool bm_edgexvert_isect_impl(BMVert *v,
                                    BMEdge *e,
                                    const float co[3],
                                    const float dir[3],
                                    float lambda,
                                    float data_dist_sq,
                                    int *data_cut_edges_len,
                                    struct EDBMSplitElem r_pair[2])
{
  BMVert *e_v;
  float dist_sq_vert_factor;

  if (!IN_RANGE_INCL(lambda, 0.0f, 1.0f)) {
    /* Vert x Vert is already handled elsewhere. */
    return false;
  }

  if (lambda < 0.5f) {
    e_v = e->v1;
    dist_sq_vert_factor = lambda;
  }
  else {
    e_v = e->v2;
    dist_sq_vert_factor = 1.0f - lambda;
  }

  if (v != e_v) {
    float dist_sq_vert = SQUARE(dist_sq_vert_factor) * len_squared_v3(dir);
    if (dist_sq_vert < data_dist_sq) {
      /* Vert x Vert is already handled elsewhere. */
      return false;
    }

    float near[3];
    madd_v3_v3v3fl(near, co, dir, lambda);

    float dist_sq = len_squared_v3v3(v->co, near);
    if (dist_sq < data_dist_sq) {
      bm_edge_pair_elem_setup(e, lambda, data_cut_edges_len, &r_pair[0]);
      bm_vert_pair_elem_setup_ex(v, &r_pair[1]);
      return true;
    }
  }

  return false;
}

/* Vertex x Vertex Callback */

static bool bm_vertxvert_isect_cb(void *userdata, int index_a, int index_b, int thread)
{
  struct EDBMSplitData *data = userdata;
  BMVert *v_a = BM_vert_at_index(data->bm, index_a);
  BMVert *v_b = BM_vert_at_index(data->bm, index_b);

  struct EDBMSplitElem *pair = BLI_stack_push_r(data->pair_stack[thread]);

  bm_vert_pair_elem_setup_ex(v_a, &pair[0]);
  bm_vert_pair_elem_setup_ex(v_b, &pair[1]);

  return true;
}

static bool bm_vertxvert_self_isect_cb(void *userdata, int index_a, int index_b, int thread)
{
  if (index_a < index_b) {
    return bm_vertxvert_isect_cb(userdata, index_a, index_b, thread);
  }
  return false;
}

/* Vertex x Edge and Edge x Vertex Callbacks */

static bool bm_edgexvert_isect_cb(void *userdata, int index_a, int index_b, int thread)
{
  struct EDBMSplitData *data = userdata;
  BMEdge *e = BM_edge_at_index(data->bm, index_a);
  BMVert *v = BM_vert_at_index(data->bm, index_b);

  float co[3], dir[3], lambda;
  copy_v3_v3(co, e->v1->co);
  sub_v3_v3v3(dir, e->v2->co, co);
  lambda = ray_point_factor_v3_ex(v->co, co, dir, 0.0f, -1.0f);

  struct EDBMSplitElem pair_tmp[2];
  if (bm_edgexvert_isect_impl(
          v, e, co, dir, lambda, data->dist_sq, &data->cut_edges_len, pair_tmp)) {
    struct EDBMSplitElem *pair = BLI_stack_push_r(data->pair_stack[thread]);
    pair[0] = pair_tmp[0];
    pair[1] = pair_tmp[1];
  }

  /* Always return false with edges. */
  return false;
}

/* Edge x Edge Callbacks */

static bool bm_edgexedge_isect_impl(struct EDBMSplitData *data,
                                    BMEdge *e_a,
                                    BMEdge *e_b,
                                    const float co_a[3],
                                    const float dir_a[3],
                                    const float co_b[3],
                                    const float dir_b[3],
                                    float lambda_a,
                                    float lambda_b,
                                    struct EDBMSplitElem r_pair[2])
{
  float dist_sq_va_factor, dist_sq_vb_factor;
  BMVert *e_a_v, *e_b_v;
  if (lambda_a < 0.5f) {
    e_a_v = e_a->v1;
    dist_sq_va_factor = lambda_a;
  }
  else {
    e_a_v = e_a->v2;
    dist_sq_va_factor = 1.0f - lambda_a;
  }

  if (lambda_b < 0.5f) {
    e_b_v = e_b->v1;
    dist_sq_vb_factor = lambda_b;
  }
  else {
    e_b_v = e_b->v2;
    dist_sq_vb_factor = 1.0f - lambda_b;
  }

  if (e_a_v != e_b_v) {
    if (!IN_RANGE_INCL(lambda_a, 0.0f, 1.0f) || !IN_RANGE_INCL(lambda_b, 0.0f, 1.0f)) {
      /* Vert x Edge is already handled elsewhere. */
      return false;
    }

    float dist_sq_va = SQUARE(dist_sq_va_factor) * len_squared_v3(dir_a);
    float dist_sq_vb = SQUARE(dist_sq_vb_factor) * len_squared_v3(dir_b);

    if (dist_sq_va < data->dist_sq || dist_sq_vb < data->dist_sq) {
      /* Vert x Edge is already handled elsewhere. */
      return false;
    }

    float near_a[3], near_b[3];
    madd_v3_v3v3fl(near_a, co_a, dir_a, lambda_a);
    madd_v3_v3v3fl(near_b, co_b, dir_b, lambda_b);

    float dist_sq = len_squared_v3v3(near_a, near_b);
    if (dist_sq < data->dist_sq) {
      bm_edge_pair_elem_setup(e_a, lambda_a, &data->cut_edges_len, &r_pair[0]);
      bm_edge_pair_elem_setup(e_b, lambda_b, &data->cut_edges_len, &r_pair[1]);
      return true;
    }
  }
  return false;
}

static bool bm_edgexedge_isect_cb(void *userdata, int index_a, int index_b, int thread)
{
  struct EDBMSplitData *data = userdata;
  BMEdge *e_a = BM_edge_at_index(data->bm, index_a);
  BMEdge *e_b = BM_edge_at_index(data->bm, index_b);

  if (BM_edge_share_vert_check(e_a, e_b)) {
    /* The other vertices may intersect but Vert x Edge is already handled elsewhere. */
    return false;
  }

  float co_a[3], dir_a[3], co_b[3], dir_b[3];
  copy_v3_v3(co_a, e_a->v1->co);
  sub_v3_v3v3(dir_a, e_a->v2->co, co_a);

  copy_v3_v3(co_b, e_b->v1->co);
  sub_v3_v3v3(dir_b, e_b->v2->co, co_b);

  float lambda_a, lambda_b;
  /* Using with dist^4 as `epsilon` is not the best solution, but it fits in most cases. */
  if (isect_ray_ray_epsilon_v3(co_a, dir_a, co_b, dir_b, data->dist_sq_sq, &lambda_a, &lambda_b)) {
    struct EDBMSplitElem pair_tmp[2];
    if (bm_edgexedge_isect_impl(
            data, e_a, e_b, co_a, dir_a, co_b, dir_b, lambda_a, lambda_b, pair_tmp)) {
      struct EDBMSplitElem *pair = BLI_stack_push_r(data->pair_stack[thread]);
      pair[0] = pair_tmp[0];
      pair[1] = pair_tmp[1];
    }
  }

  /* Edge x Edge returns always false. */
  return false;
}

static bool bm_edgexedge_self_isect_cb(void *userdata, int index_a, int index_b, int thread)
{
  if (index_a < index_b) {
    return bm_edgexedge_isect_cb(userdata, index_a, index_b, thread);
  }
  return false;
}

/* -------------------------------------------------------------------- */
/* BVHTree Overlap Function */

static void bm_elemxelem_bvhtree_overlap(const BVHTree *tree1,
                                         const BVHTree *tree2,
                                         BVHTree_OverlapCallback callback,
                                         struct EDBMSplitData *data,
                                         BLI_Stack **pair_stack,
                                         bool use_thread)
{
  int flag = 0;
  int thread_num = 1;
  if (use_thread) {
    flag |= BVH_OVERLAP_USE_THREADING;
    thread_num = BLI_bvhtree_overlap_thread_num(tree1);
  }
  for (int i = 0; i < thread_num; i++) {
    if (pair_stack[i] == NULL) {
      pair_stack[i] = BLI_stack_new(sizeof(struct EDBMSplitElem[2]), __func__);
    }
  }
  data->pair_stack = pair_stack;
  BLI_bvhtree_overlap_ex(tree1, tree2, NULL, callback, data, 1, flag);
}

/* -------------------------------------------------------------------- */
/* Callbacks for `BLI_qsort_r` */

static int sort_cmp_by_lambda_cb(const void *index1_v, const void *index2_v, void *keys_v)
{
  const struct EDBMSplitElem *pair_flat = keys_v;
  const int index1 = *(int *)index1_v;
  const int index2 = *(int *)index2_v;

  if (pair_flat[index1].lambda > pair_flat[index2].lambda) {
    return 1;
  }
  else {
    return -1;
  }
}

/* -------------------------------------------------------------------- */
/* Main API */

bool BM_mesh_intersect_edges(BMesh *bm, const char hflag, const float dist, GHash *r_targetmap)
{
  bool ok = false;

  BMIter iter;
  BMVert *v;
  BMEdge *e;
  int i;

  /* Store all intersections in this array. */
  struct EDBMSplitElem(*pair_iter)[2], (*pair_array)[2] = NULL;
  int pair_len = 0;

  BLI_Stack *pair_stack[KDOP_TREE_TYPE] = {{NULL}};

  const float dist_sq = SQUARE(dist);
  const float dist_half = dist / 2;

  struct EDBMSplitData data = {
      .bm = bm,
      .pair_stack = pair_stack,
      .cut_edges_len = 0,
      .dist_sq = dist_sq,
      .dist_sq_sq = SQUARE(dist_sq),
  };

  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE);

  /* tag and count the verts to be tested. */
  int verts_act_len = 0, verts_remain_len = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, hflag)) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
      v->head.index = -1;
      verts_act_len++;
    }
    else {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
      if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        v->head.index = -1;
        verts_remain_len++;
      }
    }
  }
  bm->elem_index_dirty |= BM_VERT;

  /* Start the creation of BVHTrees. */
  BVHTree *tree_verts_act = NULL, *tree_verts_remain = NULL;
  if (verts_act_len) {
    tree_verts_act = BLI_bvhtree_new(verts_act_len, dist_half, KDOP_TREE_TYPE, KDOP_AXIS_LEN);
  }
  if (verts_remain_len) {
    tree_verts_remain = BLI_bvhtree_new(
        verts_remain_len, dist_half, KDOP_TREE_TYPE, KDOP_AXIS_LEN);
  }

  if (tree_verts_act || tree_verts_remain) {
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        if (tree_verts_act) {
          BLI_bvhtree_insert(tree_verts_act, i, v->co, 1);
        }
      }
      else if (tree_verts_remain && !BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        BLI_bvhtree_insert(tree_verts_remain, i, v->co, 1);
      }
    }

    if (tree_verts_act) {
      BLI_bvhtree_balance(tree_verts_act);
      /* First pair search. */
      bm_elemxelem_bvhtree_overlap(
          tree_verts_act, tree_verts_act, bm_vertxvert_self_isect_cb, &data, pair_stack, true);
    }

    if (tree_verts_remain) {
      BLI_bvhtree_balance(tree_verts_remain);
    }

    if (tree_verts_act && tree_verts_remain) {
      bm_elemxelem_bvhtree_overlap(
          tree_verts_remain, tree_verts_act, bm_vertxvert_isect_cb, &data, pair_stack, true);
    }
  }

  for (i = KDOP_TREE_TYPE; i--;) {
    if (pair_stack[i]) {
      pair_len += BLI_stack_count(pair_stack[i]);
    }
  }

  const bool intersect_edges = true;
  if (intersect_edges) {
    uint vert_x_vert_pair_len = pair_len;

#define EDGE_ACT_TO_TEST 1
#define EDGE_REMAIN_TO_TEST 2
    /* Tag and count the edges. */
    int edges_act_len = 0, edges_remain_len = 0;
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) ||
          (len_squared_v3v3(e->v1->co, e->v2->co) < dist_sq)) {
        /* Don't test hidden edges or smaller than the minimum distance.
         * These have already been handled in the vertices overlap. */
        BM_elem_index_set(e, 0);
        continue;
      }

      if (BM_elem_flag_test(e->v1, BM_ELEM_TAG) || BM_elem_flag_test(e->v2, BM_ELEM_TAG)) {
        BM_elem_index_set(e, EDGE_ACT_TO_TEST);
        edges_act_len++;
      }
      else {
        BM_elem_index_set(e, EDGE_REMAIN_TO_TEST);
        edges_remain_len++;
      }

      /* Tag used in the overlap callbacks. */
      BM_elem_flag_enable(e, BM_ELEM_TAG);
    }

    BVHTree *tree_edges_act = NULL, *tree_edges_remain = NULL;
    if (edges_act_len) {
      tree_edges_act = BLI_bvhtree_new(edges_act_len, dist_half, KDOP_TREE_TYPE, KDOP_AXIS_LEN);
    }

    if (edges_remain_len && (tree_edges_act || tree_verts_act)) {
      tree_edges_remain = BLI_bvhtree_new(
          edges_remain_len, dist_half, KDOP_TREE_TYPE, KDOP_AXIS_LEN);
    }

    if (tree_edges_act || tree_edges_remain) {
      BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
        int edge_test = BM_elem_index_get(e);
        float co[2][3];
        if (edge_test == EDGE_ACT_TO_TEST) {
          BLI_assert(tree_edges_act);
          e->head.index = 0;
          copy_v3_v3(co[0], e->v1->co);
          copy_v3_v3(co[1], e->v2->co);
          BLI_bvhtree_insert(tree_edges_act, i, co[0], 2);
        }
        else if (edge_test == EDGE_REMAIN_TO_TEST) {
          BLI_assert(tree_edges_act);
          e->head.index = 0;
          copy_v3_v3(co[0], e->v1->co);
          copy_v3_v3(co[1], e->v2->co);
          BLI_bvhtree_insert(tree_edges_remain, i, co[0], 2);
        }
      }
      /* Use `e->head.index` to count intersections. */
      bm->elem_index_dirty |= BM_EDGE;

      if (tree_edges_act) {
        BLI_bvhtree_balance(tree_edges_act);
      }

      if (tree_edges_remain) {
        BLI_bvhtree_balance(tree_edges_remain);
      }

      if (tree_edges_act) {
        /* Edge x Edge */
        bm_elemxelem_bvhtree_overlap(tree_edges_act,
                                     tree_edges_act,
                                     bm_edgexedge_self_isect_cb,
                                     &data,
                                     &pair_stack[KDOP_TREE_TYPE - 1],
                                     false);

        if (tree_edges_remain) {
          bm_elemxelem_bvhtree_overlap(tree_edges_remain,
                                       tree_edges_act,
                                       bm_edgexedge_isect_cb,
                                       &data,
                                       &pair_stack[KDOP_TREE_TYPE - 1],
                                       false);
        }

        if (tree_verts_act) {
          /* Vert x Edge */
          bm_elemxelem_bvhtree_overlap(tree_edges_act,
                                       tree_verts_act,
                                       bm_edgexvert_isect_cb,
                                       &data,
                                       &pair_stack[KDOP_TREE_TYPE - 1],
                                       false);
        }

        if (tree_verts_remain) {
          /* Vert x Edge */
          bm_elemxelem_bvhtree_overlap(tree_edges_act,
                                       tree_verts_remain,
                                       bm_edgexvert_isect_cb,
                                       &data,
                                       &pair_stack[KDOP_TREE_TYPE - 1],
                                       false);
        }

        BLI_bvhtree_free(tree_edges_act);
      }

      if (tree_verts_act && tree_edges_remain) {
        /* Vert x Edge */
        bm_elemxelem_bvhtree_overlap(tree_edges_remain,
                                     tree_verts_act,
                                     bm_edgexvert_isect_cb,
                                     &data,
                                     &pair_stack[KDOP_TREE_TYPE - 1],
                                     false);
      }

      BLI_bvhtree_free(tree_edges_remain);

      pair_len = 0;
      for (i = KDOP_TREE_TYPE; i--;) {
        if (pair_stack[i]) {
          pair_len += BLI_stack_count(pair_stack[i]);
        }
      }

      int edge_x_elem_pair_len = pair_len - vert_x_vert_pair_len;
      if (edge_x_elem_pair_len) {
        pair_array = MEM_mallocN(sizeof(*pair_array) * pair_len, __func__);

        pair_iter = pair_array;
        for (i = 0; i < KDOP_TREE_TYPE; i++) {
          if (pair_stack[i]) {
            uint count = (uint)BLI_stack_count(pair_stack[i]);
            BLI_stack_pop_n_reverse(pair_stack[i], pair_iter, count);
            pair_iter += count;
          }
        }

        /* Map intersections per edge. */
        union {
          struct {
            int cuts_len;
            int cuts_index[];
          };
          int as_int[0];
        } * e_map_iter, *e_map;

        size_t e_map_size = (data.cut_edges_len * sizeof(*e_map)) +
                            (2 * (size_t)edge_x_elem_pair_len * sizeof(*(e_map->cuts_index)));

        e_map = MEM_mallocN(e_map_size, __func__);
        int map_len = 0;

        /* Convert every pair to Vert x Vert. */

        /* The list of pairs starts with [vert x vert] followed by [edge x edge]
         * and finally [edge x vert].
         * Ignore the [vert x vert] pairs */
        struct EDBMSplitElem *pair_flat, *pair_flat_iter;
        pair_flat = (struct EDBMSplitElem *)&pair_array[vert_x_vert_pair_len];
        pair_flat_iter = &pair_flat[0];
        uint pair_flat_len = 2 * edge_x_elem_pair_len;
        for (i = 0; i < pair_flat_len; i++, pair_flat_iter++) {
          if (pair_flat_iter->elem->head.htype != BM_EDGE) {
            continue;
          }

          e = pair_flat_iter->edge;
          if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
            BM_elem_flag_enable(e, BM_ELEM_TAG);
            int e_cuts_len = e->head.index;

            e_map_iter = (void *)&e_map->as_int[map_len];
            e_map_iter->cuts_len = e_cuts_len;
            e_map_iter->cuts_index[0] = i;

            /* Use `e->head.index` to indicate which slot to fill with the `cut` index. */
            e->head.index = map_len + 1;
            map_len += 1 + e_cuts_len;
          }
          else {
            e_map->as_int[++e->head.index] = i;
          }
        }

        /* Split Edges A to set all Vert x Edge. */
        for (i = 0; i < map_len;
             e_map_iter = (void *)&e_map->as_int[i], i += 1 + e_map_iter->cuts_len) {

          /* sort by lambda. */
          BLI_qsort_r(e_map_iter->cuts_index,
                      e_map_iter->cuts_len,
                      sizeof(*(e_map->cuts_index)),
                      sort_cmp_by_lambda_cb,
                      pair_flat);

          float lambda, lambda_prev = 0.0f;
          for (int j = 0; j < e_map_iter->cuts_len; j++) {
            uint index = e_map_iter->cuts_index[j];

            struct EDBMSplitElem *pair_elem = &pair_flat[index];
            lambda = (pair_elem->lambda - lambda_prev) / (1.0f - lambda_prev);
            lambda_prev = pair_elem->lambda;
            e = pair_elem->edge;

            BMVert *v_new = BM_edge_split(bm, e, e->v1, NULL, lambda);
            v_new->head.index = -1;
            pair_elem->vert = v_new;
          }
        }

        MEM_freeN(e_map);
      }
    }
  }

  BLI_bvhtree_free(tree_verts_act);
  BLI_bvhtree_free(tree_verts_remain);

  if (r_targetmap) {
    if (pair_len && pair_array == NULL) {
      pair_array = MEM_mallocN(sizeof(*pair_array) * pair_len, __func__);
      pair_iter = pair_array;
      for (i = 0; i < KDOP_TREE_TYPE; i++) {
        if (pair_stack[i]) {
          uint count = (uint)BLI_stack_count(pair_stack[i]);
          BLI_stack_pop_n_reverse(pair_stack[i], pair_iter, count);
          pair_iter += count;
        }
      }
    }

    if (pair_array) {
      pair_iter = &pair_array[0];
      for (i = 0; i < pair_len; i++, pair_iter++) {
        BLI_assert((*pair_iter)[0].elem->head.htype == BM_VERT);
        BLI_assert((*pair_iter)[1].elem->head.htype == BM_VERT);
        BLI_assert((*pair_iter)[0].elem != (*pair_iter)[1].elem);

        BLI_ghash_insert(r_targetmap, (*pair_iter)[0].vert, (*pair_iter)[1].vert);
      }

      ok = true;
    }
  }

  for (i = KDOP_TREE_TYPE; i--;) {
    if (pair_stack[i]) {
      BLI_stack_free(pair_stack[i]);
    }
  }
  if (pair_array) {
    MEM_freeN(pair_array);
  }

  return ok;
}

/** \} */
