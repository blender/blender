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
 * Rotate edges topology that share two faces.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_heap.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_OUT 1
#define FACE_MARK 1

/**
 * Rotate edges where every edge has it's own faces (we can rotate in any order).
 */
static void bm_rotate_edges_simple(BMesh *bm,
                                   BMOperator *op,
                                   const short check_flag,
                                   const bool use_ccw)
{
  BMOIter siter;
  BMEdge *e;

  BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
    /**
     * this ends up being called twice, could add option to not to call check in
     * #BM_edge_rotate to get some extra speed */
    if (BM_edge_rotate_check(e)) {
      BMEdge *e_rotate = BM_edge_rotate(bm, e, use_ccw, check_flag);
      if (e_rotate != NULL) {
        BMO_edge_flag_enable(bm, e_rotate, EDGE_OUT);
      }
    }
  }
}

/**
 * Edge length is just a way of ordering that's independent of order in the edges argument,
 * we could use some other method since ideally all edges will be rotated,
 * this just happens to be simple to calculate.
 */
static float bm_edge_calc_rotate_cost(const BMEdge *e)
{
  return -BM_edge_calc_length_squared(e);
}

/**
 * Check if this edge is a boundary: Are more than one of the connected faces edges rotating too?
 */
static float bm_edge_rotate_is_boundary(const BMEdge *e)
{
  /* Number of adjacent shared faces. */
  int count = 0;
  BMLoop *l_radial_iter = e->l;
  do {
    /* Skip this edge. */
    BMLoop *l_iter = l_radial_iter->next;
    do {
      BMEdge *e_iter = l_iter->e;
      const int e_iter_index = BM_elem_index_get(e_iter);
      if ((e_iter_index != -1)) {
        if (count == 1) {
          return false;
        }
        count += 1;
        break;
      }
    } while ((l_iter = l_iter->next) != l_radial_iter);
  } while ((l_radial_iter = l_radial_iter->radial_next) != e->l);
  return true;
}

/**
 * Rotate edges where edges share faces,
 * edges which could not rotate need to be re-considered after neighbors are rotated.
 */
static void bm_rotate_edges_shared(
    BMesh *bm, BMOperator *op, short check_flag, const bool use_ccw, const int edges_len)
{
  Heap *heap = BLI_heap_new_ex(edges_len);
  HeapNode **eheap_table = MEM_mallocN(sizeof(*eheap_table) * edges_len, __func__);
  int edges_len_rotate = 0;

  {
    BMIter iter;
    BMEdge *e;
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_index_set(e, -1); /* set_dirty! */
    }
    bm->elem_index_dirty |= BM_EDGE;
  }

  {
    BMOIter siter;
    BMEdge *e;
    uint i;
    BMO_ITER_INDEX (e, &siter, op->slots_in, "edges", BM_EDGE, i) {
      BM_elem_index_set(e, BM_edge_is_manifold(e) ? i : -1); /* set_dirty! */
      eheap_table[i] = NULL;
    }
  }

  /* First operate on boundary edges, this is often all that's needed,
   * regions that have no boundaries are handles after. */
  enum {
    PASS_TYPE_BOUNDARY = 0,
    PASS_TYPE_ALL = 1,
    PASS_TYPE_DONE = 2,
  };
  uint pass_type = PASS_TYPE_BOUNDARY;

  while ((pass_type != PASS_TYPE_DONE) && (edges_len_rotate != edges_len)) {
    BLI_assert(BLI_heap_is_empty(heap));
    {
      BMOIter siter;
      BMEdge *e;
      uint i;
      BMO_ITER_INDEX (e, &siter, op->slots_in, "edges", BM_EDGE, i) {
        BLI_assert(eheap_table[i] == NULL);

        bool ok = (BM_elem_index_get(e) != -1) && BM_edge_rotate_check(e);

        if (ok) {
          if (pass_type == PASS_TYPE_BOUNDARY) {
            ok = bm_edge_rotate_is_boundary(e);
          }
        }

        if (ok) {
          float cost = bm_edge_calc_rotate_cost(e);
          if (pass_type == PASS_TYPE_BOUNDARY) {
            /* Trick to ensure once started,
             * non boundaries are handled before other boundary edges.
             * This means the first longest boundary defines the starting point which is rotated
             * until all its connected edges are exhausted
             * and the next boundary is popped off the heap.
             *
             * Without this we may rotate from different starting points and meet in the middle
             * with obviously uneven topology.
             *
             * Move from negative to positive value,
             * inverting so large values are still handled first.
             */
            cost = cost != 0.0f ? -1.0f / cost : FLT_MAX;
          }
          eheap_table[i] = BLI_heap_insert(heap, cost, e);
        }
      }
    }

    if (BLI_heap_is_empty(heap)) {
      pass_type += 1;
      continue;
    }

    const int edges_len_rotate_prev = edges_len_rotate;
    while (!BLI_heap_is_empty(heap)) {
      BMEdge *e_best = BLI_heap_pop_min(heap);
      eheap_table[BM_elem_index_get(e_best)] = NULL;

      /* No problem if this fails, re-evaluate if faces connected to this edge are touched. */
      if (BM_edge_rotate_check(e_best)) {
        BMEdge *e_rotate = BM_edge_rotate(bm, e_best, use_ccw, check_flag);
        if (e_rotate != NULL) {
          BMO_edge_flag_enable(bm, e_rotate, EDGE_OUT);

          /* invalidate so we don't try touch this again. */
          BM_elem_index_set(e_rotate, -1); /* set_dirty! */

          edges_len_rotate += 1;

          /* Note: we could validate all edges which have not been rotated
           * (not just previously degenerate edges).
           * However there is no real need -
           * they can be left until they're popped off the queue. */

          /* We don't know the exact topology after rotating the edge,
           * so loop over all faces attached to the new edge,
           * typically this will only be two faces. */
          BMLoop *l_radial_iter = e_rotate->l;
          do {
            /* Skip this edge. */
            BMLoop *l_iter = l_radial_iter->next;
            do {
              BMEdge *e_iter = l_iter->e;
              const int e_iter_index = BM_elem_index_get(e_iter);
              if ((e_iter_index != -1) && (eheap_table[e_iter_index] == NULL)) {
                if (BM_edge_rotate_check(e_iter)) {
                  /* Previously degenerate, now valid. */
                  float cost = bm_edge_calc_rotate_cost(e_iter);
                  eheap_table[e_iter_index] = BLI_heap_insert(heap, cost, e_iter);
                }
              }
            } while ((l_iter = l_iter->next) != l_radial_iter);
          } while ((l_radial_iter = l_radial_iter->radial_next) != e_rotate->l);
        }
      }
    }

    /* If no actions were taken, move onto the next pass. */
    if (edges_len_rotate == edges_len_rotate_prev) {
      pass_type += 1;
      continue;
    }
  }

  BLI_heap_free(heap, NULL);
  MEM_freeN(eheap_table);
}

void bmo_rotate_edges_exec(BMesh *bm, BMOperator *op)
{
  BMOIter siter;
  BMEdge *e;
  const int edges_len = BMO_slot_buffer_count(op->slots_in, "edges");
  const bool use_ccw = BMO_slot_bool_get(op->slots_in, "use_ccw");
  const bool is_single = (edges_len == 1);
  short check_flag = is_single ? BM_EDGEROT_CHECK_EXISTS :
                                 BM_EDGEROT_CHECK_EXISTS | BM_EDGEROT_CHECK_DEGENERATE;

  bool is_simple = true;
  if (is_single == false) {
    BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
      BMFace *f_pair[2];
      if (BM_edge_face_pair(e, &f_pair[0], &f_pair[1])) {
        for (uint i = 0; i < ARRAY_SIZE(f_pair); i += 1) {
          if (BMO_face_flag_test(bm, f_pair[i], FACE_MARK)) {
            is_simple = false;
            break;
          }
          BMO_face_flag_enable(bm, f_pair[i], FACE_MARK);
        }
        if (is_simple == false) {
          break;
        }
      }
    }
  }

  if (is_simple) {
    bm_rotate_edges_simple(bm, op, check_flag, use_ccw);
  }
  else {
    bm_rotate_edges_shared(bm, op, check_flag, use_ccw, edges_len);
  }

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);
}
