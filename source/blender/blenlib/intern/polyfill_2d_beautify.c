/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * This function is to improve the tessellation resulting from polyfill2d,
 * creating optimal topology.
 *
 * The functionality here matches #BM_mesh_beautify_fill,
 * but its far simpler to perform this operation in 2d,
 * on a simple polygon representation where we _know_:
 *
 * - The polygon is primitive with no holes with a continuous boundary.
 * - Triangles have consistent winding.
 * - 2d (saves some hassles projecting face pairs on an axis for every edge-rotation)
 *   also saves us having to store all previous edge-states
 *   (see #EdRotState in `bmesh_beautify.cc`).
 *
 * \note
 *
 * No globals - keep threadsafe.
 */

#include "BLI_utildefines.h"

#include "BLI_heap.h"
#include "BLI_math_geom.h"
#include "BLI_memarena.h"

#include "BLI_polyfill_2d_beautify.h" /* own include */

#include "BLI_strict_flags.h"

/* Used to find matching edges. */
struct OrderEdge {
  uint verts[2];
  uint e_half;
};

/* Half edge used for rotating in-place. */
struct HalfEdge {
  uint v;
  uint e_next;
  uint e_radial;
  uint base_index;
};

static int oedge_cmp(const void *a1, const void *a2)
{
  const struct OrderEdge *x1 = a1, *x2 = a2;
  if (x1->verts[0] > x2->verts[0]) {
    return 1;
  }
  if (x1->verts[0] < x2->verts[0]) {
    return -1;
  }

  if (x1->verts[1] > x2->verts[1]) {
    return 1;
  }
  if (x1->verts[1] < x2->verts[1]) {
    return -1;
  }

  /* Only for predictability. */
  if (x1->e_half > x2->e_half) {
    return 1;
  }
  if (x1->e_half < x2->e_half) {
    return -1;
  }
  /* Should never get here, no two edges should be the same. */
  BLI_assert(false);
  return 0;
}

BLI_INLINE bool is_boundary_edge(uint i_a, uint i_b, const uint coord_last)
{
  BLI_assert(i_a < i_b);
  return ((i_a + 1 == i_b) || UNLIKELY((i_a == 0) && (i_b == coord_last)));
}
float BLI_polyfill_beautify_quad_rotate_calc_ex(const float v1[2],
                                                const float v2[2],
                                                const float v3[2],
                                                const float v4[2],
                                                const bool lock_degenerate,
                                                float *r_area)
{
  /* not a loop (only to be able to break out) */
  do {
    /* Allow very small faces to be considered non-zero. */
    const float eps_zero_area = 1e-12f;
    const float area_2x_234 = cross_tri_v2(v2, v3, v4);
    const float area_2x_241 = cross_tri_v2(v2, v4, v1);

    const float area_2x_123 = cross_tri_v2(v1, v2, v3);
    const float area_2x_134 = cross_tri_v2(v1, v3, v4);

    BLI_assert((ELEM(v1, v2, v3, v4) == false) && (ELEM(v2, v1, v3, v4) == false) &&
               (ELEM(v3, v1, v2, v4) == false) && (ELEM(v4, v1, v2, v3) == false));

    if (r_area) {
      *r_area = fabsf(area_2x_234) + fabsf(area_2x_241) +
                /* Include both pairs for predictable results. */
                fabsf(area_2x_123) + fabsf(area_2x_134) / 8.0f;
    }

    /*
     * Test for unusable (1-3) state.
     * - Area sign flipping to check faces aren't going to point in opposite directions.
     * - Area epsilon check that the one of the faces won't be zero area.
     */
    if ((area_2x_123 >= 0.0f) != (area_2x_134 >= 0.0f)) {
      break;
    }
    if ((fabsf(area_2x_123) <= eps_zero_area) || (fabsf(area_2x_134) <= eps_zero_area)) {
      break;
    }

    /* Test for unusable (2-4) state (same as above). */
    if ((area_2x_234 >= 0.0f) != (area_2x_241 >= 0.0f)) {
      if (lock_degenerate) {
        break;
      }

      return -FLT_MAX; /* always rotate */
    }
    if ((fabsf(area_2x_234) <= eps_zero_area) || (fabsf(area_2x_241) <= eps_zero_area)) {
      return -FLT_MAX; /* always rotate */
    }

    {
      /* testing rule: the area divided by the perimeter,
       * check if (1-3) beats the existing (2-4) edge rotation */
      float area_a, area_b;
      float prim_a, prim_b;
      float fac_24, fac_13;

      float len_12, len_23, len_34, len_41, len_24, len_13;

      /* edges around the quad */
      len_12 = len_v2v2(v1, v2);
      len_23 = len_v2v2(v2, v3);
      len_34 = len_v2v2(v3, v4);
      len_41 = len_v2v2(v4, v1);
      /* edges crossing the quad interior */
      len_13 = len_v2v2(v1, v3);
      len_24 = len_v2v2(v2, v4);

      /* NOTE: area is in fact (area * 2),
       * but in this case its OK, since we're comparing ratios */

      /* edge (2-4), current state */
      area_a = fabsf(area_2x_234);
      area_b = fabsf(area_2x_241);
      prim_a = len_23 + len_34 + len_24;
      prim_b = len_41 + len_12 + len_24;
      fac_24 = (area_a / prim_a) + (area_b / prim_b);

      /* edge (1-3), new state */
      area_a = fabsf(area_2x_123);
      area_b = fabsf(area_2x_134);
      prim_a = len_12 + len_23 + len_13;
      prim_b = len_34 + len_41 + len_13;
      fac_13 = (area_a / prim_a) + (area_b / prim_b);

      /* negative number if (1-3) is an improved state */
      return fac_24 - fac_13;
    }
  } while (false);

  return FLT_MAX;
}

static float polyedge_rotate_beauty_calc(const float (*coords)[2],
                                         const struct HalfEdge *edges,
                                         const struct HalfEdge *e_a,
                                         float *r_area)
{
  const struct HalfEdge *e_b = &edges[e_a->e_radial];

  const struct HalfEdge *e_a_other = &edges[edges[e_a->e_next].e_next];
  const struct HalfEdge *e_b_other = &edges[edges[e_b->e_next].e_next];

  const float *v1, *v2, *v3, *v4;

  v1 = coords[e_a_other->v];
  v2 = coords[e_a->v];
  v3 = coords[e_b_other->v];
  v4 = coords[e_b->v];

  return BLI_polyfill_beautify_quad_rotate_calc_ex(v1, v2, v3, v4, false, r_area);
}

static void polyedge_beauty_cost_update_single(const float (*coords)[2],
                                               const struct HalfEdge *edges,
                                               struct HalfEdge *e,
                                               Heap *eheap,
                                               HeapNode **eheap_table)
{
  const uint i = e->base_index;
  /* recalculate edge */
  float area;
  const float cost = polyedge_rotate_beauty_calc(coords, edges, e, &area);
  /* We can get cases where both choices generate very small negative costs,
   * which leads to infinite loop. Anyway, costs above that are not worth recomputing,
   * maybe we could even optimize it to a smaller limit?
   * Actually, FLT_EPSILON is too small in some cases, 1e-6f seems to work OK hopefully?
   * See #43578, #49478.
   *
   * In fact a larger epsilon can still fail when the area of the face is very large,
   * now the epsilon is scaled by the face area.
   * See #56532. */
  if (cost < -1e-6f * max_ff(area, 1.0f)) {
    BLI_heap_insert_or_update(eheap, &eheap_table[i], cost, e);
  }
  else {
    if (eheap_table[i]) {
      BLI_heap_remove(eheap, eheap_table[i]);
      eheap_table[i] = NULL;
    }
  }
}

static void polyedge_beauty_cost_update(const float (*coords)[2],
                                        struct HalfEdge *edges,
                                        struct HalfEdge *e,
                                        Heap *eheap,
                                        HeapNode **eheap_table)
{
  struct HalfEdge *e_arr[4];
  e_arr[0] = &edges[e->e_next];
  e_arr[1] = &edges[e_arr[0]->e_next];

  e = &edges[e->e_radial];
  e_arr[2] = &edges[e->e_next];
  e_arr[3] = &edges[e_arr[2]->e_next];

  for (uint i = 0; i < 4; i++) {
    if (e_arr[i] && e_arr[i]->base_index != UINT_MAX) {
      polyedge_beauty_cost_update_single(coords, edges, e_arr[i], eheap, eheap_table);
    }
  }
}

static void polyedge_rotate(struct HalfEdge *edges, struct HalfEdge *e)
{
  /** CCW winding, rotate internal edge to new vertical state.
   *
   * \code{.unparsed}
   *   Before         After
   *      X             X
   *     / \           /|\
   *  e4/   \e5     e4/ | \e5
   *   / e3  \       /  |  \
   * X ------- X -> X e0|e3 X
   *   \ e0  /       \  |  /
   *  e2\   /e1     e2\ | /e1
   *     \ /           \|/
   *      X             X
   * \endcode
   */
  struct HalfEdge *ed[6];
  uint ed_index[6];

  ed_index[0] = (uint)(e - edges);
  ed[0] = &edges[ed_index[0]];
  ed_index[1] = ed[0]->e_next;
  ed[1] = &edges[ed_index[1]];
  ed_index[2] = ed[1]->e_next;
  ed[2] = &edges[ed_index[2]];

  ed_index[3] = e->e_radial;
  ed[3] = &edges[ed_index[3]];
  ed_index[4] = ed[3]->e_next;
  ed[4] = &edges[ed_index[4]];
  ed_index[5] = ed[4]->e_next;
  ed[5] = &edges[ed_index[5]];

  ed[0]->e_next = ed_index[2];
  ed[1]->e_next = ed_index[3];
  ed[2]->e_next = ed_index[4];
  ed[3]->e_next = ed_index[5];
  ed[4]->e_next = ed_index[0];
  ed[5]->e_next = ed_index[1];

  ed[0]->v = ed[5]->v;
  ed[3]->v = ed[2]->v;
}

void BLI_polyfill_beautify(const float (*coords)[2],
                           const uint coords_num,
                           uint (*tris)[3],

                           /* structs for reuse */
                           MemArena *arena,
                           Heap *eheap)
{
  const uint coord_last = coords_num - 1;
  const uint tris_len = coords_num - 2;
  /* internal edges only (between 2 tris) */
  const uint edges_len = tris_len - 1;

  HeapNode **eheap_table;

  const uint half_edges_len = 3 * tris_len;
  struct HalfEdge *half_edges = BLI_memarena_alloc(arena, sizeof(*half_edges) * half_edges_len);
  struct OrderEdge *order_edges = BLI_memarena_alloc(arena,
                                                     sizeof(struct OrderEdge) * 2 * edges_len);
  uint order_edges_len = 0;

  /* first build edges */
  for (uint i = 0; i < tris_len; i++) {
    for (uint j_curr = 0, j_prev = 2; j_curr < 3; j_prev = j_curr++) {
      const uint e_index_prev = (i * 3) + j_prev;
      const uint e_index_curr = (i * 3) + j_curr;

      half_edges[e_index_prev].v = tris[i][j_prev];
      half_edges[e_index_prev].e_next = e_index_curr;
      half_edges[e_index_prev].e_radial = UINT_MAX;
      half_edges[e_index_prev].base_index = UINT_MAX;

      uint e_pair[2] = {tris[i][j_prev], tris[i][j_curr]};
      if (e_pair[0] > e_pair[1]) {
        SWAP(uint, e_pair[0], e_pair[1]);
      }

      /* ensure internal edges. */
      if (!is_boundary_edge(e_pair[0], e_pair[1], coord_last)) {
        order_edges[order_edges_len].verts[0] = e_pair[0];
        order_edges[order_edges_len].verts[1] = e_pair[1];
        order_edges[order_edges_len].e_half = e_index_prev;
        order_edges_len += 1;
      }
    }
  }
  BLI_assert(edges_len * 2 == order_edges_len);

  qsort(order_edges, order_edges_len, sizeof(struct OrderEdge), oedge_cmp);

  for (uint i = 0, base_index = 0; i < order_edges_len; base_index++) {
    const struct OrderEdge *oe_a = &order_edges[i++];
    const struct OrderEdge *oe_b = &order_edges[i++];
    BLI_assert(oe_a->verts[0] == oe_b->verts[0] && oe_a->verts[1] == oe_b->verts[1]);
    half_edges[oe_a->e_half].e_radial = oe_b->e_half;
    half_edges[oe_b->e_half].e_radial = oe_a->e_half;
    half_edges[oe_a->e_half].base_index = base_index;
    half_edges[oe_b->e_half].base_index = base_index;
  }
  /* order_edges could be freed now. */

  /* Now perform iterative rotations. */
#if 0
  eheap_table = BLI_memarena_alloc(arena, sizeof(HeapNode *) * (size_t)edges_len);
#else
  /* We can re-use this since its big enough. */
  eheap_table = (void *)order_edges;
  order_edges = NULL;
#endif

  /* Build heap. */
  {
    struct HalfEdge *e = half_edges;
    for (uint i = 0; i < half_edges_len; i++, e++) {
      /* Accounts for boundary edged too (UINT_MAX). */
      if (e->e_radial < i) {
        const float cost = polyedge_rotate_beauty_calc(coords, half_edges, e, NULL);
        if (cost < 0.0f) {
          eheap_table[e->base_index] = BLI_heap_insert(eheap, cost, e);
        }
        else {
          eheap_table[e->base_index] = NULL;
        }
      }
    }
  }

  while (BLI_heap_is_empty(eheap) == false) {
    struct HalfEdge *e = BLI_heap_pop_min(eheap);
    eheap_table[e->base_index] = NULL;

    polyedge_rotate(half_edges, e);

    /* recalculate faces connected on the heap */
    polyedge_beauty_cost_update(coords, half_edges, e, eheap, eheap_table);
  }

  BLI_heap_clear(eheap, NULL);

  // MEM_freeN(eheap_table); /* arena */

  /* get tris from half edge. */
  uint tri_index = 0;
  for (uint i = 0; i < half_edges_len; i++) {
    struct HalfEdge *e = &half_edges[i];
    if (e->v != UINT_MAX) {
      uint *tri = tris[tri_index++];

      tri[0] = e->v;
      e->v = UINT_MAX;

      e = &half_edges[e->e_next];
      tri[1] = e->v;
      e->v = UINT_MAX;

      e = &half_edges[e->e_next];
      tri[2] = e->v;
      e->v = UINT_MAX;
    }
  }
}
