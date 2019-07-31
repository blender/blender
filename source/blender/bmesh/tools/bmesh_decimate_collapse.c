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
 */

/** \file
 * \ingroup bmesh
 *
 * BMesh decimator that uses an edge collapse method.
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_quadric.h"
#include "BLI_heap.h"
#include "BLI_linklist.h"
#include "BLI_alloca.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_utildefines_stack.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "bmesh_decimate.h" /* own include */

#include "../intern/bmesh_structure.h"

#define USE_SYMMETRY
#ifdef USE_SYMMETRY
#  include "BLI_kdtree.h"
#endif

/* defines for testing */
#define USE_CUSTOMDATA
#define USE_TRIANGULATE
/** Has the advantage that flipped faces don't mess up vertex normals. */
#define USE_VERT_NORMAL_INTERP

/** if the cost from #BLI_quadric_evaluate is 'noise', fallback to topology */
#define USE_TOPOLOGY_FALLBACK
#ifdef USE_TOPOLOGY_FALLBACK
/** cost is calculated with double precision, it's ok to use a very small epsilon, see T48154. */
#  define TOPOLOGY_FALLBACK_EPS 1e-12f
#endif

#define BOUNDARY_PRESERVE_WEIGHT 100.0f
/** Uses double precision, impacts behavior on near-flat surfaces,
 * cane give issues with very small faces. 1e-2 is too big, see: T48154. */
#define OPTIMIZE_EPS 1e-8
#define COST_INVALID FLT_MAX

typedef enum CD_UseFlag {
  CD_DO_VERT = (1 << 0),
  CD_DO_EDGE = (1 << 1),
  CD_DO_LOOP = (1 << 2),
} CD_UseFlag;

/* BMesh Helper Functions
 * ********************** */

/**
 * \param vquadrics: must be calloc'd
 */
static void bm_decim_build_quadrics(BMesh *bm, Quadric *vquadrics)
{
  BMIter iter;
  BMFace *f;
  BMEdge *e;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_first;
    BMLoop *l_iter;

    float center[3];
    double plane_db[4];
    Quadric q;

    BM_face_calc_center_median(f, center);
    copy_v3db_v3fl(plane_db, f->no);
    plane_db[3] = -dot_v3db_v3fl(plane_db, center);

    BLI_quadric_from_plane(&q, plane_db);

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(l_iter->v)], &q);
    } while ((l_iter = l_iter->next) != l_first);
  }

  /* boundary edges */
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (UNLIKELY(BM_edge_is_boundary(e))) {
      float edge_vector[3];
      float edge_plane[3];
      double edge_plane_db[4];
      sub_v3_v3v3(edge_vector, e->v2->co, e->v1->co);
      f = e->l->f;

      cross_v3_v3v3(edge_plane, edge_vector, f->no);
      copy_v3db_v3fl(edge_plane_db, edge_plane);

      if (normalize_v3_d(edge_plane_db) > (double)FLT_EPSILON) {
        Quadric q;
        float center[3];

        mid_v3_v3v3(center, e->v1->co, e->v2->co);

        edge_plane_db[3] = -dot_v3db_v3fl(edge_plane_db, center);
        BLI_quadric_from_plane(&q, edge_plane_db);
        BLI_quadric_mul(&q, BOUNDARY_PRESERVE_WEIGHT);

        BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(e->v1)], &q);
        BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(e->v2)], &q);
      }
    }
  }
}

static void bm_decim_calc_target_co_db(BMEdge *e, double optimize_co[3], const Quadric *vquadrics)
{
  /* compute an edge contraction target for edge 'e'
   * this is computed by summing it's vertices quadrics and
   * optimizing the result. */
  Quadric q;

  BLI_quadric_add_qu_ququ(
      &q, &vquadrics[BM_elem_index_get(e->v1)], &vquadrics[BM_elem_index_get(e->v2)]);

  if (BLI_quadric_optimize(&q, optimize_co, OPTIMIZE_EPS)) {
    /* all is good */
    return;
  }
  else {
    optimize_co[0] = 0.5 * ((double)e->v1->co[0] + (double)e->v2->co[0]);
    optimize_co[1] = 0.5 * ((double)e->v1->co[1] + (double)e->v2->co[1]);
    optimize_co[2] = 0.5 * ((double)e->v1->co[2] + (double)e->v2->co[2]);
  }
}

static void bm_decim_calc_target_co_fl(BMEdge *e, float optimize_co[3], const Quadric *vquadrics)
{
  double optimize_co_db[3];
  bm_decim_calc_target_co_db(e, optimize_co_db, vquadrics);
  copy_v3fl_v3db(optimize_co, optimize_co_db);
}

static bool bm_edge_collapse_is_degenerate_flip(BMEdge *e, const float optimize_co[3])
{
  BMIter liter;
  BMLoop *l;
  uint i;

  for (i = 0; i < 2; i++) {
    /* loop over both verts */
    BMVert *v = *((&e->v1) + i);

    BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
      if (l->e != e && l->prev->e != e) {
        const float *co_prev = l->prev->v->co;
        const float *co_next = l->next->v->co;
        float cross_exist[3];
        float cross_optim[3];

#if 1
        /* line between the two outer verts, re-use for both cross products */
        float vec_other[3];
        /* before collapse */
        float vec_exist[3];
        /* after collapse */
        float vec_optim[3];

        sub_v3_v3v3(vec_other, co_prev, co_next);
        sub_v3_v3v3(vec_exist, co_prev, v->co);
        sub_v3_v3v3(vec_optim, co_prev, optimize_co);

        cross_v3_v3v3(cross_exist, vec_other, vec_exist);
        cross_v3_v3v3(cross_optim, vec_other, vec_optim);

        /* avoid normalize */
        if (dot_v3v3(cross_exist, cross_optim) <=
            (len_squared_v3(cross_exist) + len_squared_v3(cross_optim)) * 0.01f) {
          return true;
        }
#else
        normal_tri_v3(cross_exist, v->co, co_prev, co_next);
        normal_tri_v3(cross_optim, optimize_co, co_prev, co_next);

        /* use a small value rather then zero so we don't flip a face in multiple steps
         * (first making it zero area, then flipping again) */
        if (dot_v3v3(cross_exist, cross_optim) <= FLT_EPSILON) {
          // printf("no flip\n");
          return true;
        }
#endif
      }
    }
  }

  return false;
}

#ifdef USE_TOPOLOGY_FALLBACK
/**
 * when the cost is so small that its not useful (flat surfaces),
 * fallback to using a 'topology' cost.
 *
 * This avoids cases where a flat (or near flat) areas get very un-even geometry.
 */
static float bm_decim_build_edge_cost_single_squared__topology(BMEdge *e)
{
  return fabsf(dot_v3v3(e->v1->no, e->v2->no)) /
         min_ff(-len_squared_v3v3(e->v1->co, e->v2->co), -FLT_EPSILON);
}
static float bm_decim_build_edge_cost_single__topology(BMEdge *e)
{
  return fabsf(dot_v3v3(e->v1->no, e->v2->no)) /
         min_ff(-len_v3v3(e->v1->co, e->v2->co), -FLT_EPSILON);
}

#endif /* USE_TOPOLOGY_FALLBACK */

static void bm_decim_build_edge_cost_single(BMEdge *e,
                                            const Quadric *vquadrics,
                                            const float *vweights,
                                            const float vweight_factor,
                                            Heap *eheap,
                                            HeapNode **eheap_table)
{
  float cost;

  if (UNLIKELY(vweights && ((vweights[BM_elem_index_get(e->v1)] == 0.0f) ||
                            (vweights[BM_elem_index_get(e->v2)] == 0.0f)))) {
    goto clear;
  }

  /* check we can collapse, some edges we better not touch */
  if (BM_edge_is_boundary(e)) {
    if (e->l->f->len == 3) {
      /* pass */
    }
    else {
      /* only collapse tri's */
      goto clear;
    }
  }
  else if (BM_edge_is_manifold(e)) {
    if ((e->l->f->len == 3) && (e->l->radial_next->f->len == 3)) {
      /* pass */
    }
    else {
      /* only collapse tri's */
      goto clear;
    }
  }
  else {
    goto clear;
  }
  /* end sanity check */

  {
    double optimize_co[3];
    bm_decim_calc_target_co_db(e, optimize_co, vquadrics);

    const Quadric *q1, *q2;
    q1 = &vquadrics[BM_elem_index_get(e->v1)];
    q2 = &vquadrics[BM_elem_index_get(e->v2)];

    cost = (BLI_quadric_evaluate(q1, optimize_co) + BLI_quadric_evaluate(q2, optimize_co));
  }

  /* note, 'cost' shouldn't be negative but happens sometimes with small values.
   * this can cause faces that make up a flat surface to over-collapse, see [#37121] */
  cost = fabsf(cost);

#ifdef USE_TOPOLOGY_FALLBACK
  if (UNLIKELY(cost < TOPOLOGY_FALLBACK_EPS)) {
    /* subtract existing cost to further differentiate edges from one another
     *
     * keep topology cost below 0.0 so their values don't interfere with quadric cost,
     * (and they get handled first).
     * */
    if (vweights == NULL) {
      cost = bm_decim_build_edge_cost_single_squared__topology(e) - cost;
    }
    else {
      /* with weights we need to use the real length so we can scale them properly */
      const float e_weight = (vweights[BM_elem_index_get(e->v1)] +
                              vweights[BM_elem_index_get(e->v2)]);
      cost = bm_decim_build_edge_cost_single__topology(e) - cost;
      /* note, this is rather arbitrary max weight is 2 here,
       * allow for skipping edges 4x the length, based on weights */
      if (e_weight) {
        cost *= 1.0f + (e_weight * vweight_factor);
      }

      BLI_assert(cost <= 0.0f);
    }
  }
  else
#endif
      if (vweights) {
    const float e_weight = 2.0f - (vweights[BM_elem_index_get(e->v1)] +
                                   vweights[BM_elem_index_get(e->v2)]);
    if (e_weight) {
      cost += (BM_edge_calc_length(e) * ((e_weight * vweight_factor)));
    }
  }

  BLI_heap_insert_or_update(eheap, &eheap_table[BM_elem_index_get(e)], cost, e);
  return;

clear:
  if (eheap_table[BM_elem_index_get(e)]) {
    BLI_heap_remove(eheap, eheap_table[BM_elem_index_get(e)]);
  }
  eheap_table[BM_elem_index_get(e)] = NULL;
}

/* use this for degenerate cases - add back to the heap with an invalid cost,
 * this way it may be calculated again if surrounding geometry changes */
static void bm_decim_invalid_edge_cost_single(BMEdge *e, Heap *eheap, HeapNode **eheap_table)
{
  BLI_assert(eheap_table[BM_elem_index_get(e)] == NULL);
  eheap_table[BM_elem_index_get(e)] = BLI_heap_insert(eheap, COST_INVALID, e);
}

static void bm_decim_build_edge_cost(BMesh *bm,
                                     const Quadric *vquadrics,
                                     const float *vweights,
                                     const float vweight_factor,
                                     Heap *eheap,
                                     HeapNode **eheap_table)
{
  BMIter iter;
  BMEdge *e;
  uint i;

  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    /* keep sanity check happy */
    eheap_table[i] = NULL;
    bm_decim_build_edge_cost_single(e, vquadrics, vweights, vweight_factor, eheap, eheap_table);
  }
}

#ifdef USE_SYMMETRY

struct KD_Symmetry_Data {
  /* pre-flipped coords */
  float e_v1_co[3], e_v2_co[3];
  /* Use to compare the correct endpoints incase v1/v2 are swapped */
  float e_dir[3];

  int e_found_index;

  /* same for all */
  BMEdge **etable;
  float limit_sq;
};

static bool bm_edge_symmetry_check_cb(void *user_data,
                                      int index,
                                      const float UNUSED(co[3]),
                                      float UNUSED(dist_sq))
{
  struct KD_Symmetry_Data *sym_data = user_data;
  BMEdge *e_other = sym_data->etable[index];
  float e_other_dir[3];

  sub_v3_v3v3(e_other_dir, e_other->v2->co, e_other->v1->co);

  if (dot_v3v3(e_other_dir, sym_data->e_dir) > 0.0f) {
    if ((len_squared_v3v3(sym_data->e_v1_co, e_other->v1->co) > sym_data->limit_sq) ||
        (len_squared_v3v3(sym_data->e_v2_co, e_other->v2->co) > sym_data->limit_sq)) {
      return true;
    }
  }
  else {
    if ((len_squared_v3v3(sym_data->e_v1_co, e_other->v2->co) > sym_data->limit_sq) ||
        (len_squared_v3v3(sym_data->e_v2_co, e_other->v1->co) > sym_data->limit_sq)) {
      return true;
    }
  }

  /* exit on first-hit, this is OK since the search range is very small */
  sym_data->e_found_index = index;
  return false;
}

static int *bm_edge_symmetry_map(BMesh *bm, uint symmetry_axis, float limit)
{
  struct KD_Symmetry_Data sym_data;
  BMIter iter;
  BMEdge *e, **etable;
  uint i;
  int *edge_symmetry_map;
  const float limit_sq = SQUARE(limit);
  KDTree_3d *tree;

  tree = BLI_kdtree_3d_new(bm->totedge);

  etable = MEM_mallocN(sizeof(*etable) * bm->totedge, __func__);
  edge_symmetry_map = MEM_mallocN(sizeof(*edge_symmetry_map) * bm->totedge, __func__);

  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    float co[3];
    mid_v3_v3v3(co, e->v1->co, e->v2->co);
    BLI_kdtree_3d_insert(tree, i, co);
    etable[i] = e;
    edge_symmetry_map[i] = -1;
  }

  BLI_kdtree_3d_balance(tree);

  sym_data.etable = etable;
  sym_data.limit_sq = limit_sq;

  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    if (edge_symmetry_map[i] == -1) {
      float co[3];
      mid_v3_v3v3(co, e->v1->co, e->v2->co);
      co[symmetry_axis] *= -1.0f;

      copy_v3_v3(sym_data.e_v1_co, e->v1->co);
      copy_v3_v3(sym_data.e_v2_co, e->v2->co);
      sym_data.e_v1_co[symmetry_axis] *= -1.0f;
      sym_data.e_v2_co[symmetry_axis] *= -1.0f;
      sub_v3_v3v3(sym_data.e_dir, sym_data.e_v2_co, sym_data.e_v1_co);
      sym_data.e_found_index = -1;

      BLI_kdtree_3d_range_search_cb(tree, co, limit, bm_edge_symmetry_check_cb, &sym_data);

      if (sym_data.e_found_index != -1) {
        const int i_other = sym_data.e_found_index;
        edge_symmetry_map[i] = i_other;
        edge_symmetry_map[i_other] = i;
      }
    }
  }

  MEM_freeN(etable);
  BLI_kdtree_3d_free(tree);

  return edge_symmetry_map;
}
#endif /* USE_SYMMETRY */

#ifdef USE_TRIANGULATE
/* Temp Triangulation
 * ****************** */

/**
 * To keep things simple we can only collapse edges on triangulated data
 * (limitation with edge collapse and error calculation functions).
 *
 * But to avoid annoying users by only giving triangle results, we can
 * triangulate, keeping a reference between the faces, then join after
 * if the edges don't collapse, this will also allow more choices when
 * collapsing edges so even has some advantage over decimating quads
 * directly.
 *
 * \return true if any faces were triangulated.
 */
static bool bm_face_triangulate(BMesh *bm,
                                BMFace *f_base,
                                LinkNode **r_faces_double,
                                int *r_edges_tri_tot,

                                MemArena *pf_arena,
                                /* use for MOD_TRIANGULATE_NGON_BEAUTY only! */
                                struct Heap *pf_heap)
{
  const int f_base_len = f_base->len;
  int faces_array_tot = f_base_len - 3;
  int edges_array_tot = f_base_len - 3;
  BMFace **faces_array = BLI_array_alloca(faces_array, faces_array_tot);
  BMEdge **edges_array = BLI_array_alloca(edges_array, edges_array_tot);
  const int quad_method = 0, ngon_method = 0; /* beauty */

  bool has_cut = false;

  const int f_index = BM_elem_index_get(f_base);

  BM_face_triangulate(bm,
                      f_base,
                      faces_array,
                      &faces_array_tot,
                      edges_array,
                      &edges_array_tot,
                      r_faces_double,
                      quad_method,
                      ngon_method,
                      false,
                      pf_arena,
                      pf_heap);

  for (int i = 0; i < edges_array_tot; i++) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = edges_array[i]->l;
    do {
      BM_elem_index_set(l_iter, f_index); /* set_dirty */
      has_cut = true;
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  for (int i = 0; i < faces_array_tot; i++) {
    BM_face_normal_update(faces_array[i]);
  }

  *r_edges_tri_tot += edges_array_tot;

  return has_cut;
}

static bool bm_decim_triangulate_begin(BMesh *bm, int *r_edges_tri_tot)
{
  BMIter iter;
  BMFace *f;
  bool has_quad = false;
  bool has_ngon = false;
  bool has_cut = false;

  BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

  /* first clear loop index values */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter;
    BMLoop *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_elem_index_set(l_iter, -1); /* set_dirty */
    } while ((l_iter = l_iter->next) != l_first);

    has_quad |= (f->len > 3);
    has_ngon |= (f->len > 4);
  }

  bm->elem_index_dirty |= BM_LOOP;

  {
    MemArena *pf_arena;
    Heap *pf_heap;

    LinkNode *faces_double = NULL;

    if (has_ngon) {
      pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
      pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
    }
    else {
      pf_arena = NULL;
      pf_heap = NULL;
    }

    /* adding new faces as we loop over faces
     * is normally best avoided, however in this case its not so bad because any face touched twice
     * will already be triangulated*/
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (f->len > 3) {
        has_cut |= bm_face_triangulate(bm,
                                       f,
                                       &faces_double,
                                       r_edges_tri_tot,

                                       pf_arena,
                                       pf_heap);
      }
    }

    while (faces_double) {
      LinkNode *next = faces_double->next;
      BM_face_kill(bm, faces_double->link);
      MEM_freeN(faces_double);
      faces_double = next;
    }

    if (has_ngon) {
      BLI_memarena_free(pf_arena);
      BLI_heap_free(pf_heap, NULL);
    }

    BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

    if (has_cut) {
      /* now triangulation is done we need to correct index values */
      BM_mesh_elem_index_ensure(bm, BM_EDGE | BM_FACE);
    }
  }

  return has_cut;
}

static void bm_decim_triangulate_end(BMesh *bm, const int edges_tri_tot)
{
  /* decimation finished, now re-join */
  BMIter iter;
  BMEdge *e;

  /* we need to collect before merging for ngons since the loops indices will be lost */
  BMEdge **edges_tri = MEM_mallocN(MIN2(edges_tri_tot, bm->totedge) * sizeof(*edges_tri),
                                   __func__);
  STACK_DECLARE(edges_tri);

  STACK_INIT(edges_tri, MIN2(edges_tri_tot, bm->totedge));

  /* boundary edges */
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    BMLoop *l_a, *l_b;
    if (BM_edge_loop_pair(e, &l_a, &l_b)) {
      const int l_a_index = BM_elem_index_get(l_a);
      if (l_a_index != -1) {
        const int l_b_index = BM_elem_index_get(l_b);
        if (l_a_index == l_b_index) {
          if (l_a->v != l_b->v) { /* if this is the case, faces have become flipped */
                                  /* check we are not making a degenerate quad */

#  define CAN_LOOP_MERGE(l) \
    (BM_loop_is_manifold(l) && ((l)->v != (l)->radial_next->v) && \
     (l_a_index == BM_elem_index_get(l)) && (l_a_index == BM_elem_index_get((l)->radial_next)))

            if ((l_a->f->len == 3 && l_b->f->len == 3) && (!CAN_LOOP_MERGE(l_a->next)) &&
                (!CAN_LOOP_MERGE(l_a->prev)) && (!CAN_LOOP_MERGE(l_b->next)) &&
                (!CAN_LOOP_MERGE(l_b->prev))) {
              BMVert *vquad[4] = {
                  e->v1,
                  BM_vert_in_edge(e, l_a->next->v) ? l_a->prev->v : l_a->next->v,
                  e->v2,
                  BM_vert_in_edge(e, l_b->next->v) ? l_b->prev->v : l_b->next->v,
              };

              BLI_assert(ELEM(vquad[0], vquad[1], vquad[2], vquad[3]) == false);
              BLI_assert(ELEM(vquad[1], vquad[0], vquad[2], vquad[3]) == false);
              BLI_assert(ELEM(vquad[2], vquad[1], vquad[0], vquad[3]) == false);
              BLI_assert(ELEM(vquad[3], vquad[1], vquad[2], vquad[0]) == false);

              if (!is_quad_convex_v3(vquad[0]->co, vquad[1]->co, vquad[2]->co, vquad[3]->co)) {
                continue;
              }
            }
#  undef CAN_LOOP_MERGE

            /* highly unlikely to fail, but prevents possible double-ups */
            STACK_PUSH(edges_tri, e);
          }
        }
      }
    }
  }

  for (int i = 0; i < STACK_SIZE(edges_tri); i++) {
    BMLoop *l_a, *l_b;
    e = edges_tri[i];
    if (BM_edge_loop_pair(e, &l_a, &l_b)) {
      BMFace *f_array[2] = {l_a->f, l_b->f};
      BM_faces_join(bm, f_array, 2, false);
      if (e->l == NULL) {
        BM_edge_kill(bm, e);
      }
    }
  }
  MEM_freeN(edges_tri);
}

#endif /* USE_TRIANGULATE */

/* Edge Collapse Functions
 * *********************** */

#ifdef USE_CUSTOMDATA

/**
 * \param l: defines the vert to collapse into.
 */
static void bm_edge_collapse_loop_customdata(
    BMesh *bm, BMLoop *l, BMVert *v_clear, BMVert *v_other, const float customdata_fac)
{
  /* Disable seam check - the seam check would have to be done per layer,
   * its not really that important. */
  //#define USE_SEAM
  /* these don't need to be updated, since they will get removed when the edge collapses */
  BMLoop *l_clear, *l_other;
  const bool is_manifold = BM_edge_is_manifold(l->e);
  int side;

  /* first find the loop of 'v_other' that's attached to the face of 'l' */
  if (l->v == v_clear) {
    l_clear = l;
    l_other = l->next;
  }
  else {
    l_clear = l->next;
    l_other = l;
  }

  BLI_assert(l_clear->v == v_clear);
  BLI_assert(l_other->v == v_other);
  /* quiet warnings for release */
  (void)v_other;

  /* now we have both corners of the face 'l->f' */
  for (side = 0; side < 2; side++) {
#  ifdef USE_SEAM
    bool is_seam = false;
#  endif
    void *src[2];
    BMFace *f_exit = is_manifold ? l->radial_next->f : NULL;
    BMEdge *e_prev = l->e;
    BMLoop *l_first;
    BMLoop *l_iter;
    float w[2];

    if (side == 0) {
      l_iter = l_first = l_clear;
      src[0] = l_clear->head.data;
      src[1] = l_other->head.data;

      w[0] = customdata_fac;
      w[1] = 1.0f - customdata_fac;
    }
    else {
      l_iter = l_first = l_other;
      src[0] = l_other->head.data;
      src[1] = l_clear->head.data;

      w[0] = 1.0f - customdata_fac;
      w[1] = customdata_fac;
    }

    // print_v2("weights", w);

    /* WATCH IT! - should NOT reference (_clear or _other) vars for this while loop */

    /* walk around the fan using 'e_prev' */
    while (((l_iter = BM_vert_step_fan_loop(l_iter, &e_prev)) != l_first) && (l_iter != NULL)) {
      int i;
      /* quit once we hit the opposite face, if we have one */
      if (f_exit && UNLIKELY(f_exit == l_iter->f)) {
        break;
      }

#  ifdef USE_SEAM
      /* break out unless we find a match */
      is_seam = true;
#  endif

      /* ok. we have a loop. now be smart with it! */
      for (i = 0; i < bm->ldata.totlayer; i++) {
        if (CustomData_layer_has_math(&bm->ldata, i)) {
          const int offset = bm->ldata.layers[i].offset;
          const int type = bm->ldata.layers[i].type;
          const void *cd_src[2] = {
              POINTER_OFFSET(src[0], offset),
              POINTER_OFFSET(src[1], offset),
          };
          void *cd_iter = POINTER_OFFSET(l_iter->head.data, offset);

          /* detect seams */
          if (CustomData_data_equals(type, cd_src[0], cd_iter)) {
            CustomData_bmesh_interp_n(&bm->ldata,
                                      cd_src,
                                      w,
                                      NULL,
                                      ARRAY_SIZE(cd_src),
                                      POINTER_OFFSET(l_iter->head.data, offset),
                                      i);
#  ifdef USE_SEAM
            is_seam = false;
#  endif
          }
        }
      }

#  ifdef USE_SEAM
      if (is_seam) {
        break;
      }
#  endif
    }
  }

  //#undef USE_SEAM
}
#endif /* USE_CUSTOMDATA */

/**
 * Check if the collapse will result in a degenerate mesh,
 * that is - duplicate edges or faces.
 *
 * This situation could be checked for when calculating collapse cost
 * however its quite slow and a degenerate collapse could eventuate
 * after the cost is calculated, so instead, check just before collapsing.
 */

static void bm_edge_tag_enable(BMEdge *e)
{
  BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
  BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
  if (e->l) {
    BM_elem_flag_enable(e->l->f, BM_ELEM_TAG);
    if (e->l != e->l->radial_next) {
      BM_elem_flag_enable(e->l->radial_next->f, BM_ELEM_TAG);
    }
  }
}

static void bm_edge_tag_disable(BMEdge *e)
{
  BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
  BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
  if (e->l) {
    BM_elem_flag_disable(e->l->f, BM_ELEM_TAG);
    if (e->l != e->l->radial_next) {
      BM_elem_flag_disable(e->l->radial_next->f, BM_ELEM_TAG);
    }
  }
}

static bool bm_edge_tag_test(BMEdge *e)
{
  /* is the edge or one of its faces tagged? */
  return (BM_elem_flag_test(e->v1, BM_ELEM_TAG) || BM_elem_flag_test(e->v2, BM_ELEM_TAG) ||
          (e->l &&
           (BM_elem_flag_test(e->l->f, BM_ELEM_TAG) ||
            (e->l != e->l->radial_next && BM_elem_flag_test(e->l->radial_next->f, BM_ELEM_TAG)))));
}

/* takes the edges loop */
BLI_INLINE int bm_edge_is_manifold_or_boundary(BMLoop *l)
{
#if 0
  /* less optimized version of check below */
  return (BM_edge_is_manifold(l->e) || BM_edge_is_boundary(l->e);
#else
  /* if the edge is a boundary it points to its self, else this must be a manifold */
  return LIKELY(l) && LIKELY(l->radial_next->radial_next == l);
#endif
}

static bool bm_edge_collapse_is_degenerate_topology(BMEdge *e_first)
{
  /* simply check that there is no overlap between faces and edges of each vert,
   * (excluding the 2 faces attached to 'e' and 'e' its self) */

  BMEdge *e_iter;

  /* clear flags on both disks */
  e_iter = e_first;
  do {
    if (!bm_edge_is_manifold_or_boundary(e_iter->l)) {
      return true;
    }
    bm_edge_tag_disable(e_iter);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v1)) != e_first);

  e_iter = e_first;
  do {
    if (!bm_edge_is_manifold_or_boundary(e_iter->l)) {
      return true;
    }
    bm_edge_tag_disable(e_iter);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v2)) != e_first);

  /* now enable one side... */
  e_iter = e_first;
  do {
    bm_edge_tag_enable(e_iter);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v1)) != e_first);

  /* ... except for the edge we will collapse, we know that's shared,
   * disable this to avoid false positive. We could be smart and never enable these
   * face/edge tags in the first place but easier to do this */
  // bm_edge_tag_disable(e_first);
  /* do inline... */
  {
#if 0
    BMIter iter;
    BMIter liter;
    BMLoop *l;
    BMVert *v;
    BM_ITER_ELEM (l, &liter, e_first, BM_LOOPS_OF_EDGE) {
      BM_elem_flag_disable(l->f, BM_ELEM_TAG);
      BM_ITER_ELEM (v, &iter, l->f, BM_VERTS_OF_FACE) {
        BM_elem_flag_disable(v, BM_ELEM_TAG);
      }
    }
#else
    /* we know each face is a triangle, no looping/iterators needed here */

    BMLoop *l_radial;
    BMLoop *l_face;

    l_radial = e_first->l;
    l_face = l_radial;
    BLI_assert(l_face->f->len == 3);
    BM_elem_flag_disable(l_face->f, BM_ELEM_TAG);
    BM_elem_flag_disable((l_face = l_radial)->v, BM_ELEM_TAG);
    BM_elem_flag_disable((l_face = l_face->next)->v, BM_ELEM_TAG);
    BM_elem_flag_disable((l_face->next)->v, BM_ELEM_TAG);
    l_face = l_radial->radial_next;
    if (l_radial != l_face) {
      BLI_assert(l_face->f->len == 3);
      BM_elem_flag_disable(l_face->f, BM_ELEM_TAG);
      BM_elem_flag_disable((l_face = l_radial->radial_next)->v, BM_ELEM_TAG);
      BM_elem_flag_disable((l_face = l_face->next)->v, BM_ELEM_TAG);
      BM_elem_flag_disable((l_face->next)->v, BM_ELEM_TAG);
    }
#endif
  }

  /* and check for overlap */
  e_iter = e_first;
  do {
    if (bm_edge_tag_test(e_iter)) {
      return true;
    }
  } while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v2)) != e_first);

  return false;
}

/**
 * special, highly limited edge collapse function
 * intended for speed over flexibility.
 * can only collapse edges connected to (1, 2) tris.
 *
 * Important - dont add vert/edge/face data on collapsing!
 *
 * \param r_e_clear_other: Let caller know what edges we remove besides \a e_clear
 * \param customdata_flag: Merge factor, scales from 0 - 1 ('v_clear' -> 'v_other')
 */
static bool bm_edge_collapse(BMesh *bm,
                             BMEdge *e_clear,
                             BMVert *v_clear,
                             int r_e_clear_other[2],
#ifdef USE_SYMMETRY
                             int *edge_symmetry_map,
#endif
#ifdef USE_CUSTOMDATA
                             const CD_UseFlag customdata_flag,
                             const float customdata_fac
#else
                             const CD_UseFlag UNUSED(customdata_flag),
                             const float UNUSED(customdata_fac)
#endif
)
{
  BMVert *v_other;

  v_other = BM_edge_other_vert(e_clear, v_clear);
  BLI_assert(v_other != NULL);

  if (BM_edge_is_manifold(e_clear)) {
    BMLoop *l_a, *l_b;
    BMEdge *e_a_other[2], *e_b_other[2];
    bool ok;

    ok = BM_edge_loop_pair(e_clear, &l_a, &l_b);

    BLI_assert(ok == true);
    BLI_assert(l_a->f->len == 3);
    BLI_assert(l_b->f->len == 3);
    UNUSED_VARS_NDEBUG(ok);

    /* keep 'v_clear' 0th */
    if (BM_vert_in_edge(l_a->prev->e, v_clear)) {
      e_a_other[0] = l_a->prev->e;
      e_a_other[1] = l_a->next->e;
    }
    else {
      e_a_other[1] = l_a->prev->e;
      e_a_other[0] = l_a->next->e;
    }

    if (BM_vert_in_edge(l_b->prev->e, v_clear)) {
      e_b_other[0] = l_b->prev->e;
      e_b_other[1] = l_b->next->e;
    }
    else {
      e_b_other[1] = l_b->prev->e;
      e_b_other[0] = l_b->next->e;
    }

    /* we could assert this case, but better just bail out */
#if 0
    BLI_assert(e_a_other[0] != e_b_other[0]);
    BLI_assert(e_a_other[0] != e_b_other[1]);
    BLI_assert(e_b_other[0] != e_a_other[0]);
    BLI_assert(e_b_other[0] != e_a_other[1]);
#endif
    /* not totally common but we want to avoid */
    if (ELEM(e_a_other[0], e_b_other[0], e_b_other[1]) ||
        ELEM(e_a_other[1], e_b_other[0], e_b_other[1])) {
      return false;
    }

    BLI_assert(BM_edge_share_vert(e_a_other[0], e_b_other[0]));
    BLI_assert(BM_edge_share_vert(e_a_other[1], e_b_other[1]));

    r_e_clear_other[0] = BM_elem_index_get(e_a_other[0]);
    r_e_clear_other[1] = BM_elem_index_get(e_b_other[0]);

#ifdef USE_CUSTOMDATA
    /* before killing, do customdata */
    if (customdata_flag & CD_DO_VERT) {
      BM_data_interp_from_verts(bm, v_other, v_clear, v_other, customdata_fac);
    }
    if (customdata_flag & CD_DO_EDGE) {
      BM_data_interp_from_edges(bm, e_a_other[1], e_a_other[0], e_a_other[1], customdata_fac);
      BM_data_interp_from_edges(bm, e_b_other[1], e_b_other[0], e_b_other[1], customdata_fac);
    }
    if (customdata_flag & CD_DO_LOOP) {
      bm_edge_collapse_loop_customdata(bm, e_clear->l, v_clear, v_other, customdata_fac);
      bm_edge_collapse_loop_customdata(
          bm, e_clear->l->radial_next, v_clear, v_other, customdata_fac);
    }
#endif

    BM_edge_kill(bm, e_clear);

    v_other->head.hflag |= v_clear->head.hflag;
    BM_vert_splice(bm, v_other, v_clear);

    e_a_other[1]->head.hflag |= e_a_other[0]->head.hflag;
    e_b_other[1]->head.hflag |= e_b_other[0]->head.hflag;
    BM_edge_splice(bm, e_a_other[1], e_a_other[0]);
    BM_edge_splice(bm, e_b_other[1], e_b_other[0]);

#ifdef USE_SYMMETRY
    /* update mirror map */
    if (edge_symmetry_map) {
      if (edge_symmetry_map[r_e_clear_other[0]] != -1) {
        edge_symmetry_map[edge_symmetry_map[r_e_clear_other[0]]] = BM_elem_index_get(e_a_other[1]);
      }
      if (edge_symmetry_map[r_e_clear_other[1]] != -1) {
        edge_symmetry_map[edge_symmetry_map[r_e_clear_other[1]]] = BM_elem_index_get(e_b_other[1]);
      }
    }
#endif

    // BM_mesh_validate(bm);

    return true;
  }
  else if (BM_edge_is_boundary(e_clear)) {
    /* same as above but only one triangle */
    BMLoop *l_a;
    BMEdge *e_a_other[2];

    l_a = e_clear->l;

    BLI_assert(l_a->f->len == 3);

    /* keep 'v_clear' 0th */
    if (BM_vert_in_edge(l_a->prev->e, v_clear)) {
      e_a_other[0] = l_a->prev->e;
      e_a_other[1] = l_a->next->e;
    }
    else {
      e_a_other[1] = l_a->prev->e;
      e_a_other[0] = l_a->next->e;
    }

    r_e_clear_other[0] = BM_elem_index_get(e_a_other[0]);
    r_e_clear_other[1] = -1;

#ifdef USE_CUSTOMDATA
    /* before killing, do customdata */
    if (customdata_flag & CD_DO_VERT) {
      BM_data_interp_from_verts(bm, v_other, v_clear, v_other, customdata_fac);
    }
    if (customdata_flag & CD_DO_EDGE) {
      BM_data_interp_from_edges(bm, e_a_other[1], e_a_other[0], e_a_other[1], customdata_fac);
    }
    if (customdata_flag & CD_DO_LOOP) {
      bm_edge_collapse_loop_customdata(bm, e_clear->l, v_clear, v_other, customdata_fac);
    }
#endif

    BM_edge_kill(bm, e_clear);

    v_other->head.hflag |= v_clear->head.hflag;
    BM_vert_splice(bm, v_other, v_clear);

    e_a_other[1]->head.hflag |= e_a_other[0]->head.hflag;
    BM_edge_splice(bm, e_a_other[1], e_a_other[0]);

#ifdef USE_SYMMETRY
    /* update mirror map */
    if (edge_symmetry_map) {
      if (edge_symmetry_map[r_e_clear_other[0]] != -1) {
        edge_symmetry_map[edge_symmetry_map[r_e_clear_other[0]]] = BM_elem_index_get(e_a_other[1]);
      }
    }
#endif

    // BM_mesh_validate(bm);

    return true;
  }
  else {
    return false;
  }
}

/**
 * Collapse e the edge, removing e->v2
 *
 * \return true when the edge was collapsed.
 */
static bool bm_decim_edge_collapse(BMesh *bm,
                                   BMEdge *e,
                                   Quadric *vquadrics,
                                   float *vweights,
                                   const float vweight_factor,
                                   Heap *eheap,
                                   HeapNode **eheap_table,
#ifdef USE_SYMMETRY
                                   int *edge_symmetry_map,
#endif
                                   const CD_UseFlag customdata_flag,
                                   float optimize_co[3],
                                   bool optimize_co_calc)
{
  int e_clear_other[2];
  BMVert *v_other = e->v1;
  const int v_other_index = BM_elem_index_get(e->v1);
  /* the vert is removed so only store the index */
  const int v_clear_index = BM_elem_index_get(e->v2);
  float customdata_fac;

#ifdef USE_VERT_NORMAL_INTERP
  float v_clear_no[3];
  copy_v3_v3(v_clear_no, e->v2->no);
#endif

  /* when false, use without degenerate checks */
  if (optimize_co_calc) {
    /* disallow collapsing which results in degenerate cases */
    if (UNLIKELY(bm_edge_collapse_is_degenerate_topology(e))) {
      /* add back with a high cost */
      bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);
      return false;
    }

    bm_decim_calc_target_co_fl(e, optimize_co, vquadrics);

    /* check if this would result in an overlapping face */
    if (UNLIKELY(bm_edge_collapse_is_degenerate_flip(e, optimize_co))) {
      /* add back with a high cost */
      bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);
      return false;
    }
  }

  /* use for customdata merging */
  if (LIKELY(compare_v3v3(e->v1->co, e->v2->co, FLT_EPSILON) == false)) {
    customdata_fac = line_point_factor_v3(optimize_co, e->v1->co, e->v2->co);
#if 0
    /* simple test for stupid collapse */
    if (customdata_fac < 0.0 - FLT_EPSILON || customdata_fac > 1.0f + FLT_EPSILON) {
      return false;
    }
#endif
  }
  else {
    /* avoid divide by zero */
    customdata_fac = 0.5f;
  }

  if (bm_edge_collapse(bm,
                       e,
                       e->v2,
                       e_clear_other,
#ifdef USE_SYMMETRY
                       edge_symmetry_map,
#endif
                       customdata_flag,
                       customdata_fac)) {
    /* update collapse info */
    int i;

    if (vweights) {
      float v_other_weight = interpf(
          vweights[v_other_index], vweights[v_clear_index], customdata_fac);
      CLAMP(v_other_weight, 0.0f, 1.0f);
      vweights[v_other_index] = v_other_weight;
    }

    /* paranoid safety check */
    e = NULL;

    copy_v3_v3(v_other->co, optimize_co);

    /* remove eheap */
    for (i = 0; i < 2; i++) {
      /* highly unlikely 'eheap_table[ke_other[i]]' would be NULL, but do for sanity sake */
      if ((e_clear_other[i] != -1) && (eheap_table[e_clear_other[i]] != NULL)) {
        BLI_heap_remove(eheap, eheap_table[e_clear_other[i]]);
        eheap_table[e_clear_other[i]] = NULL;
      }
    }

    /* update vertex quadric, add kept vertex from killed vertex */
    BLI_quadric_add_qu_qu(&vquadrics[v_other_index], &vquadrics[v_clear_index]);

    /* update connected normals */

    /* in fact face normals are not used for progressive updates, no need to update them */
    // BM_vert_normal_update_all(v);
#ifdef USE_VERT_NORMAL_INTERP
    interp_v3_v3v3(v_other->no, v_other->no, v_clear_no, customdata_fac);
    normalize_v3(v_other->no);
#else
    BM_vert_normal_update(v_other);
#endif

    /* update error costs and the eheap */
    if (LIKELY(v_other->e)) {
      BMEdge *e_iter;
      BMEdge *e_first;
      e_iter = e_first = v_other->e;
      do {
        BLI_assert(BM_edge_find_double(e_iter) == NULL);
        bm_decim_build_edge_cost_single(
            e_iter, vquadrics, vweights, vweight_factor, eheap, eheap_table);
      } while ((e_iter = bmesh_disk_edge_next(e_iter, v_other)) != e_first);
    }

    /* this block used to be disabled,
     * but enable now since surrounding faces may have been
     * set to COST_INVALID because of a face overlap that no longer occurs */
#if 1
    /* optional, update edges around the vertex face fan */
    {
      BMIter liter;
      BMLoop *l;
      BM_ITER_ELEM (l, &liter, v_other, BM_LOOPS_OF_VERT) {
        if (l->f->len == 3) {
          BMEdge *e_outer;
          if (BM_vert_in_edge(l->prev->e, l->v)) {
            e_outer = l->next->e;
          }
          else {
            e_outer = l->prev->e;
          }

          BLI_assert(BM_vert_in_edge(e_outer, l->v) == false);

          bm_decim_build_edge_cost_single(
              e_outer, vquadrics, vweights, vweight_factor, eheap, eheap_table);
        }
      }
    }
    /* end optional update */
    return true;
#endif
  }
  else {
    /* add back with a high cost */
    bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);
    return false;
  }
}

/* Main Decimate Function
 * ********************** */

/**
 * \brief BM_mesh_decimate
 * \param bm: The mesh
 * \param factor: face count multiplier [0 - 1]
 * \param vweights: Optional array of vertex  aligned weights [0 - 1],
 *        a vertex group is the usual source for this.
 * \param symmetry_axis: Axis of symmetry, -1 to disable mirror decimate.
 * \param symmetry_eps: Threshold when matching mirror verts.
 */
void BM_mesh_decimate_collapse(BMesh *bm,
                               const float factor,
                               float *vweights,
                               float vweight_factor,
                               const bool do_triangulate,
                               const int symmetry_axis,
                               const float symmetry_eps)
{
  /* edge heap */
  Heap *eheap;
  /* edge index aligned table pointing to the eheap */
  HeapNode **eheap_table;
  /* vert index aligned quadrics */
  Quadric *vquadrics;
  int tot_edge_orig;
  int face_tot_target;

  CD_UseFlag customdata_flag = 0;

#ifdef USE_SYMMETRY
  bool use_symmetry = (symmetry_axis != -1);
  int *edge_symmetry_map;
#endif

#ifdef USE_TRIANGULATE
  int edges_tri_tot = 0;
  /* temp convert quads to triangles */
  bool use_triangulate = bm_decim_triangulate_begin(bm, &edges_tri_tot);
#else
  UNUSED_VARS(do_triangulate);
#endif

  /* alloc vars */
  vquadrics = MEM_callocN(sizeof(Quadric) * bm->totvert, __func__);
  /* since some edges may be degenerate, we might be over allocing a little here */
  eheap = BLI_heap_new_ex(bm->totedge);
  eheap_table = MEM_mallocN(sizeof(HeapNode *) * bm->totedge, __func__);
  tot_edge_orig = bm->totedge;

  /* build initial edge collapse cost data */
  bm_decim_build_quadrics(bm, vquadrics);

  bm_decim_build_edge_cost(bm, vquadrics, vweights, vweight_factor, eheap, eheap_table);

  face_tot_target = bm->totface * factor;
  bm->elem_index_dirty |= BM_ALL;

#ifdef USE_SYMMETRY
  edge_symmetry_map = (use_symmetry) ? bm_edge_symmetry_map(bm, symmetry_axis, symmetry_eps) :
                                       NULL;
#else
  UNUSED_VARS(symmetry_axis, symmetry_eps);
#endif

#ifdef USE_CUSTOMDATA
  /* initialize customdata flag, we only need math for loops */
  if (CustomData_has_interp(&bm->vdata)) {
    customdata_flag |= CD_DO_VERT;
  }
  if (CustomData_has_interp(&bm->edata)) {
    customdata_flag |= CD_DO_EDGE;
  }
  if (CustomData_has_math(&bm->ldata)) {
    customdata_flag |= CD_DO_LOOP;
  }
#endif

  /* iterative edge collapse and maintain the eheap */
#ifdef USE_SYMMETRY
  if (use_symmetry == false)
#endif
  {
    /* simple non-mirror case */
    while ((bm->totface > face_tot_target) && (BLI_heap_is_empty(eheap) == false) &&
           (BLI_heap_top_value(eheap) != COST_INVALID)) {
      // const float value = BLI_heap_node_value(BLI_heap_top(eheap));
      BMEdge *e = BLI_heap_pop_min(eheap);
      float optimize_co[3];
      /* handy to detect corruptions elsewhere */
      BLI_assert(BM_elem_index_get(e) < tot_edge_orig);

      /* under normal conditions wont be accessed again,
       * but NULL just incase so we don't use freed node */
      eheap_table[BM_elem_index_get(e)] = NULL;

      bm_decim_edge_collapse(bm,
                             e,
                             vquadrics,
                             vweights,
                             vweight_factor,
                             eheap,
                             eheap_table,
#ifdef USE_SYMMETRY
                             edge_symmetry_map,
#endif
                             customdata_flag,
                             optimize_co,
                             true);
    }
  }
#ifdef USE_SYMMETRY
  else {
    while ((bm->totface > face_tot_target) && (BLI_heap_is_empty(eheap) == false) &&
           (BLI_heap_top_value(eheap) != COST_INVALID)) {
      /**
       * \note
       * - `eheap_table[e_index_mirr]` is only removed from the heap at the last moment
       *   since its possible (in theory) for collapsing `e` to remove `e_mirr`.
       * - edges sharing a vertex are ignored, so the pivot vertex isnt moved to one side.
       */

      BMEdge *e = BLI_heap_pop_min(eheap);
      const int e_index = BM_elem_index_get(e);
      const int e_index_mirr = edge_symmetry_map[e_index];
      BMEdge *e_mirr = NULL;
      float optimize_co[3];
      char e_invalidate = 0;

      BLI_assert(e_index < tot_edge_orig);

      eheap_table[e_index] = NULL;

      if (e_index_mirr != -1) {
        if (e_index_mirr == e_index) {
          /* pass */
        }
        else if (eheap_table[e_index_mirr]) {
          e_mirr = BLI_heap_node_ptr(eheap_table[e_index_mirr]);
          /* for now ignore edges with a shared vertex */
          if (BM_edge_share_vert_check(e, e_mirr)) {
            /* ignore permanently!
             * Otherwise we would keep re-evaluating and attempting to collapse. */
            // e_invalidate |= (1 | 2);
            goto invalidate;
          }
        }
        else {
          /* mirror edge can't be operated on (happens with asymmetrical meshes) */
          e_invalidate |= 1;
          goto invalidate;
        }
      }

      /* when false, use without degenerate checks */
      {
        /* run both before checking (since they invalidate surrounding geometry) */
        bool ok_a, ok_b;

        ok_a = !bm_edge_collapse_is_degenerate_topology(e);
        ok_b = e_mirr ? !bm_edge_collapse_is_degenerate_topology(e_mirr) : true;

        /* disallow collapsing which results in degenerate cases */

        if (UNLIKELY(!ok_a || !ok_b)) {
          e_invalidate |= (1 | (e_mirr ? 2 : 0));
          goto invalidate;
        }

        bm_decim_calc_target_co_fl(e, optimize_co, vquadrics);

        if (e_index_mirr == e_index) {
          optimize_co[symmetry_axis] = 0.0f;
        }

        /* check if this would result in an overlapping face */
        if (UNLIKELY(bm_edge_collapse_is_degenerate_flip(e, optimize_co))) {
          e_invalidate |= (1 | (e_mirr ? 2 : 0));
          goto invalidate;
        }
      }

      if (bm_decim_edge_collapse(bm,
                                 e,
                                 vquadrics,
                                 vweights,
                                 vweight_factor,
                                 eheap,
                                 eheap_table,
                                 edge_symmetry_map,
                                 customdata_flag,
                                 optimize_co,
                                 false)) {
        if (e_mirr && (eheap_table[e_index_mirr])) {
          BLI_assert(e_index_mirr != e_index);
          BLI_heap_remove(eheap, eheap_table[e_index_mirr]);
          eheap_table[e_index_mirr] = NULL;
          optimize_co[symmetry_axis] *= -1.0f;
          bm_decim_edge_collapse(bm,
                                 e_mirr,
                                 vquadrics,
                                 vweights,
                                 vweight_factor,
                                 eheap,
                                 eheap_table,
                                 edge_symmetry_map,
                                 customdata_flag,
                                 optimize_co,
                                 false);
        }
      }
      else {
        if (e_mirr && (eheap_table[e_index_mirr])) {
          e_invalidate |= 2;
          goto invalidate;
        }
      }

      BLI_assert(e_invalidate == 0);
      continue;

    invalidate:
      if (e_invalidate & 1) {
        bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);
      }

      if (e_invalidate & 2) {
        BLI_assert(eheap_table[e_index_mirr] != NULL);
        BLI_heap_remove(eheap, eheap_table[e_index_mirr]);
        eheap_table[e_index_mirr] = NULL;
        bm_decim_invalid_edge_cost_single(e_mirr, eheap, eheap_table);
      }
    }

    MEM_freeN((void *)edge_symmetry_map);
  }
#endif /* USE_SYMMETRY */

#ifdef USE_TRIANGULATE
  if (do_triangulate == false) {
    /* its possible we only had triangles, skip this step in that case */
    if (LIKELY(use_triangulate)) {
      /* temp convert quads to triangles */
      bm_decim_triangulate_end(bm, edges_tri_tot);
    }
  }
#endif

  /* free vars */
  MEM_freeN(vquadrics);
  MEM_freeN(eheap_table);
  BLI_heap_free(eheap, NULL);

  /* testing only */
  // BM_mesh_validate(bm);

  /* quiet release build warning */
  (void)tot_edge_orig;
}
