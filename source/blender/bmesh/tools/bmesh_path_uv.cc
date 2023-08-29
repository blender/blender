/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Find a path between 2 elements in UV space.
 */

#include "MEM_guardedalloc.h"

#include "BLI_heap_simple.h"
#include "BLI_linklist.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "bmesh_path_uv.h" /* own include */
#include "intern/bmesh_query.h"
#include "intern/bmesh_query_uv.h"

#define COST_INIT_MAX FLT_MAX

/* -------------------------------------------------------------------- */
/** \name Generic Helpers
 * \{ */

/**
 * Use skip options when we want to start measuring from a boundary.
 *
 * See #step_cost_3_v3_ex in `bmesh_path.cc` which follows the same logic.
 */
static float step_cost_3_v2_ex(
    const float v1[2], const float v2[2], const float v3[2], bool skip_12, bool skip_23)
{
  float d1[2], d2[2];

  /* The cost is based on the simple sum of the length of the two edges. */
  sub_v2_v2v2(d1, v2, v1);
  sub_v2_v2v2(d2, v3, v2);
  const float cost_12 = normalize_v2(d1);
  const float cost_23 = normalize_v2(d2);
  const float cost = ((skip_12 ? 0.0f : cost_12) + (skip_23 ? 0.0f : cost_23));

  /* But is biased to give higher values to sharp turns, so that it will take paths with
   * fewer "turns" when selecting between equal-weighted paths between the two edges. */
  return cost * (1.0f + 0.5f * (2.0f - sqrtf(fabsf(dot_v2v2(d1, d2)))));
}

static float step_cost_3_v2(const float v1[2], const float v2[2], const float v3[2])
{
  return step_cost_3_v2_ex(v1, v2, v3, false, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BM_mesh_calc_path_uv_vert
 * \{ */

static void verttag_add_adjacent_uv(HeapSimple *heap,
                                    BMLoop *l_a,
                                    BMLoop **loops_prev,
                                    float *cost,
                                    const BMCalcPathUVParams *params)
{
  BLI_assert(params->aspect_y != 0.0f);
  const int cd_loop_uv_offset = params->cd_loop_uv_offset;
  const int l_a_index = BM_elem_index_get(l_a);
  const float *luv_a = BM_ELEM_CD_GET_FLOAT_P(l_a, cd_loop_uv_offset);
  const float uv_a[2] = {luv_a[0], luv_a[1] / params->aspect_y};

  {
    BMIter liter;
    BMLoop *l;
    /* Loop over faces of face, but do so by first looping over loops. */
    BM_ITER_ELEM (l, &liter, l_a->v, BM_LOOPS_OF_VERT) {
      const float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
      if (equals_v2v2(luv_a, luv)) {
        /* 'l_a' is already tagged, tag all adjacent. */
        BM_elem_flag_enable(l, BM_ELEM_TAG);
        BMLoop *l_b = l->next;
        do {
          if (!BM_elem_flag_test(l_b, BM_ELEM_TAG)) {
            const float *luv_b = BM_ELEM_CD_GET_FLOAT_P(l_b, cd_loop_uv_offset);
            const float uv_b[2] = {luv_b[0], luv_b[1] / params->aspect_y};
            /* We know 'l_b' is not visited, check it out! */
            const int l_b_index = BM_elem_index_get(l_b);
            const float cost_cut = params->use_topology_distance ? 1.0f : len_v2v2(uv_a, uv_b);
            const float cost_new = cost[l_a_index] + cost_cut;

            if (cost[l_b_index] > cost_new) {
              cost[l_b_index] = cost_new;
              loops_prev[l_b_index] = l_a;
              BLI_heapsimple_insert(heap, cost_new, l_b);
            }
          }
          /* This means we only step onto `l->prev` & `l->next`. */
          if (params->use_step_face == false) {
            if (l_b == l->next) {
              l_b = l->prev->prev;
            }
          }
        } while ((l_b = l_b->next) != l);
      }
    }
  }
}

LinkNode *BM_mesh_calc_path_uv_vert(BMesh *bm,
                                    BMLoop *l_src,
                                    BMLoop *l_dst,
                                    const BMCalcPathUVParams *params,
                                    bool (*filter_fn)(BMLoop *, void *),
                                    void *user_data)
{
  LinkNode *path = nullptr;
  /* BM_ELEM_TAG flag is used to store visited edges */
  BMIter viter;
  HeapSimple *heap;
  float *cost;
  BMLoop **loops_prev;
  int i = 0, totloop;
  BMFace *f;

  /* NOTE: would pass BM_EDGE except we are looping over all faces anyway. */
  // BM_mesh_elem_index_ensure(bm, BM_LOOP); /* NOTE: not needed for facetag. */

  BM_ITER_MESH (f, &viter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BM_elem_flag_set(l_iter, BM_ELEM_TAG, !filter_fn(l_iter, user_data));
      BM_elem_index_set(l_iter, i); /* set_inline */
      i += 1;
    } while ((l_iter = l_iter->next) != l_first);
  }
  bm->elem_index_dirty &= ~BM_LOOP;

  /* Allocate. */
  totloop = bm->totloop;
  loops_prev = static_cast<BMLoop **>(MEM_callocN(sizeof(*loops_prev) * totloop, __func__));
  cost = static_cast<float *>(MEM_mallocN(sizeof(*cost) * totloop, __func__));

  copy_vn_fl(cost, totloop, COST_INIT_MAX);

  /* Regular dijkstra shortest path, but over UV loops instead of vertices. */
  heap = BLI_heapsimple_new();
  BLI_heapsimple_insert(heap, 0.0f, l_src);
  cost[BM_elem_index_get(l_src)] = 0.0f;

  BMLoop *l = nullptr;
  while (!BLI_heapsimple_is_empty(heap)) {
    l = static_cast<BMLoop *>(BLI_heapsimple_pop_min(heap));

    if ((l->v == l_dst->v) && BM_loop_uv_share_vert_check(l, l_dst, params->cd_loop_uv_offset)) {
      break;
    }

    if (!BM_elem_flag_test(l, BM_ELEM_TAG)) {
      /* Adjacent loops are tagged while stepping to avoid 2x loops. */
      BM_elem_flag_enable(l, BM_ELEM_TAG);
      verttag_add_adjacent_uv(heap, l, loops_prev, cost, params);
    }
  }

  if ((l->v == l_dst->v) && BM_loop_uv_share_vert_check(l, l_dst, params->cd_loop_uv_offset)) {
    do {
      BLI_linklist_prepend(&path, l);
    } while ((l = loops_prev[BM_elem_index_get(l)]));
  }

  MEM_freeN(loops_prev);
  MEM_freeN(cost);
  BLI_heapsimple_free(heap, nullptr);

  return path;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BM_mesh_calc_path_uv_edge
 * \{ */

static float edgetag_cut_cost_vert_uv(
    BMLoop *l_e_a, BMLoop *l_e_b, BMLoop *l_v, const float aspect_y, const int cd_loop_uv_offset)
{
  BMLoop *l_v1 = (l_v->v == l_e_a->v) ? l_e_a->next : l_e_a;
  BMLoop *l_v2 = (l_v->v == l_e_b->v) ? l_e_b->next : l_e_b;

  float *luv_v1 = BM_ELEM_CD_GET_FLOAT_P(l_v1, cd_loop_uv_offset);
  float *luv_v2 = BM_ELEM_CD_GET_FLOAT_P(l_v2, cd_loop_uv_offset);
  float *luv_v = BM_ELEM_CD_GET_FLOAT_P(l_v, cd_loop_uv_offset);

  float uv_v1[2] = {luv_v1[0], luv_v1[1] / aspect_y};
  float uv_v2[2] = {luv_v2[0], luv_v2[1] / aspect_y};
  float uv_v[2] = {luv_v[0], luv_v[1] / aspect_y};

  return step_cost_3_v2(uv_v1, uv_v, uv_v2);
}

static float edgetag_cut_cost_face_uv(
    BMLoop *l_e_a, BMLoop *l_e_b, BMFace *f, const float aspect_v2[2], const int cd_loop_uv_offset)
{
  float l_e_a_cent[2], l_e_b_cent[2], f_cent[2];
  float *luv_e_a = BM_ELEM_CD_GET_FLOAT_P(l_e_a, cd_loop_uv_offset);
  float *luv_e_b = BM_ELEM_CD_GET_FLOAT_P(l_e_b, cd_loop_uv_offset);

  mid_v2_v2v2(l_e_a_cent, luv_e_a, luv_e_a);
  mid_v2_v2v2(l_e_b_cent, luv_e_b, luv_e_b);

  mul_v2_v2(l_e_a_cent, aspect_v2);
  mul_v2_v2(l_e_b_cent, aspect_v2);

  BM_face_uv_calc_center_median_weighted(f, aspect_v2, cd_loop_uv_offset, f_cent);

  return step_cost_3_v2(l_e_a_cent, l_e_b_cent, f_cent);
}

static void edgetag_add_adjacent_uv(HeapSimple *heap,
                                    BMLoop *l_a,
                                    BMLoop **loops_prev,
                                    float *cost,
                                    const BMCalcPathUVParams *params)
{
  BLI_assert(params->aspect_y != 0.0f);
  const int cd_loop_uv_offset = params->cd_loop_uv_offset;
  BMLoop *l_a_verts[2] = {l_a, l_a->next};
  const int l_a_index = BM_elem_index_get(l_a);

  if (params->use_step_face == false) {
    for (int i = 0; i < ARRAY_SIZE(l_a_verts); i++) {

      /* Skip current UV vert if it is part of the previous UV edge in the path. */
      if (loops_prev[l_a_index]) {
        BMLoop *l_prev = loops_prev[l_a_index];
        if (l_a_verts[i]->v != l_prev->v) {
          l_prev = (l_a_verts[i]->v == l_prev->next->v) ? l_prev->next : nullptr;
        }
        if (l_prev && BM_loop_uv_share_vert_check(l_a_verts[i], l_prev, cd_loop_uv_offset)) {
          continue;
        }
      }

      BMEdge *e_b;
      BMIter eiter;
      BM_ITER_ELEM (e_b, &eiter, l_a_verts[i]->v, BM_EDGES_OF_VERT) {
        BMLoop *l_first, *l_b;
        l_first = l_b = e_b->l;
        do {
          if (!BM_elem_flag_test(l_b, BM_ELEM_TAG)) {
            BMLoop *l_b_vert = (l_a_verts[i]->v == l_b->v) ? l_b : l_b->next;
            if (BM_loop_uv_share_vert_check(l_a_verts[i], l_b_vert, cd_loop_uv_offset)) {
              /* We know 'l_b' is not visited, check it out! */
              const int l_b_index = BM_elem_index_get(l_b);
              const float cost_cut = params->use_topology_distance ?
                                         1.0f :
                                         edgetag_cut_cost_vert_uv(l_a,
                                                                  l_b,
                                                                  l_a_verts[i],
                                                                  params->aspect_y,
                                                                  cd_loop_uv_offset);
              const float cost_new = cost[l_a_index] + cost_cut;

              if (cost[l_b_index] > cost_new) {
                cost[l_b_index] = cost_new;
                loops_prev[l_b_index] = l_a;
                BLI_heapsimple_insert(heap, cost_new, l_b);
              }
            }
          }
        } while ((l_b = l_b->radial_next) != l_first);
      }
    }
  }
  else {
    const float aspect_v2[2] = {1.0f, 1.0f / params->aspect_y};
    BMLoop *l_first, *l_iter;
    l_iter = l_first = l_a;
    do {
      /* Ensures connected UVs and that they lie on the same island. */
      if (!BM_loop_uv_share_edge_check(l_a, l_iter, cd_loop_uv_offset)) {
        continue;
      }

      BMLoop *l_cycle_iter, *l_cycle_end;
      l_cycle_iter = l_iter->next;
      l_cycle_end = l_iter;
      do {
        BMLoop *l_b = l_cycle_iter;
        if (!BM_elem_flag_test(l_b, BM_ELEM_TAG)) {
          /* We know 'l_b' is not visited, check it out! */
          const int l_b_index = BM_elem_index_get(l_b);
          const float cost_cut = params->use_topology_distance ?
                                     1.0f :
                                     edgetag_cut_cost_face_uv(l_a,
                                                              l_b,
                                                              l_iter->f,
                                                              aspect_v2,
                                                              params->cd_loop_uv_offset);
          const float cost_new = cost[l_a_index] + cost_cut;

          if (cost[l_b_index] > cost_new) {
            cost[l_b_index] = cost_new;
            loops_prev[l_b_index] = l_a;
            BLI_heapsimple_insert(heap, cost_new, l_b);
          }
        }
      } while ((l_cycle_iter = l_cycle_iter->next) != l_cycle_end);
    } while ((l_iter = l_iter->radial_next) != l_first);
  }
}

LinkNode *BM_mesh_calc_path_uv_edge(BMesh *bm,
                                    BMLoop *l_src,
                                    BMLoop *l_dst,
                                    const BMCalcPathUVParams *params,
                                    bool (*filter_fn)(BMLoop *, void *),
                                    void *user_data)
{
  LinkNode *path = nullptr;

  BMFace *f;
  BMIter iter;
  HeapSimple *heap;
  float *cost;
  BMLoop **loops_prev;
  int i = 0, totloop;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BM_elem_flag_set(l_iter, BM_ELEM_TAG, !filter_fn(l_iter, user_data));
      BM_elem_index_set(l_iter, i);
      i += 1;
    } while ((l_iter = l_iter->next) != l_first);
  }
  bm->elem_index_dirty &= ~BM_LOOP;

  totloop = bm->totloop;
  loops_prev = static_cast<BMLoop **>(MEM_callocN(sizeof(*loops_prev) * totloop, __func__));
  cost = static_cast<float *>(MEM_mallocN(sizeof(*cost) * totloop, __func__));

  copy_vn_fl(cost, totloop, COST_INIT_MAX);

  /* Regular dijkstra shortest path, but over UV loops/edges instead of vertices. */
  heap = BLI_heapsimple_new();
  BLI_heapsimple_insert(heap, 0.0f, l_src);
  cost[BM_elem_index_get(l_src)] = 0.0f;

  BMLoop *l = nullptr;
  while (!BLI_heapsimple_is_empty(heap)) {
    l = static_cast<BMLoop *>(BLI_heapsimple_pop_min(heap));

    if ((l->e == l_dst->e) && BM_loop_uv_share_edge_check(l, l_dst, params->cd_loop_uv_offset)) {
      break;
    }

    if (!BM_elem_flag_test(l, BM_ELEM_TAG)) {
      BM_elem_flag_enable(l, BM_ELEM_TAG);
      edgetag_add_adjacent_uv(heap, l, loops_prev, cost, params);
    }
  }

  if ((l->e == l_dst->e) && BM_loop_uv_share_edge_check(l, l_dst, params->cd_loop_uv_offset)) {
    do {
      BLI_linklist_prepend(&path, l);
    } while ((l = loops_prev[BM_elem_index_get(l)]));
  }

  MEM_freeN(loops_prev);
  MEM_freeN(cost);
  BLI_heapsimple_free(heap, nullptr);

  return path;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BM_mesh_calc_path_uv_face
 * \{ */

static float facetag_cut_cost_edge_uv(BMFace *f_a,
                                      BMFace *f_b,
                                      BMLoop *l_edge,
                                      const void *const f_endpoints[2],
                                      const float aspect_v2[2],
                                      const int cd_loop_uv_offset)
{
  float f_a_cent[2];
  float f_b_cent[2];
  float e_cent[2];

  BM_face_uv_calc_center_median_weighted(f_a, aspect_v2, cd_loop_uv_offset, f_a_cent);
  BM_face_uv_calc_center_median_weighted(f_b, aspect_v2, cd_loop_uv_offset, f_b_cent);

  const float *co_v1 = BM_ELEM_CD_GET_FLOAT_P(l_edge, cd_loop_uv_offset);
  const float *co_v2 = BM_ELEM_CD_GET_FLOAT_P(l_edge->next, cd_loop_uv_offset);

#if 0
  mid_v2_v2v2(e_cent, co_v1, co_v2);
#else
  /* For triangle fans it gives better results to pick a point on the edge. */
  {
    float ix_e[2];
    isect_line_line_v2_point(co_v1, co_v2, f_a_cent, f_b_cent, ix_e);
    const float factor = line_point_factor_v2(ix_e, co_v1, co_v2);
    if (factor < 0.0f) {
      copy_v2_v2(e_cent, co_v1);
    }
    else if (factor > 1.0f) {
      copy_v2_v2(e_cent, co_v2);
    }
    else {
      copy_v2_v2(e_cent, ix_e);
    }
  }
#endif

  /* Apply aspect before calculating cost. */
  mul_v2_v2(f_a_cent, aspect_v2);
  mul_v2_v2(f_b_cent, aspect_v2);
  mul_v2_v2(e_cent, aspect_v2);

  return step_cost_3_v2_ex(
      f_a_cent, e_cent, f_b_cent, (f_a == f_endpoints[0]), (f_b == f_endpoints[1]));
}

static float facetag_cut_cost_vert_uv(BMFace *f_a,
                                      BMFace *f_b,
                                      BMLoop *l_vert,
                                      const void *const f_endpoints[2],
                                      const float aspect_v2[2],
                                      const int cd_loop_uv_offset)
{
  float f_a_cent[2];
  float f_b_cent[2];
  float v_cent[2];

  BM_face_uv_calc_center_median_weighted(f_a, aspect_v2, cd_loop_uv_offset, f_a_cent);
  BM_face_uv_calc_center_median_weighted(f_b, aspect_v2, cd_loop_uv_offset, f_b_cent);

  copy_v2_v2(v_cent, BM_ELEM_CD_GET_FLOAT_P(l_vert, cd_loop_uv_offset));

  mul_v2_v2(f_a_cent, aspect_v2);
  mul_v2_v2(f_b_cent, aspect_v2);
  mul_v2_v2(v_cent, aspect_v2);

  return step_cost_3_v2_ex(
      f_a_cent, v_cent, f_b_cent, (f_a == f_endpoints[0]), (f_b == f_endpoints[1]));
}

static void facetag_add_adjacent_uv(HeapSimple *heap,
                                    BMFace *f_a,
                                    BMFace **faces_prev,
                                    float *cost,
                                    const void *const f_endpoints[2],
                                    const float aspect_v2[2],
                                    const BMCalcPathUVParams *params)
{
  const int cd_loop_uv_offset = params->cd_loop_uv_offset;
  const int f_a_index = BM_elem_index_get(f_a);

  /* Loop over faces of face, but do so by first looping over loops. */
  {
    BMIter liter;
    BMLoop *l_a;

    BM_ITER_ELEM (l_a, &liter, f_a, BM_LOOPS_OF_FACE) {
      BMLoop *l_first, *l_iter;

      /* Check there is an adjacent face to loop over. */
      if (l_a != l_a->radial_next) {
        l_iter = l_first = l_a->radial_next;
        do {
          BMFace *f_b = l_iter->f;
          if (!BM_elem_flag_test(f_b, BM_ELEM_TAG)) {
            if (BM_loop_uv_share_edge_check(l_a, l_iter, cd_loop_uv_offset)) {
              /* We know 'f_b' is not visited, check it out! */
              const int f_b_index = BM_elem_index_get(f_b);
              const float cost_cut =
                  params->use_topology_distance ?
                      1.0f :
                      facetag_cut_cost_edge_uv(
                          f_a, f_b, l_iter, f_endpoints, aspect_v2, cd_loop_uv_offset);
              const float cost_new = cost[f_a_index] + cost_cut;

              if (cost[f_b_index] > cost_new) {
                cost[f_b_index] = cost_new;
                faces_prev[f_b_index] = f_a;
                BLI_heapsimple_insert(heap, cost_new, f_b);
              }
            }
          }
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
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
            if (BM_loop_uv_share_vert_check(l_a, l_b, cd_loop_uv_offset)) {
              /* We know 'f_b' is not visited, check it out! */
              const int f_b_index = BM_elem_index_get(f_b);
              const float cost_cut =
                  params->use_topology_distance ?
                      1.0f :
                      facetag_cut_cost_vert_uv(
                          f_a, f_b, l_a, f_endpoints, aspect_v2, cd_loop_uv_offset);
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
}

LinkNode *BM_mesh_calc_path_uv_face(BMesh *bm,
                                    BMFace *f_src,
                                    BMFace *f_dst,
                                    const BMCalcPathUVParams *params,
                                    bool (*filter_fn)(BMFace *, void *),
                                    void *user_data)
{
  const float aspect_v2[2] = {1.0f, 1.0f / params->aspect_y};
  LinkNode *path = nullptr;
  /* BM_ELEM_TAG flag is used to store visited edges */
  BMIter fiter;
  HeapSimple *heap;
  float *cost;
  BMFace **faces_prev;
  int i = 0, totface;

  /* Start measuring face path at the face edges, ignoring their centers. */
  const void *const f_endpoints[2] = {f_src, f_dst};

  /* NOTE: would pass BM_EDGE except we are looping over all faces anyway. */
  // BM_mesh_elem_index_ensure(bm, BM_LOOP); /* NOTE: not needed for facetag. */

  {
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_elem_flag_set(f, BM_ELEM_TAG, !filter_fn(f, user_data));
      BM_elem_index_set(f, i); /* set_inline */
      i += 1;
    }
    bm->elem_index_dirty &= ~BM_FACE;
  }

  /* Allocate. */
  totface = bm->totface;
  faces_prev = static_cast<BMFace **>(MEM_callocN(sizeof(*faces_prev) * totface, __func__));
  cost = static_cast<float *>(MEM_mallocN(sizeof(*cost) * totface, __func__));

  copy_vn_fl(cost, totface, COST_INIT_MAX);

  /* Regular dijkstra shortest path, but over UV faces instead of vertices. */
  heap = BLI_heapsimple_new();
  BLI_heapsimple_insert(heap, 0.0f, f_src);
  cost[BM_elem_index_get(f_src)] = 0.0f;

  BMFace *f = nullptr;
  while (!BLI_heapsimple_is_empty(heap)) {
    f = static_cast<BMFace *>(BLI_heapsimple_pop_min(heap));

    if (f == f_dst) {
      break;
    }

    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      /* Adjacent loops are tagged while stepping to avoid 2x loops. */
      BM_elem_flag_enable(f, BM_ELEM_TAG);
      facetag_add_adjacent_uv(heap, f, faces_prev, cost, f_endpoints, aspect_v2, params);
    }
  }

  if (f == f_dst) {
    do {
      BLI_linklist_prepend(&path, f);
    } while ((f = faces_prev[BM_elem_index_get(f)]));
  }

  MEM_freeN(faces_prev);
  MEM_freeN(cost);
  BLI_heapsimple_free(heap, nullptr);

  return path;
}

/** \} */
