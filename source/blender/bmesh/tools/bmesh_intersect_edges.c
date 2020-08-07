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

#include "atomic_ops.h"

#include "bmesh.h"

#include "bmesh_intersect_edges.h" /* own include */

//#define INTERSECT_EDGES_DEBUG

#define KDOP_TREE_TYPE 4
#define KDOP_AXIS_LEN 14
#define BLI_STACK_PAIR_LEN 2 * KDOP_TREE_TYPE

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
  float best_edgenet_range_on_face_normal;
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

  BMEdge **e_iter = &data->edgenet[0];
  BMEdge *e_next = data->edgenet[1];
  BMVert *v_test = ELEM((*e_iter)->v1, e_next->v1, e_next->v2) ? (*e_iter)->v2 : (*e_iter)->v1;

  int verts_len = data->edgenet_len - 1;
  for (int i = verts_len; i--; e_iter++) {
    v_test = BM_edge_other_vert(*e_iter, v_test);
    BLI_assert(v_test != NULL);
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

  const float test_edgenet_range_on_face_normal = max - min;
  if (test_edgenet_range_on_face_normal < data->best_edgenet_range_on_face_normal) {
    data->best_edgenet_range_on_face_normal = test_edgenet_range_on_face_normal;
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
  const float range_min = -FLT_EPSILON;
  const float range_max = 1.0f + FLT_EPSILON;

  float co[3];
  float dir[3];
  float lambda_b;

  copy_v3_v3(co, l_a->prev->v->co);
  sub_v3_v3v3(dir, l_a->next->v->co, co);
  if (isect_ray_ray_v3(v_a_co, v_a_b_dir, co, dir, NULL, &lambda_b)) {
    if (IN_RANGE(lambda_b, range_min, range_max)) {
      return true;
    }
    copy_v3_v3(co, l_b->prev->v->co);
    sub_v3_v3v3(dir, l_b->next->v->co, co);
    if (isect_ray_ray_v3(v_a_co, v_a_b_dir, co, dir, NULL, &lambda_b)) {
      return IN_RANGE(lambda_b, range_min, range_max);
    }
  }
  return false;
}

static BMFace *bm_vert_pair_best_face_get(
    BMVert *v_a, BMVert *v_b, BMEdge **edgenet, const int edgenet_len, const float epsilon)
{
  BMFace *r_best_face = NULL;

  BLI_assert(v_a != v_b);

  BMLoop *dummy;
  if (edgenet_len == 1) {
    float data[2][3];
    copy_v3_v3(data[0], v_b->co);
    sub_v3_v3v3(data[1], v_a->co, data[0]);
    r_best_face = BM_vert_pair_shared_face_cb(
        v_a, v_b, false, bm_vert_pair_share_splittable_face_cb, &data, &dummy, &dummy);
    BLI_assert(!r_best_face || BM_edge_in_face(edgenet[0], r_best_face) == false);
  }
  else {
    struct EDBMSplitBestFaceData data = {
        .edgenet = edgenet,
        .edgenet_len = edgenet_len,
        .best_edgenet_range_on_face_normal = FLT_MAX,
        .r_best_face = NULL,
    };
    BM_vert_pair_shared_face_cb(
        v_a, v_b, true, bm_vert_pair_share_best_splittable_face_cb, &data, &dummy, &dummy);

    if (data.r_best_face) {
      /* Check if the edgenet's range is smaller than the face's range. */
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
      float face_range_on_normal = max - min + 2 * epsilon;
      if (face_range_on_normal < data.best_edgenet_range_on_face_normal) {
        data.r_best_face = NULL;
      }
    }
    r_best_face = data.r_best_face;
  }

  return r_best_face;
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

  /* Even though we have multiple atomic operations, this is fine here, since
   * there is no dependency on order.
   * The `e->head.index` check + atomic increment will ever be true once, as
   * expected. We don't care which instance of the code actually ends up
   * incrementing `r_data_cut_edge_len`, so there is no race condition here. */
  if (atomic_fetch_and_add_int32(&e->head.index, 1) == 0) {
    atomic_fetch_and_add_int32(r_data_cut_edges_len, 1);
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
    float dist_sq_vert = square_f(dist_sq_vert_factor) * len_squared_v3(dir);
    if (dist_sq_vert < data_dist_sq) {
      /* Vert x Vert is already handled elsewhere. */
      return false;
    }

    float closest[3];
    madd_v3_v3v3fl(closest, co, dir, lambda);

    float dist_sq = len_squared_v3v3(v->co, closest);
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

    float dist_sq_va = square_f(dist_sq_va_factor) * len_squared_v3(dir_a);
    float dist_sq_vb = square_f(dist_sq_vb_factor) * len_squared_v3(dir_b);

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
                                         BLI_Stack **pair_stack)
{
  int parallel_tasks_num = BLI_bvhtree_overlap_thread_num(tree1);
  for (int i = 0; i < parallel_tasks_num; i++) {
    if (pair_stack[i] == NULL) {
      pair_stack[i] = BLI_stack_new(sizeof(const struct EDBMSplitElem[2]), __func__);
    }
  }
  data->pair_stack = pair_stack;
  BLI_bvhtree_overlap_ex(tree1, tree2, NULL, callback, data, 1, BVH_OVERLAP_USE_THREADING);
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
  return -1;
}

/* -------------------------------------------------------------------- */
/* Main API */

#define INTERSECT_EDGES

bool BM_mesh_intersect_edges(
    BMesh *bm, const char hflag, const float dist, const bool split_faces, GHash *r_targetmap)
{
  bool ok = false;

  BMIter iter;
  BMVert *v;
  BMEdge *e;
  int i;

  /* Store all intersections in this array. */
  struct EDBMSplitElem(*pair_iter)[2], (*pair_array)[2] = NULL;
  int pair_len = 0;

  BLI_Stack *pair_stack[BLI_STACK_PAIR_LEN] = {NULL};
  BLI_Stack **pair_stack_vertxvert = pair_stack;
  BLI_Stack **pair_stack_edgexelem = &pair_stack[KDOP_TREE_TYPE];

  const float dist_sq = square_f(dist);
  const float dist_half = dist / 2;

  struct EDBMSplitData data = {
      .bm = bm,
      .pair_stack = pair_stack,
      .cut_edges_len = 0,
      .dist_sq = dist_sq,
      .dist_sq_sq = square_f(dist_sq),
  };

  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE);

  /* tag and count the verts to be tested. */
  int verts_act_len = 0, verts_remain_len = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, hflag)) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
      verts_act_len++;
    }
    else {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
      if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        verts_remain_len++;
      }
    }

    /* The index will indicate which cut in pair_array this vertex belongs to. */
    BM_elem_index_set(v, -1);
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
          tree_verts_act, tree_verts_act, bm_vertxvert_self_isect_cb, &data, pair_stack_vertxvert);
    }

    if (tree_verts_remain) {
      BLI_bvhtree_balance(tree_verts_remain);
    }

    if (tree_verts_act && tree_verts_remain) {
      bm_elemxelem_bvhtree_overlap(
          tree_verts_remain, tree_verts_act, bm_vertxvert_isect_cb, &data, pair_stack_vertxvert);
    }
  }

  for (i = KDOP_TREE_TYPE; i--;) {
    if (pair_stack_vertxvert[i]) {
      pair_len += BLI_stack_count(pair_stack_vertxvert[i]);
    }
  }

#ifdef INTERSECT_EDGES
  uint vertxvert_pair_len = pair_len;

#  define EDGE_ACT_TO_TEST 1
#  define EDGE_REMAIN_TO_TEST 2
  /* Tag and count the edges. */
  int edges_act_len = 0, edges_remain_len = 0;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) ||
        (len_squared_v3v3(e->v1->co, e->v2->co) < dist_sq)) {
      /* Don't test hidden edges or smaller than the minimum distance.
       * These have already been handled in the vertices overlap. */
      BM_elem_index_set(e, 0);
      if (split_faces) {
        /* Tag to be ignored. */
        BM_elem_flag_enable(e, BM_ELEM_TAG);
      }
      continue;
    }

    if (BM_elem_flag_test(e->v1, BM_ELEM_TAG) || BM_elem_flag_test(e->v2, BM_ELEM_TAG)) {
      BM_elem_index_set(e, EDGE_ACT_TO_TEST);
      edges_act_len++;
    }
    else {
      BM_elem_index_set(e, EDGE_REMAIN_TO_TEST);
      edges_remain_len++;
      if (split_faces) {
        /* Tag to be ignored. */
        BM_elem_flag_enable(e, BM_ELEM_TAG);
      }
    }
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
        BLI_assert(tree_edges_remain);
        e->head.index = 0;
        copy_v3_v3(co[0], e->v1->co);
        copy_v3_v3(co[1], e->v2->co);
        BLI_bvhtree_insert(tree_edges_remain, i, co[0], 2);
      }
#  ifdef INTERSECT_EDGES_DEBUG
      else {
        e->head.index = 0;
      }
#  endif
      /* Tag used when converting pairs to vert x vert. */
      BM_elem_flag_disable(e, BM_ELEM_TAG);
    }
#  undef EDGE_ACT_TO_TEST
#  undef EDGE_REMAIN_TO_TEST

    /* Use `e->head.index` to count intersections. */
    bm->elem_index_dirty |= BM_EDGE;

    if (tree_edges_act) {
      BLI_bvhtree_balance(tree_edges_act);
    }

    if (tree_edges_remain) {
      BLI_bvhtree_balance(tree_edges_remain);
    }

    int edgexedge_pair_len = 0;
    if (tree_edges_act) {
      /* Edge x Edge */
      bm_elemxelem_bvhtree_overlap(
          tree_edges_act, tree_edges_act, bm_edgexedge_self_isect_cb, &data, pair_stack_edgexelem);

      if (tree_edges_remain) {
        bm_elemxelem_bvhtree_overlap(
            tree_edges_remain, tree_edges_act, bm_edgexedge_isect_cb, &data, pair_stack_edgexelem);
      }

      for (i = KDOP_TREE_TYPE; i--;) {
        if (pair_stack_edgexelem[i]) {
          edgexedge_pair_len += BLI_stack_count(pair_stack_edgexelem[i]);
        }
      }

      if (tree_verts_act) {
        /* Edge v Vert */
        bm_elemxelem_bvhtree_overlap(
            tree_edges_act, tree_verts_act, bm_edgexvert_isect_cb, &data, pair_stack_edgexelem);
      }

      if (tree_verts_remain) {
        /* Edge v Vert */
        bm_elemxelem_bvhtree_overlap(
            tree_edges_act, tree_verts_remain, bm_edgexvert_isect_cb, &data, pair_stack_edgexelem);
      }

      BLI_bvhtree_free(tree_edges_act);
    }

    if (tree_verts_act && tree_edges_remain) {
      /* Edge v Vert */
      bm_elemxelem_bvhtree_overlap(
          tree_edges_remain, tree_verts_act, bm_edgexvert_isect_cb, &data, pair_stack_edgexelem);
    }

    BLI_bvhtree_free(tree_edges_remain);

    int edgexelem_pair_len = 0;
    for (i = KDOP_TREE_TYPE; i--;) {
      if (pair_stack_edgexelem[i]) {
        edgexelem_pair_len += BLI_stack_count(pair_stack_edgexelem[i]);
      }
    }

    pair_len += edgexelem_pair_len;
    int edgexvert_pair_len = edgexelem_pair_len - edgexedge_pair_len;

    if (edgexelem_pair_len) {
      pair_array = MEM_mallocN(sizeof(*pair_array) * pair_len, __func__);

      pair_iter = pair_array;
      for (i = 0; i < BLI_STACK_PAIR_LEN; i++) {
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

#  ifdef INTERSECT_EDGES_DEBUG
      int cut_edges_len = 0;
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        if (e->head.index != 0) {
          cut_edges_len++;
        }
      }
      BLI_assert(cut_edges_len == data.cut_edges_len);
#  endif

      size_t e_map_size = (data.cut_edges_len * sizeof(*e_map)) +
                          (((size_t)2 * edgexedge_pair_len + edgexvert_pair_len) *
                           sizeof(*(e_map->cuts_index)));

      e_map = MEM_mallocN(e_map_size, __func__);
      int map_len = 0;

      /* Convert every pair to Vert x Vert. */

      /* The list of pairs starts with [vert x vert] followed by [edge x edge]
       * and finally [edge x vert].
       * Ignore the [vert x vert] pairs */
      struct EDBMSplitElem *pair_flat, *pair_flat_iter;
      pair_flat = (struct EDBMSplitElem *)&pair_array[vertxvert_pair_len];
      pair_flat_iter = &pair_flat[0];
      uint pair_flat_len = 2 * edgexelem_pair_len;
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
          if (split_faces) {
            /* Tagged edges are ignored when split faces.
             * Un-tag these. */
            BM_elem_flag_disable(e, BM_ELEM_TAG);
          }

          BMVert *v_new = BM_edge_split(bm, e, e->v1, NULL, lambda);
          pair_elem->vert = v_new;
        }
      }

      MEM_freeN(e_map);
    }
  }
#endif

  BLI_bvhtree_free(tree_verts_act);
  BLI_bvhtree_free(tree_verts_remain);

  if (r_targetmap) {
    if (pair_len && pair_array == NULL) {
      pair_array = MEM_mallocN(sizeof(*pair_array) * pair_len, __func__);
      pair_iter = pair_array;
      for (i = 0; i < BLI_STACK_PAIR_LEN; i++) {
        if (pair_stack[i]) {
          uint count = (uint)BLI_stack_count(pair_stack[i]);
          BLI_stack_pop_n_reverse(pair_stack[i], pair_iter, count);
          pair_iter += count;
        }
      }
    }

    if (pair_array) {
      BMVert *v_key, *v_val;
      pair_iter = &pair_array[0];
      for (i = 0; i < pair_len; i++, pair_iter++) {
        BLI_assert((*pair_iter)[0].elem->head.htype == BM_VERT);
        BLI_assert((*pair_iter)[1].elem->head.htype == BM_VERT);
        BLI_assert((*pair_iter)[0].elem != (*pair_iter)[1].elem);
        v_key = (*pair_iter)[0].vert;
        v_val = (*pair_iter)[1].vert;
        BLI_ghash_insert(r_targetmap, v_key, v_val);
      }

      /**
       * The weld_verts operator works best when all keys in the same group of
       * collapsed vertices point to the same vertex.
       * That is, if the pairs of vertices are:
       *   [1, 2], [2, 3] and [3, 4],
       * They are better adjusted to:
       *   [1, 4], [2, 4] and [3, 4].
       */
      pair_iter = &pair_array[0];
      for (i = 0; i < pair_len; i++, pair_iter++) {
        v_key = (*pair_iter)[0].vert;
        v_val = (*pair_iter)[1].vert;
        BMVert *v_target;
        while ((v_target = BLI_ghash_lookup(r_targetmap, v_val))) {
          v_val = v_target;
        }
        if (v_val != (*pair_iter)[1].vert) {
          BMVert **v_val_p = (BMVert **)BLI_ghash_lookup_p(r_targetmap, v_key);
          *v_val_p = (*pair_iter)[1].vert = v_val;
        }
        if (split_faces) {
          /* The vertex index indicates its position in the pair_array flat. */
          BM_elem_index_set(v_key, i * 2);
          BM_elem_index_set(v_val, i * 2 + 1);
        }
      }

      if (split_faces) {
        BMEdge **edgenet = NULL;
        int edgenet_alloc_len = 0;

        struct EDBMSplitElem *pair_flat = (struct EDBMSplitElem *)&pair_array[0];
        BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
            /* Edge out of context or already tested. */
            continue;
          }

          BMVert *va, *vb, *va_dest = NULL;
          va = e->v1;
          vb = e->v2;

          int v_cut = BM_elem_index_get(va);
          int v_cut_other = BM_elem_index_get(vb);
          if (v_cut == -1 && v_cut_other == -1) {
            if (!BM_elem_flag_test(va, BM_ELEM_TAG) && !BM_elem_flag_test(vb, BM_ELEM_TAG)) {
              /* Edge out of context. */
              BM_elem_flag_enable(e, BM_ELEM_TAG);
            }
            continue;
          }

          /* Tag to avoid testing again. */
          BM_elem_flag_enable(e, BM_ELEM_TAG);

          if (v_cut == -1) {
            SWAP(BMVert *, va, vb);
            v_cut = v_cut_other;
            v_cut_other = -1;
          }

          /* `v_cut` indicates the other vertex within the `pair_array`. */
          v_cut += v_cut % 2 ? -1 : 1;
          va_dest = pair_flat[v_cut].vert;

          if (BM_vert_pair_share_face_check(va, va_dest)) {
            /* Vert par acts on the same face.
             * Although there are cases like this where the face can be split,
             * for efficiency it is better to ignore then. */
            continue;
          }

          BMFace *best_face = NULL;
          BMVert *v_other_dest, *v_other = vb;
          BMEdge *e_net = e;
          int edgenet_len = 0;
          while (true) {
            if (v_cut_other != -1) {
              v_cut_other += v_cut_other % 2 ? -1 : 1;
              v_other_dest = pair_flat[v_cut_other].vert;

              if (BM_vert_pair_share_face_check(v_other, v_other_dest)) {
                /* Vert par acts on the same face.
                 * Although there are cases like this where the face can be split,
                 * for efficiency and to avoid complications, it is better to ignore these cases.
                 */
                break;
              }
            }
            else {
              v_other_dest = v_other;
            }

            if (va_dest == v_other_dest) {
              /* Edge/Edgenet to vertex - we can't split the face. */
              break;
            }
            if (edgenet_len == 0 && BM_edge_exists(va_dest, v_other_dest)) {
              /* Edge to edge - no need to detect face. */
              break;
            }

            if (edgenet_alloc_len == edgenet_len) {
              edgenet_alloc_len = (edgenet_alloc_len + 1) * 2;
              edgenet = MEM_reallocN(edgenet, (edgenet_alloc_len) * sizeof(*edgenet));
            }
            edgenet[edgenet_len++] = e_net;

            best_face = bm_vert_pair_best_face_get(
                va_dest, v_other_dest, edgenet, edgenet_len, dist);

            if (best_face) {
              if ((va_dest != va) && !BM_edge_exists(va_dest, va)) {
                /**
                 * <pre>
                 *  va---vb---
                 *      /
                 *  va_dest
                 * </pre>
                 */
                e_net = edgenet[0];
                if (edgenet_len > 1) {
                  vb = BM_edge_other_vert(e_net, va);
                }
                else {
                  vb = v_other_dest;
                }
                edgenet[0] = BM_edge_create(bm, va_dest, vb, e_net, BM_CREATE_NOP);
              }
              if ((edgenet_len > 1) && (v_other_dest != v_other) &&
                  !BM_edge_exists(v_other_dest, v_other)) {
                /**
                 * <pre>
                 *  ---v---v_other
                 *      \
                 *       v_other_dest
                 * </pre>
                 */
                e_net = edgenet[edgenet_len - 1];
                edgenet[edgenet_len - 1] = BM_edge_create(
                    bm, v_other_dest, BM_edge_other_vert(e_net, v_other), e_net, BM_CREATE_NOP);
              }
              break;
            }

            BMEdge *e_test = e_net, *e_next = NULL;
            while ((e_test = BM_DISK_EDGE_NEXT(e_test, v_other)) != (e_net)) {
              if (!BM_edge_is_wire(e_test)) {
                if (BM_elem_flag_test(e_test, BM_ELEM_TAG)) {
                  continue;
                }
                if (!BM_elem_flag_test(e_test->v1, BM_ELEM_TAG) &&
                    !BM_elem_flag_test(e_test->v2, BM_ELEM_TAG)) {
                  continue;
                }
                /* Avoids endless loop. */
                BM_elem_flag_enable(e_test, BM_ELEM_TAG);
              }
              else if (!BM_edge_is_wire(e_net)) {
                continue;
              }
              e_next = e_test;
              break;
            }

            if (e_next == NULL) {
              break;
            }

            e_net = e_next;
            v_other = BM_edge_other_vert(e_net, v_other);
            if (v_other == va) {
              /* Endless loop. */
              break;
            }
            v_cut_other = BM_elem_index_get(v_other);
          }

          if (best_face) {
            BMFace **face_arr = NULL;
            int face_arr_len = 0;
            BM_face_split_edgenet(bm, best_face, edgenet, edgenet_len, &face_arr, &face_arr_len);
            if (face_arr) {
              /* Update the new faces normal.
               * Normal is necessary to obtain the best face for edgenet */
              while (face_arr_len--) {
                BM_face_normal_update(face_arr[face_arr_len]);
              }
              MEM_freeN(face_arr);
            }
          }
        }

        if (edgenet) {
          MEM_freeN(edgenet);
        }
      }
      ok = true;
    }
  }

  for (i = BLI_STACK_PAIR_LEN; i--;) {
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
