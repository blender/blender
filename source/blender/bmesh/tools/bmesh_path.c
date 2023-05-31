/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Find a path between 2 elements.
 *
 * \note All 3 functions are similar, changes to one most likely apply to another.
 */

#include "MEM_guardedalloc.h"

#include "BLI_heap_simple.h"
#include "BLI_linklist.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_path.h" /* own include */

#define COST_INIT_MAX FLT_MAX

/* -------------------------------------------------------------------- */
/** \name Generic Helpers
 * \{ */

/**
 * Use skip options when we want to start measuring from a boundary.
 */
static float step_cost_3_v3_ex(
    const float v1[3], const float v2[3], const float v3[3], bool skip_12, bool skip_23)
{
  float d1[3], d2[3];

  /* The cost is based on the simple sum of the length of the two edges. */
  sub_v3_v3v3(d1, v2, v1);
  sub_v3_v3v3(d2, v3, v2);
  const float cost_12 = normalize_v3(d1);
  const float cost_23 = normalize_v3(d2);
  const float cost = ((skip_12 ? 0.0f : cost_12) + (skip_23 ? 0.0f : cost_23));

  /* But is biased to give higher values to sharp turns, so that it will take paths with
   * fewer "turns" when selecting between equal-weighted paths between the two edges. */
  return cost * (1.0f + 0.5f * (2.0f - sqrtf(fabsf(dot_v3v3(d1, d2)))));
}

static float step_cost_3_v3(const float v1[3], const float v2[3], const float v3[3])
{
  return step_cost_3_v3_ex(v1, v2, v3, false, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BM_mesh_calc_path_vert
 * \{ */

static void verttag_add_adjacent(HeapSimple *heap,
                                 BMVert *v_a,
                                 BMVert **verts_prev,
                                 float *cost,
                                 const struct BMCalcPathParams *params)
{
  const int v_a_index = BM_elem_index_get(v_a);

  {
    BMIter eiter;
    BMEdge *e;
    /* Loop over faces of face, but do so by first looping over loops. */
    BM_ITER_ELEM (e, &eiter, v_a, BM_EDGES_OF_VERT) {
      BMVert *v_b = BM_edge_other_vert(e, v_a);
      if (!BM_elem_flag_test(v_b, BM_ELEM_TAG)) {
        /* We know 'v_b' is not visited, check it out! */
        const int v_b_index = BM_elem_index_get(v_b);
        const float cost_cut = params->use_topology_distance ? 1.0f : len_v3v3(v_a->co, v_b->co);
        const float cost_new = cost[v_a_index] + cost_cut;

        if (cost[v_b_index] > cost_new) {
          cost[v_b_index] = cost_new;
          verts_prev[v_b_index] = v_a;
          BLI_heapsimple_insert(heap, cost_new, v_b);
        }
      }
    }
  }

  if (params->use_step_face) {
    BMIter liter;
    BMLoop *l;
    /* Loop over faces of face, but do so by first looping over loops. */
    BM_ITER_ELEM (l, &liter, v_a, BM_LOOPS_OF_VERT) {
      if (l->f->len > 3) {
        /* Skip loops on adjacent edges. */
        BMLoop *l_iter = l->next->next;
        do {
          BMVert *v_b = l_iter->v;
          if (!BM_elem_flag_test(v_b, BM_ELEM_TAG)) {
            /* We know 'v_b' is not visited, check it out! */
            const int v_b_index = BM_elem_index_get(v_b);
            const float cost_cut = params->use_topology_distance ? 1.0f :
                                                                   len_v3v3(v_a->co, v_b->co);
            const float cost_new = cost[v_a_index] + cost_cut;

            if (cost[v_b_index] > cost_new) {
              cost[v_b_index] = cost_new;
              verts_prev[v_b_index] = v_a;
              BLI_heapsimple_insert(heap, cost_new, v_b);
            }
          }
        } while ((l_iter = l_iter->next) != l->prev);
      }
    }
  }
}

LinkNode *BM_mesh_calc_path_vert(BMesh *bm,
                                 BMVert *v_src,
                                 BMVert *v_dst,
                                 const struct BMCalcPathParams *params,
                                 bool (*filter_fn)(BMVert *, void *user_data),
                                 void *user_data)
{
  LinkNode *path = NULL;
  /* #BM_ELEM_TAG flag is used to store visited edges. */
  BMVert *v;
  BMIter viter;
  HeapSimple *heap;
  float *cost;
  BMVert **verts_prev;
  int i, totvert;

  /* NOTE: would pass #BM_EDGE except we are looping over all faces anyway. */
  // BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */); // NOT NEEDED FOR FACETAG

  BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
    BM_elem_flag_set(v, BM_ELEM_TAG, !filter_fn(v, user_data));
    BM_elem_index_set(v, i); /* set_inline */
  }
  bm->elem_index_dirty &= ~BM_VERT;

  /* Allocate. */
  totvert = bm->totvert;
  verts_prev = MEM_callocN(sizeof(*verts_prev) * totvert, __func__);
  cost = MEM_mallocN(sizeof(*cost) * totvert, __func__);

  copy_vn_fl(cost, totvert, COST_INIT_MAX);

  /*
   * Arrays are now filled as follows:
   *
   * As the search continues, `verts_prev[n]` will be the previous verts on the shortest
   * path found so far to face `n`. #BM_ELEM_TAG is used to tag elements we have visited,
   * `cost[n]` will contain the length of the shortest
   * path to face n found so far, Finally, heap is a priority heap which is built on the
   * the same data as the cost array, but inverted: it is a work-list of faces prioritized
   * by the shortest path found so far to the face.
   */

  /* Regular dijkstra shortest path, but over faces instead of vertices. */
  heap = BLI_heapsimple_new();
  BLI_heapsimple_insert(heap, 0.0f, v_src);
  cost[BM_elem_index_get(v_src)] = 0.0f;

  while (!BLI_heapsimple_is_empty(heap)) {
    v = BLI_heapsimple_pop_min(heap);

    if (v == v_dst) {
      break;
    }

    if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
      verttag_add_adjacent(heap, v, verts_prev, cost, params);
    }
  }

  if (v == v_dst) {
    do {
      BLI_linklist_prepend(&path, v);
    } while ((v = verts_prev[BM_elem_index_get(v)]));
  }

  MEM_freeN(verts_prev);
  MEM_freeN(cost);
  BLI_heapsimple_free(heap, NULL);

  return path;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BM_mesh_calc_path_edge
 * \{ */

static float edgetag_cut_cost_vert(BMEdge *e_a, BMEdge *e_b, BMVert *v)
{
  BMVert *v1 = BM_edge_other_vert(e_a, v);
  BMVert *v2 = BM_edge_other_vert(e_b, v);
  return step_cost_3_v3(v1->co, v->co, v2->co);
}

static float edgetag_cut_cost_face(BMEdge *e_a, BMEdge *e_b, BMFace *f)
{
  float e_a_cent[3], e_b_cent[3], f_cent[3];

  mid_v3_v3v3(e_a_cent, e_a->v1->co, e_a->v1->co);
  mid_v3_v3v3(e_b_cent, e_b->v1->co, e_b->v1->co);

  BM_face_calc_center_median_weighted(f, f_cent);

  return step_cost_3_v3(e_a_cent, e_b_cent, f_cent);
}

static void edgetag_add_adjacent(HeapSimple *heap,
                                 BMEdge *e_a,
                                 BMEdge **edges_prev,
                                 float *cost,
                                 const struct BMCalcPathParams *params)
{
  const int e_a_index = BM_elem_index_get(e_a);

  /* Unlike vert/face, stepping faces disables scanning connected edges
   * and only steps over faces (selecting a ring of edges instead of a loop). */
  if (params->use_step_face == false || e_a->l == NULL) {
    BMIter viter;
    BMVert *v;

    BMIter eiter;
    BMEdge *e_b;

    BM_ITER_ELEM (v, &viter, e_a, BM_VERTS_OF_EDGE) {

      /* Don't walk over previous vertex. */
      if ((edges_prev[e_a_index]) && BM_vert_in_edge(edges_prev[e_a_index], v)) {
        continue;
      }

      BM_ITER_ELEM (e_b, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e_b, BM_ELEM_TAG)) {
          /* We know 'e_b' is not visited, check it out! */
          const int e_b_index = BM_elem_index_get(e_b);
          const float cost_cut = params->use_topology_distance ?
                                     1.0f :
                                     edgetag_cut_cost_vert(e_a, e_b, v);
          const float cost_new = cost[e_a_index] + cost_cut;

          if (cost[e_b_index] > cost_new) {
            cost[e_b_index] = cost_new;
            edges_prev[e_b_index] = e_a;
            BLI_heapsimple_insert(heap, cost_new, e_b);
          }
        }
      }
    }
  }
  else {
    BMLoop *l_first, *l_iter;

    l_iter = l_first = e_a->l;
    do {
      BMLoop *l_cycle_iter, *l_cycle_end;

      l_cycle_iter = l_iter->next;
      l_cycle_end = l_iter;

      /* Good, but we need to allow this otherwise paths may fail to connect at all. */
#if 0
      if (l_iter->f->len > 3) {
        l_cycle_iter = l_cycle_iter->next;
        l_cycle_end = l_cycle_end->prev;
      }
#endif

      do {
        BMEdge *e_b = l_cycle_iter->e;
        if (!BM_elem_flag_test(e_b, BM_ELEM_TAG)) {
          /* We know 'e_b' is not visited, check it out! */
          const int e_b_index = BM_elem_index_get(e_b);
          const float cost_cut = params->use_topology_distance ?
                                     1.0f :
                                     edgetag_cut_cost_face(e_a, e_b, l_iter->f);
          const float cost_new = cost[e_a_index] + cost_cut;

          if (cost[e_b_index] > cost_new) {
            cost[e_b_index] = cost_new;
            edges_prev[e_b_index] = e_a;
            BLI_heapsimple_insert(heap, cost_new, e_b);
          }
        }
      } while ((l_cycle_iter = l_cycle_iter->next) != l_cycle_end);
    } while ((l_iter = l_iter->radial_next) != l_first);
  }
}

LinkNode *BM_mesh_calc_path_edge(BMesh *bm,
                                 BMEdge *e_src,
                                 BMEdge *e_dst,
                                 const struct BMCalcPathParams *params,
                                 bool (*filter_fn)(BMEdge *, void *user_data),
                                 void *user_data)
{
  LinkNode *path = NULL;
  /* #BM_ELEM_TAG flag is used to store visited edges. */
  BMEdge *e;
  BMIter eiter;
  HeapSimple *heap;
  float *cost;
  BMEdge **edges_prev;
  int i, totedge;

  /* NOTE: would pass #BM_EDGE except we are looping over all edges anyway. */
  BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */);

  BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
    BM_elem_flag_set(e, BM_ELEM_TAG, !filter_fn(e, user_data));
    BM_elem_index_set(e, i); /* set_inline */
  }
  bm->elem_index_dirty &= ~BM_EDGE;

  /* Allocate. */
  totedge = bm->totedge;
  edges_prev = MEM_callocN(sizeof(*edges_prev) * totedge, __func__);
  cost = MEM_mallocN(sizeof(*cost) * totedge, __func__);

  copy_vn_fl(cost, totedge, COST_INIT_MAX);

  /*
   * Arrays are now filled as follows:
   *
   * As the search continues, `edges_prev[n]` will be the previous edge on the shortest
   * path found so far to edge `n`. #BM_ELEM_TAG is used to tag elements we have visited,
   * `cost[n]` will contain the length of the shortest
   * path to edge n found so far, Finally, heap is a priority heap which is built on the
   * the same data as the cost array, but inverted: it is a work-list of edges prioritized
   * by the shortest path found so far to the edge.
   */

  /* Regular dijkstra shortest path, but over edges instead of vertices. */
  heap = BLI_heapsimple_new();
  BLI_heapsimple_insert(heap, 0.0f, e_src);
  cost[BM_elem_index_get(e_src)] = 0.0f;

  while (!BLI_heapsimple_is_empty(heap)) {
    e = BLI_heapsimple_pop_min(heap);

    if (e == e_dst) {
      break;
    }

    if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
      BM_elem_flag_enable(e, BM_ELEM_TAG);
      edgetag_add_adjacent(heap, e, edges_prev, cost, params);
    }
  }

  if (e == e_dst) {
    do {
      BLI_linklist_prepend(&path, e);
    } while ((e = edges_prev[BM_elem_index_get(e)]));
  }

  MEM_freeN(edges_prev);
  MEM_freeN(cost);
  BLI_heapsimple_free(heap, NULL);

  return path;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BM_mesh_calc_path_face
 * \{ */

static float facetag_cut_cost_edge(BMFace *f_a,
                                   BMFace *f_b,
                                   BMEdge *e,
                                   const void *const f_endpoints[2])
{
  float f_a_cent[3];
  float f_b_cent[3];
  float e_cent[3];

  BM_face_calc_center_median_weighted(f_a, f_a_cent);
  BM_face_calc_center_median_weighted(f_b, f_b_cent);
#if 0
  mid_v3_v3v3(e_cent, e->v1->co, e->v2->co);
#else
  /* For triangle fans it gives better results to pick a point on the edge. */
  {
    float ix_e[3], ix_f[3];
    isect_line_line_v3(e->v1->co, e->v2->co, f_a_cent, f_b_cent, ix_e, ix_f);
    const float factor = line_point_factor_v3(ix_e, e->v1->co, e->v2->co);
    if (factor < 0.0f) {
      copy_v3_v3(e_cent, e->v1->co);
    }
    else if (factor > 1.0f) {
      copy_v3_v3(e_cent, e->v2->co);
    }
    else {
      copy_v3_v3(e_cent, ix_e);
    }
  }
#endif

  return step_cost_3_v3_ex(
      f_a_cent, e_cent, f_b_cent, (f_a == f_endpoints[0]), (f_b == f_endpoints[1]));
}

static float facetag_cut_cost_vert(BMFace *f_a,
                                   BMFace *f_b,
                                   BMVert *v,
                                   const void *const f_endpoints[2])
{
  float f_a_cent[3];
  float f_b_cent[3];

  BM_face_calc_center_median_weighted(f_a, f_a_cent);
  BM_face_calc_center_median_weighted(f_b, f_b_cent);

  return step_cost_3_v3_ex(
      f_a_cent, v->co, f_b_cent, (f_a == f_endpoints[0]), (f_b == f_endpoints[1]));
}

static void facetag_add_adjacent(HeapSimple *heap,
                                 BMFace *f_a,
                                 BMFace **faces_prev,
                                 float *cost,
                                 const void *const f_endpoints[2],
                                 const struct BMCalcPathParams *params)
{
  const int f_a_index = BM_elem_index_get(f_a);

  /* Loop over faces of face, but do so by first looping over loops. */
  {
    BMIter liter;
    BMLoop *l_a;

    BM_ITER_ELEM (l_a, &liter, f_a, BM_LOOPS_OF_FACE) {
      BMLoop *l_first, *l_iter;

      l_iter = l_first = l_a;
      do {
        BMFace *f_b = l_iter->f;
        if (!BM_elem_flag_test(f_b, BM_ELEM_TAG)) {
          /* We know 'f_b' is not visited, check it out! */
          const int f_b_index = BM_elem_index_get(f_b);
          const float cost_cut = params->use_topology_distance ?
                                     1.0f :
                                     facetag_cut_cost_edge(f_a, f_b, l_iter->e, f_endpoints);
          const float cost_new = cost[f_a_index] + cost_cut;

          if (cost[f_b_index] > cost_new) {
            cost[f_b_index] = cost_new;
            faces_prev[f_b_index] = f_a;
            BLI_heapsimple_insert(heap, cost_new, f_b);
          }
        }
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
  }

  if (params->use_step_face) {
    BMIter liter;
    BMLoop *l_a;

    BM_ITER_ELEM (l_a, &liter, f_a, BM_LOOPS_OF_FACE) {
      BMIter litersub;
      BMLoop *l_b;
      BM_ITER_ELEM (l_b, &litersub, l_a->v, BM_LOOPS_OF_VERT) {
        if ((l_a != l_b) && !BM_loop_share_edge_check(l_a, l_b)) {
          BMFace *f_b = l_b->f;
          if (!BM_elem_flag_test(f_b, BM_ELEM_TAG)) {
            /* We know 'f_b' is not visited, check it out! */
            const int f_b_index = BM_elem_index_get(f_b);
            const float cost_cut = params->use_topology_distance ?
                                       1.0f :
                                       facetag_cut_cost_vert(f_a, f_b, l_a->v, f_endpoints);
            const float cost_new = cost[f_a_index] + cost_cut;

            if (cost[f_b_index] > cost_new) {
              cost[f_b_index] = cost_new;
              faces_prev[f_b_index] = f_a;
              BLI_heapsimple_insert(heap, cost_new, f_b);
            }
          }
        }
      }
    }
  }
}

LinkNode *BM_mesh_calc_path_face(BMesh *bm,
                                 BMFace *f_src,
                                 BMFace *f_dst,
                                 const struct BMCalcPathParams *params,
                                 bool (*filter_fn)(BMFace *, void *user_data),
                                 void *user_data)
{
  LinkNode *path = NULL;
  /* #BM_ELEM_TAG flag is used to store visited edges. */
  BMFace *f;
  BMIter fiter;
  HeapSimple *heap;
  float *cost;
  BMFace **faces_prev;
  int i, totface;

  /* Start measuring face path at the face edges, ignoring their centers. */
  const void *const f_endpoints[2] = {f_src, f_dst};

  /* NOTE: would pass #BM_EDGE except we are looping over all faces anyway. */
  // BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */); // NOT NEEDED FOR FACETAG

  BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, i) {
    BM_elem_flag_set(f, BM_ELEM_TAG, !filter_fn(f, user_data));
    BM_elem_index_set(f, i); /* set_inline */
  }
  bm->elem_index_dirty &= ~BM_FACE;

  /* Allocate. */
  totface = bm->totface;
  faces_prev = MEM_callocN(sizeof(*faces_prev) * totface, __func__);
  cost = MEM_mallocN(sizeof(*cost) * totface, __func__);

  copy_vn_fl(cost, totface, COST_INIT_MAX);

  /*
   * Arrays are now filled as follows:
   *
   * As the search continues, `faces_prev[n]` will be the previous face on the shortest
   * path found so far to face `n`. #BM_ELEM_TAG is used to tag elements we have visited,
   * `cost[n]` will contain the length of the shortest
   * path to face n found so far, Finally, heap is a priority heap which is built on the
   * the same data as the cost array, but inverted: it is a work-list of faces prioritized
   * by the shortest path found so far to the face.
   */

  /* Regular dijkstra shortest path, but over faces instead of vertices. */
  heap = BLI_heapsimple_new();
  BLI_heapsimple_insert(heap, 0.0f, f_src);
  cost[BM_elem_index_get(f_src)] = 0.0f;

  while (!BLI_heapsimple_is_empty(heap)) {
    f = BLI_heapsimple_pop_min(heap);

    if (f == f_dst) {
      break;
    }

    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      BM_elem_flag_enable(f, BM_ELEM_TAG);
      facetag_add_adjacent(heap, f, faces_prev, cost, f_endpoints, params);
    }
  }

  if (f == f_dst) {
    do {
      BLI_linklist_prepend(&path, f);
    } while ((f = faces_prev[BM_elem_index_get(f)]));
  }

  MEM_freeN(faces_prev);
  MEM_freeN(cost);
  BLI_heapsimple_free(heap, NULL);

  return path;
}

/** \} */
