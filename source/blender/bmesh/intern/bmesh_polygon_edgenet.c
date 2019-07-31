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
 * This file contains functions for splitting faces into isolated regions,
 * defined by connected edges.
 */
// #define DEBUG_PRINT

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_utildefines_stack.h"
#include "BLI_linklist_stack.h"
#include "BLI_sort_utils.h"
#include "BLI_kdopbvh.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* -------------------------------------------------------------------- */
/* Face Split Edge-Net */

/** \name BM_face_split_edgenet and helper functions.
 *
 * \note Don't use #BM_edge_is_wire or #BM_edge_is_boundary
 * since we need to take flagged faces into account.
 * Also take care accessing e->l directly.
 *
 * \{ */

/* Note: All these flags _must_ be cleared on exit */

/* face is apart of the edge-net (including the original face we're splitting) */
#define FACE_NET _FLAG_WALK
/* edge is apart of the edge-net we're filling */
#define EDGE_NET _FLAG_WALK
/* tag verts we've visit */
#define VERT_VISIT _FLAG_WALK
#define VERT_IN_QUEUE _FLAG_WALK_ALT

struct VertOrder {
  float angle;
  BMVert *v;
};

static uint bm_edge_flagged_radial_count(BMEdge *e)
{
  uint count = 0;
  BMLoop *l;

  if ((l = e->l)) {
    do {
      if (BM_ELEM_API_FLAG_TEST(l->f, FACE_NET)) {
        count++;
      }
    } while ((l = l->radial_next) != e->l);
  }
  return count;
}

static BMLoop *bm_edge_flagged_radial_first(BMEdge *e)
{
  BMLoop *l;

  if ((l = e->l)) {
    do {
      if (BM_ELEM_API_FLAG_TEST(l->f, FACE_NET)) {
        return l;
      }
    } while ((l = l->radial_next) != e->l);
  }
  return NULL;
}

static void normalize_v2_m3_v3v3(float out[2],
                                 float axis_mat[3][3],
                                 const float v1[3],
                                 const float v2[3])
{
  float dir[3];
  sub_v3_v3v3(dir, v1, v2);
  mul_v2_m3v3(out, axis_mat, dir);
  normalize_v2(out);
}

/**
 * \note Be sure to update #bm_face_split_edgenet_find_loop_pair_exists
 * when making changed to edge picking logic.
 */
static bool bm_face_split_edgenet_find_loop_pair(BMVert *v_init,
                                                 const float face_normal[3],
                                                 float face_normal_matrix[3][3],
                                                 BMEdge *e_pair[2])
{
  /* Always find one boundary edge (to determine winding)
   * and one wire (if available), otherwise another boundary.
   */

  /* detect winding */
  BMLoop *l_walk;
  bool swap;

  BLI_SMALLSTACK_DECLARE(edges_boundary, BMEdge *);
  BLI_SMALLSTACK_DECLARE(edges_wire, BMEdge *);
  int edges_boundary_len = 0;
  int edges_wire_len = 0;

  {
    BMEdge *e, *e_first;
    e = e_first = v_init->e;
    do {
      if (BM_ELEM_API_FLAG_TEST(e, EDGE_NET)) {
        const uint count = bm_edge_flagged_radial_count(e);
        if (count == 1) {
          BLI_SMALLSTACK_PUSH(edges_boundary, e);
          edges_boundary_len++;
        }
        else if (count == 0) {
          BLI_SMALLSTACK_PUSH(edges_wire, e);
          edges_wire_len++;
        }
      }
    } while ((e = BM_DISK_EDGE_NEXT(e, v_init)) != e_first);
  }

  /* first edge should always be boundary */
  if (edges_boundary_len == 0) {
    return false;
  }
  e_pair[0] = BLI_SMALLSTACK_POP(edges_boundary);

  /* use to hold boundary OR wire edges */
  BLI_SMALLSTACK_DECLARE(edges_search, BMEdge *);

  /* attempt one boundary and one wire, or 2 boundary */
  if (edges_wire_len == 0) {
    if (edges_boundary_len > 1) {
      e_pair[1] = BLI_SMALLSTACK_POP(edges_boundary);

      if (edges_boundary_len > 2) {
        BLI_SMALLSTACK_SWAP(edges_search, edges_boundary);
      }
    }
    else {
      /* one boundary and no wire */
      return false;
    }
  }
  else {
    e_pair[1] = BLI_SMALLSTACK_POP(edges_wire);
    if (edges_wire_len > 1) {
      BLI_SMALLSTACK_SWAP(edges_search, edges_wire);
    }
  }

  /* if we swapped above, search this list for the best edge */
  if (!BLI_SMALLSTACK_IS_EMPTY(edges_search)) {
    /* find the best edge in 'edge_list' to use for 'e_pair[1]' */
    const BMVert *v_prev = BM_edge_other_vert(e_pair[0], v_init);
    const BMVert *v_next = BM_edge_other_vert(e_pair[1], v_init);

    float dir_prev[2], dir_next[2];

    normalize_v2_m3_v3v3(dir_prev, face_normal_matrix, v_prev->co, v_init->co);
    normalize_v2_m3_v3v3(dir_next, face_normal_matrix, v_next->co, v_init->co);
    float angle_best_cos = dot_v2v2(dir_next, dir_prev);

    BMEdge *e;
    while ((e = BLI_SMALLSTACK_POP(edges_search))) {
      v_next = BM_edge_other_vert(e, v_init);
      float dir_test[2];

      normalize_v2_m3_v3v3(dir_test, face_normal_matrix, v_next->co, v_init->co);
      const float angle_test_cos = dot_v2v2(dir_prev, dir_test);

      if (angle_test_cos > angle_best_cos) {
        angle_best_cos = angle_test_cos;
        e_pair[1] = e;
      }
    }
  }

  /* flip based on winding */
  l_walk = bm_edge_flagged_radial_first(e_pair[0]);
  swap = false;
  if (face_normal == l_walk->f->no) {
    swap = !swap;
  }
  if (l_walk->v != v_init) {
    swap = !swap;
  }
  if (swap) {
    SWAP(BMEdge *, e_pair[0], e_pair[1]);
  }

  return true;
}

/**
 * A reduced version of #bm_face_split_edgenet_find_loop_pair
 * that only checks if it would return true.
 *
 * \note There is no use in caching resulting edges here,
 * since between this check and running #bm_face_split_edgenet_find_loop,
 * the selected edges may have had faces attached.
 */
static bool bm_face_split_edgenet_find_loop_pair_exists(BMVert *v_init)
{
  int edges_boundary_len = 0;
  int edges_wire_len = 0;

  {
    BMEdge *e, *e_first;
    e = e_first = v_init->e;
    do {
      if (BM_ELEM_API_FLAG_TEST(e, EDGE_NET)) {
        const uint count = bm_edge_flagged_radial_count(e);
        if (count == 1) {
          edges_boundary_len++;
        }
        else if (count == 0) {
          edges_wire_len++;
        }
      }
    } while ((e = BM_DISK_EDGE_NEXT(e, v_init)) != e_first);
  }

  /* first edge should always be boundary */
  if (edges_boundary_len == 0) {
    return false;
  }

  /* attempt one boundary and one wire, or 2 boundary */
  if (edges_wire_len == 0) {
    if (edges_boundary_len >= 2) {
      /* pass */
    }
    else {
      /* one boundary and no wire */
      return false;
    }
  }
  else {
    /* pass */
  }

  return true;
}

static bool bm_face_split_edgenet_find_loop_walk(BMVert *v_init,
                                                 const float face_normal[3],
                                                 /* cache to avoid realloc every time */
                                                 struct VertOrder *edge_order,
                                                 const uint edge_order_len,
                                                 BMEdge *e_pair[2])
{
  /* fast-path for the common case (avoid push-pop).
   * Also avoids tagging as visited since we know we
   * can't reach these verts some other way */
#define USE_FASTPATH_NOFORK

  BMVert *v;
  BMVert *v_dst;
  bool found = false;

  struct VertOrder *eo;
  STACK_DECLARE(edge_order);

  /* store visited verts so we can clear the visit flag after execution */
  BLI_SMALLSTACK_DECLARE(vert_visit, BMVert *);

  /* likely this will stay very small
   * all verts pushed into this stack _must_ have their previous edges set! */
  BLI_SMALLSTACK_DECLARE(vert_stack, BMVert *);
  BLI_SMALLSTACK_DECLARE(vert_stack_next, BMVert *);

  STACK_INIT(edge_order, edge_order_len);

  /* start stepping */
  v = BM_edge_other_vert(e_pair[0], v_init);
  v->e = e_pair[0];
  BLI_SMALLSTACK_PUSH(vert_stack, v);

  v_dst = BM_edge_other_vert(e_pair[1], v_init);

#ifdef DEBUG_PRINT
  printf("%s: vert (search) %d\n", __func__, BM_elem_index_get(v_init));
#endif

  /* This loop will keep stepping over the best possible edge,
   * in most cases it finds the direct route to close the face.
   *
   * In cases where paths can't be closed,
   * alternatives are stored in the 'vert_stack'.
   */
  while ((v = BLI_SMALLSTACK_POP_EX(vert_stack, vert_stack_next))) {
#ifdef USE_FASTPATH_NOFORK
  walk_nofork:
#else
    BLI_SMALLSTACK_PUSH(vert_visit, v);
    BM_ELEM_API_FLAG_ENABLE(v, VERT_VISIT);
#endif

    BLI_assert(STACK_SIZE(edge_order) == 0);

    /* check if we're done! */
    if (v == v_dst) {
      found = true;
      goto finally;
    }

    BMEdge *e_next, *e_first;
    e_first = v->e;
    e_next = BM_DISK_EDGE_NEXT(e_first, v); /* always skip this verts edge */

    /* in rare cases there may be edges with a single connecting vertex */
    if (e_next != e_first) {
      do {
        if ((BM_ELEM_API_FLAG_TEST(e_next, EDGE_NET)) &&
            (bm_edge_flagged_radial_count(e_next) < 2)) {
          BMVert *v_next;

          v_next = BM_edge_other_vert(e_next, v);
          BLI_assert(v->e != e_next);

#ifdef DEBUG_PRINT
          /* indent and print */
          {
            BMVert *_v = v;
            do {
              printf("  ");
            } while ((_v = BM_edge_other_vert(_v->e, _v)) != v_init);
            printf("vert %d -> %d (add=%d)\n",
                   BM_elem_index_get(v),
                   BM_elem_index_get(v_next),
                   BM_ELEM_API_FLAG_TEST(v_next, VERT_VISIT) == 0);
          }
#endif

          if (!BM_ELEM_API_FLAG_TEST(v_next, VERT_VISIT)) {
            eo = STACK_PUSH_RET_PTR(edge_order);
            eo->v = v_next;

            v_next->e = e_next;
          }
        }
      } while ((e_next = BM_DISK_EDGE_NEXT(e_next, v)) != e_first);
    }

#ifdef USE_FASTPATH_NOFORK
    if (STACK_SIZE(edge_order) == 1) {
      eo = STACK_POP_PTR(edge_order);
      v = eo->v;

      goto walk_nofork;
    }
#endif

    /* sort by angle if needed */
    if (STACK_SIZE(edge_order) > 1) {
      uint j;
      BMVert *v_prev = BM_edge_other_vert(v->e, v);

      for (j = 0; j < STACK_SIZE(edge_order); j++) {
        edge_order[j].angle = angle_signed_on_axis_v3v3v3_v3(
            v_prev->co, v->co, edge_order[j].v->co, face_normal);
      }
      qsort(edge_order,
            STACK_SIZE(edge_order),
            sizeof(struct VertOrder),
            BLI_sortutil_cmp_float_reverse);

#ifdef USE_FASTPATH_NOFORK
      /* only tag forks */
      BLI_SMALLSTACK_PUSH(vert_visit, v);
      BM_ELEM_API_FLAG_ENABLE(v, VERT_VISIT);
#endif
    }

    while ((eo = STACK_POP_PTR(edge_order))) {
      BLI_SMALLSTACK_PUSH(vert_stack_next, eo->v);
    }

    if (!BLI_SMALLSTACK_IS_EMPTY(vert_stack_next)) {
      BLI_SMALLSTACK_SWAP(vert_stack, vert_stack_next);
    }
  }

finally:
  /* clear flag for next execution */
  while ((v = BLI_SMALLSTACK_POP(vert_visit))) {
    BM_ELEM_API_FLAG_DISABLE(v, VERT_VISIT);
  }

  return found;

#undef USE_FASTPATH_NOFORK
}

static bool bm_face_split_edgenet_find_loop(BMVert *v_init,
                                            const float face_normal[3],
                                            float face_normal_matrix[3][3],
                                            /* cache to avoid realloc every time */
                                            struct VertOrder *edge_order,
                                            const uint edge_order_len,
                                            BMVert **r_face_verts,
                                            int *r_face_verts_len)
{
  BMEdge *e_pair[2];
  BMVert *v;

  if (!bm_face_split_edgenet_find_loop_pair(v_init, face_normal, face_normal_matrix, e_pair)) {
    return false;
  }

  BLI_assert((bm_edge_flagged_radial_count(e_pair[0]) == 1) ||
             (bm_edge_flagged_radial_count(e_pair[1]) == 1));

  if (bm_face_split_edgenet_find_loop_walk(
          v_init, face_normal, edge_order, edge_order_len, e_pair)) {
    uint i = 0;

    r_face_verts[i++] = v_init;
    v = BM_edge_other_vert(e_pair[1], v_init);
    do {
      r_face_verts[i++] = v;
    } while ((v = BM_edge_other_vert(v->e, v)) != v_init);
    *r_face_verts_len = i;
    return (i > 2) ? true : false;
  }
  else {
    return false;
  }
}

/**
 * Splits a face into many smaller faces defined by an edge-net.
 * handle customdata and degenerate cases.
 *
 * - isolated holes or unsupported face configurations, will be ignored.
 * - customdata calculations aren't efficient
 *   (need to calculate weights for each vert).
 */
bool BM_face_split_edgenet(BMesh *bm,
                           BMFace *f,
                           BMEdge **edge_net,
                           const int edge_net_len,
                           BMFace ***r_face_arr,
                           int *r_face_arr_len)
{
  /* re-use for new face verts */
  BMVert **face_verts;
  int face_verts_len;

  BMFace **face_arr = NULL;
  BLI_array_declare(face_arr);

  BMVert **vert_queue;
  STACK_DECLARE(vert_queue);
  int i;

  struct VertOrder *edge_order;
  const uint edge_order_len = edge_net_len + 2;

  BMVert *v;

  BMLoop *l_iter, *l_first;

  if (!edge_net_len) {
    if (r_face_arr) {
      *r_face_arr = NULL;
      *r_face_arr_len = 0;
    }
    return false;
  }

  /* These arrays used to be stack memory, however they can be
   * large for singe faces with complex edgenets, see: T65980. */

  /* over-alloc (probably 2-4 is only used in most cases), for the biggest-fan */
  edge_order = MEM_mallocN(sizeof(*edge_order) * edge_order_len, __func__);

  /* use later */
  face_verts = MEM_mallocN(sizeof(*face_verts) * (edge_net_len + f->len), __func__);

  vert_queue = MEM_mallocN(sizeof(vert_queue) * (edge_net_len + f->len), __func__);
  STACK_INIT(vert_queue, f->len + edge_net_len);

  BLI_assert(BM_ELEM_API_FLAG_TEST(f, FACE_NET) == 0);
  BM_ELEM_API_FLAG_ENABLE(f, FACE_NET);

#ifdef DEBUG
  for (i = 0; i < edge_net_len; i++) {
    BLI_assert(BM_ELEM_API_FLAG_TEST(edge_net[i], EDGE_NET) == 0);
    BLI_assert(BM_edge_in_face(edge_net[i], f) == false);
  }
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BLI_assert(BM_ELEM_API_FLAG_TEST(l_iter->e, EDGE_NET) == 0);
  } while ((l_iter = l_iter->next) != l_first);
#endif

  /* Note: 'VERT_IN_QUEUE' is often not needed at all,
   * however in rare cases verts are added multiple times to the queue,
   * that on it's own is harmless but in _very_ rare cases,
   * the queue will overflow its maximum size,
   * so we better be strict about this! see: T51539 */

  for (i = 0; i < edge_net_len; i++) {
    BM_ELEM_API_FLAG_ENABLE(edge_net[i], EDGE_NET);
    BM_ELEM_API_FLAG_DISABLE(edge_net[i]->v1, VERT_IN_QUEUE);
    BM_ELEM_API_FLAG_DISABLE(edge_net[i]->v2, VERT_IN_QUEUE);
  }
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BM_ELEM_API_FLAG_ENABLE(l_iter->e, EDGE_NET);
    BM_ELEM_API_FLAG_DISABLE(l_iter->v, VERT_IN_QUEUE);
  } while ((l_iter = l_iter->next) != l_first);

  float face_normal_matrix[3][3];
  axis_dominant_v3_to_m3(face_normal_matrix, f->no);

  /* any vert can be used to begin with */
  STACK_PUSH(vert_queue, l_first->v);
  BM_ELEM_API_FLAG_ENABLE(l_first->v, VERT_IN_QUEUE);

  while ((v = STACK_POP(vert_queue))) {
    BM_ELEM_API_FLAG_DISABLE(v, VERT_IN_QUEUE);
    if (bm_face_split_edgenet_find_loop(v,
                                        f->no,
                                        face_normal_matrix,
                                        edge_order,
                                        edge_order_len,
                                        face_verts,
                                        &face_verts_len)) {
      BMFace *f_new;

      f_new = BM_face_create_verts(bm, face_verts, face_verts_len, f, BM_CREATE_NOP, false);

      for (i = 0; i < edge_net_len; i++) {
        BLI_assert(BM_ELEM_API_FLAG_TEST(edge_net[i], EDGE_NET));
      }

      if (f_new) {
        BLI_array_append(face_arr, f_new);
        copy_v3_v3(f_new->no, f->no);

        /* warning, normally don't do this,
         * its needed for mesh intersection - which tracks face-sides based on selection */
        f_new->head.hflag = f->head.hflag;
        if (f->head.hflag & BM_ELEM_SELECT) {
          bm->totfacesel++;
        }

        BM_ELEM_API_FLAG_ENABLE(f_new, FACE_NET);

        /* add new verts to keep finding loops for
         * (verts between boundary and manifold edges) */
        l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
        do {
          /* Avoid adding to queue multiple times (not common but happens). */
          if (!BM_ELEM_API_FLAG_TEST(l_iter->v, VERT_IN_QUEUE) &&
              bm_face_split_edgenet_find_loop_pair_exists(l_iter->v)) {
            STACK_PUSH(vert_queue, l_iter->v);
            BM_ELEM_API_FLAG_ENABLE(l_iter->v, VERT_IN_QUEUE);
          }
        } while ((l_iter = l_iter->next) != l_first);
      }
    }
  }

  if (CustomData_has_math(&bm->ldata)) {
    /* reuse VERT_VISIT here to tag vert's already interpolated */
    BMIter iter;
    BMLoop *l_other;

    /* see: #BM_loop_interp_from_face for similar logic  */
    void **blocks = BLI_array_alloca(blocks, f->len);
    float(*cos_2d)[2] = BLI_array_alloca(cos_2d, f->len);
    float *w = BLI_array_alloca(w, f->len);
    float axis_mat[3][3];
    float co[2];

    /* interior loops */
    axis_dominant_v3_to_m3(axis_mat, f->no);

    /* first simply copy from existing face */
    i = 0;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_ITER_ELEM (l_other, &iter, l_iter->v, BM_LOOPS_OF_VERT) {
        if ((l_other->f != f) && BM_ELEM_API_FLAG_TEST(l_other->f, FACE_NET)) {
          CustomData_bmesh_copy_data(
              &bm->ldata, &bm->ldata, l_iter->head.data, &l_other->head.data);
        }
      }
      /* tag not to interpolate */
      BM_ELEM_API_FLAG_ENABLE(l_iter->v, VERT_VISIT);

      mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
      blocks[i] = l_iter->head.data;

    } while ((void)i++, (l_iter = l_iter->next) != l_first);

    for (i = 0; i < edge_net_len; i++) {
      BM_ITER_ELEM (v, &iter, edge_net[i], BM_VERTS_OF_EDGE) {
        if (!BM_ELEM_API_FLAG_TEST(v, VERT_VISIT)) {
          BMIter liter;

          BM_ELEM_API_FLAG_ENABLE(v, VERT_VISIT);

          /* interpolate this loop, then copy to the rest */
          l_first = NULL;

          BM_ITER_ELEM (l_iter, &liter, v, BM_LOOPS_OF_VERT) {
            if (BM_ELEM_API_FLAG_TEST(l_iter->f, FACE_NET)) {
              if (l_first == NULL) {
                mul_v2_m3v3(co, axis_mat, v->co);
                interp_weights_poly_v2(w, cos_2d, f->len, co);
                CustomData_bmesh_interp(
                    &bm->ldata, (const void **)blocks, w, NULL, f->len, l_iter->head.data);
                l_first = l_iter;
              }
              else {
                CustomData_bmesh_copy_data(
                    &bm->ldata, &bm->ldata, l_first->head.data, &l_iter->head.data);
              }
            }
          }
        }
      }
    }
  }

  /* cleanup */
  for (i = 0; i < edge_net_len; i++) {
    BM_ELEM_API_FLAG_DISABLE(edge_net[i], EDGE_NET);
    /* from interp only */
    BM_ELEM_API_FLAG_DISABLE(edge_net[i]->v1, VERT_VISIT);
    BM_ELEM_API_FLAG_DISABLE(edge_net[i]->v2, VERT_VISIT);
  }
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BM_ELEM_API_FLAG_DISABLE(l_iter->e, EDGE_NET);
    /* from interp only */
    BM_ELEM_API_FLAG_DISABLE(l_iter->v, VERT_VISIT);
  } while ((l_iter = l_iter->next) != l_first);

  if (BLI_array_len(face_arr)) {
    bmesh_face_swap_data(f, face_arr[0]);
    BM_face_kill(bm, face_arr[0]);
    face_arr[0] = f;
  }
  else {
    BM_ELEM_API_FLAG_DISABLE(f, FACE_NET);
  }

  for (i = 0; i < BLI_array_len(face_arr); i++) {
    BM_ELEM_API_FLAG_DISABLE(face_arr[i], FACE_NET);
  }

  if (r_face_arr) {
    *r_face_arr = face_arr;
    *r_face_arr_len = BLI_array_len(face_arr);
  }
  else {
    if (face_arr) {
      MEM_freeN(face_arr);
    }
  }

  MEM_freeN(edge_order);
  MEM_freeN(face_verts);
  MEM_freeN(vert_queue);

  return true;
}

#undef FACE_NET
#undef VERT_VISIT
#undef EDGE_NET

/** \} */

/* -------------------------------------------------------------------- */
/* Face Split Edge-Net Connect Islands */

/** \name BM_face_split_edgenet_connect_islands and helper functions.
 *
 * Connect isolated mesh 'islands' so they form legal regions from which we can create faces.
 *
 * Intended to be used as a pre-processing step for #BM_face_split_edgenet.
 *
 * \warning Currently this risks running out of stack memory (#alloca),
 * likely we'll pass in a memory arena (cleared each use) eventually.
 *
 * \{ */

#define USE_PARTIAL_CONNECT

#define VERT_IS_VALID BM_ELEM_INTERNAL_TAG

/* can be X or Y */
#define SORT_AXIS 0

BLI_INLINE bool edge_isect_verts_point_2d(const BMEdge *e,
                                          const BMVert *v_a,
                                          const BMVert *v_b,
                                          float r_isect[2])
{
  /* This bias seems like it could be too large,
   * mostly its not needed, see T52329 for example where it is. */
  const float endpoint_bias = 1e-4f;
  return ((isect_seg_seg_v2_point_ex(
               v_a->co, v_b->co, e->v1->co, e->v2->co, endpoint_bias, r_isect) == 1) &&
          ((e->v1 != v_a) && (e->v2 != v_a) && (e->v1 != v_b) && (e->v2 != v_b)));
}

BLI_INLINE int axis_pt_cmp(const float pt_a[2], const float pt_b[2])
{
  if (pt_a[0] < pt_b[0]) {
    return -1;
  }
  if (pt_a[0] > pt_b[0]) {
    return 1;
  }
  if (pt_a[1] < pt_b[1]) {
    return -1;
  }
  if (pt_a[1] > pt_b[1]) {
    return 1;
  }
  return 0;
}

/**
 * Represents isolated edge-links,
 * each island owns contiguous slices of the vert array.
 * (edges remain in `edge_links`).
 */
struct EdgeGroupIsland {
  LinkNode edge_links; /* keep first */
  uint vert_len, edge_len;

  /* Set the following vars once we have >1 groups */

  /* when an edge in a previous group connects to this one,
   * so theres no need to create one pointing back. */
  uint has_prev_edge : 1;

  /* verts in the group which has the lowest & highest values,
   * the lower vertex is connected to the first edge */
  struct {
    BMVert *min, *max;
    /* used for sorting only */
    float min_axis[2];
    float max_axis[2];
  } vert_span;
};

static int group_min_cmp_fn(const void *p1, const void *p2)
{
  const struct EdgeGroupIsland *g1 = *(struct EdgeGroupIsland **)p1;
  const struct EdgeGroupIsland *g2 = *(struct EdgeGroupIsland **)p2;
  /* min->co[SORT_AXIS] hasn't been applied yet */
  int test = axis_pt_cmp(g1->vert_span.min_axis, g2->vert_span.min_axis);
  if (UNLIKELY(test == 0)) {
    test = axis_pt_cmp(g1->vert_span.max_axis, g2->vert_span.max_axis);
  }
  return test;
}

struct Edges_VertVert_BVHTreeTest {
  float dist_orig;
  BMEdge **edge_arr;

  BMVert *v_origin;
  BMVert *v_other;

  const uint *vert_range;
};

struct Edges_VertRay_BVHTreeTest {
  BMEdge **edge_arr;

  BMVert *v_origin;

  const uint *vert_range;
};

static void bvhtree_test_edges_isect_2d_vert_cb(void *user_data,
                                                int index,
                                                const BVHTreeRay *UNUSED(ray),
                                                BVHTreeRayHit *hit)
{
  struct Edges_VertVert_BVHTreeTest *data = user_data;
  const BMEdge *e = data->edge_arr[index];
  const int v1_index = BM_elem_index_get(e->v1);
  float co_isect[2];

  if (edge_isect_verts_point_2d(e, data->v_origin, data->v_other, co_isect)) {
    const float t = line_point_factor_v2(co_isect, data->v_origin->co, data->v_other->co);
    const float dist_new = data->dist_orig * t;
    /* avoid float precision issues, possible this is greater,
     * check above zero to allow some overlap
     * (and needed for partial-connect which will overlap vertices) */
    if (LIKELY((dist_new < hit->dist) && (dist_new > 0.0f))) {
      /* v1/v2 will both be in the same group */
      if (v1_index < (int)data->vert_range[0] || v1_index >= (int)data->vert_range[1]) {
        hit->dist = dist_new;
        hit->index = index;
      }
    }
  }
}

static void bvhtree_test_edges_isect_2d_ray_cb(void *user_data,
                                               int index,
                                               const BVHTreeRay *ray,
                                               BVHTreeRayHit *hit)
{
  struct Edges_VertRay_BVHTreeTest *data = user_data;
  const BMEdge *e = data->edge_arr[index];

  /* direction is normalized, so this will be the distance */
  float dist_new;
  if (isect_ray_seg_v2(
          data->v_origin->co, ray->direction, e->v1->co, e->v2->co, &dist_new, NULL)) {
    /* avoid float precision issues, possible this is greater,
     * check above zero to allow some overlap
     * (and needed for partial-connect which will overlap vertices) */
    if (LIKELY(dist_new < hit->dist && (dist_new > 0.0f))) {
      if (e->v1 != data->v_origin && e->v2 != data->v_origin) {
        const int v1_index = BM_elem_index_get(e->v1);
        /* v1/v2 will both be in the same group */
        if (v1_index < (int)data->vert_range[0] || v1_index >= (int)data->vert_range[1]) {
          hit->dist = dist_new;
          hit->index = index;
        }
      }
    }
  }
}

/**
 * Store values for:
 * - #bm_face_split_edgenet_find_connection
 * - #test_edges_isect_2d_vert
 * ... which don't change each call.
 */
struct EdgeGroup_FindConnection_Args {
  BVHTree *bvhtree;
  BMEdge **edge_arr;
  uint edge_arr_len;

  BMEdge **edge_arr_new;
  uint edge_arr_new_len;

  const uint *vert_range;
};

static BMEdge *test_edges_isect_2d_vert(const struct EdgeGroup_FindConnection_Args *args,
                                        BMVert *v_origin,
                                        BMVert *v_other)
{
  int index;

  BVHTreeRayHit hit = {0};
  float dir[3];

  sub_v2_v2v2(dir, v_other->co, v_origin->co);
  dir[2] = 0.0f;
  hit.index = -1;
  hit.dist = normalize_v2(dir);

  struct Edges_VertVert_BVHTreeTest user_data = {0};
  user_data.dist_orig = hit.dist;
  user_data.edge_arr = args->edge_arr;
  user_data.v_origin = v_origin;
  user_data.v_other = v_other;
  user_data.vert_range = args->vert_range;

  index = BLI_bvhtree_ray_cast_ex(args->bvhtree,
                                  v_origin->co,
                                  dir,
                                  0.0f,
                                  &hit,
                                  bvhtree_test_edges_isect_2d_vert_cb,
                                  &user_data,
                                  0);

  BMEdge *e_hit = (index != -1) ? args->edge_arr[index] : NULL;

  /* check existing connections (no spatial optimization here since we're continually adding). */
  if (LIKELY(index == -1)) {
    float t_best = 1.0f;
    for (uint i = 0; i < args->edge_arr_new_len; i++) {
      float co_isect[2];
      if (UNLIKELY(
              edge_isect_verts_point_2d(args->edge_arr_new[i], v_origin, v_other, co_isect))) {
        const float t_test = line_point_factor_v2(co_isect, v_origin->co, v_other->co);
        if (t_test < t_best) {
          t_best = t_test;

          e_hit = args->edge_arr_new[i];
        }
      }
    }
  }

  return e_hit;
}

/**
 * Similar to #test_edges_isect_2d_vert but we're casting into a direction,
 * (not to a vertex)
 */
static BMEdge *test_edges_isect_2d_ray(const struct EdgeGroup_FindConnection_Args *args,
                                       BMVert *v_origin,
                                       const float dir[3])
{
  int index;
  BVHTreeRayHit hit = {0};

  BLI_ASSERT_UNIT_V2(dir);

  hit.index = -1;
  hit.dist = BVH_RAYCAST_DIST_MAX;

  struct Edges_VertRay_BVHTreeTest user_data = {0};
  user_data.edge_arr = args->edge_arr;
  user_data.v_origin = v_origin;
  user_data.vert_range = args->vert_range;

  index = BLI_bvhtree_ray_cast_ex(args->bvhtree,
                                  v_origin->co,
                                  dir,
                                  0.0f,
                                  &hit,
                                  bvhtree_test_edges_isect_2d_ray_cb,
                                  &user_data,
                                  0);

  BMEdge *e_hit = (index != -1) ? args->edge_arr[index] : NULL;

  /* check existing connections (no spatial optimization here since we're continually adding). */
  if (LIKELY(index != -1)) {
    for (uint i = 0; i < args->edge_arr_new_len; i++) {
      BMEdge *e = args->edge_arr_new[i];
      float dist_new;
      if (isect_ray_seg_v2(v_origin->co, dir, e->v1->co, e->v2->co, &dist_new, NULL)) {
        if (e->v1 != v_origin && e->v2 != v_origin) {
          /* avoid float precision issues, possible this is greater */
          if (LIKELY(dist_new < hit.dist)) {
            hit.dist = dist_new;

            e_hit = args->edge_arr_new[i];
          }
        }
      }
    }
  }

  return e_hit;
}

static int bm_face_split_edgenet_find_connection(const struct EdgeGroup_FindConnection_Args *args,
                                                 BMVert *v_origin,
                                                 /* false = negative, true = positive */
                                                 bool direction_sign)
{
  /**
   * Method for finding connection is as follows:
   *
   * - Cast a ray along either the positive or negative directions.
   * - Take the hit-edge, and cast rays to their vertices
   *   checking those rays don't intersect a closer edge.
   * - Keep taking the hit-edge and testing its verts
   *   until a vertex is found which isn't blocked by an edge.
   *
   * \note It's possible none of the verts can be accessed (with self-intersecting lines).
   * In that case theres no right answer (without subdividing edges),
   * so return a fall-back vertex in that case.
   */

  const float dir[3] = {[SORT_AXIS] = direction_sign ? 1.0f : -1.0f};
  BMEdge *e_hit = test_edges_isect_2d_ray(args, v_origin, dir);
  BMVert *v_other = NULL;

  if (e_hit) {
    BMVert *v_other_fallback = NULL;

    BLI_SMALLSTACK_DECLARE(vert_search, BMVert *);

    /* ensure we never add verts multiple times (not all that likely - but possible) */
    BLI_SMALLSTACK_DECLARE(vert_blacklist, BMVert *);

    do {
      BMVert *v_pair[2];
      /* ensure the closest vertex is popped back off the stack first */
      if (len_squared_v2v2(v_origin->co, e_hit->v1->co) >
          len_squared_v2v2(v_origin->co, e_hit->v2->co)) {
        ARRAY_SET_ITEMS(v_pair, e_hit->v1, e_hit->v2);
      }
      else {
        ARRAY_SET_ITEMS(v_pair, e_hit->v2, e_hit->v1);
      }

      for (int j = 0; j < 2; j++) {
        BMVert *v_iter = v_pair[j];
        if (BM_elem_flag_test(v_iter, VERT_IS_VALID)) {
          if (direction_sign ? (v_iter->co[SORT_AXIS] > v_origin->co[SORT_AXIS]) :
                               (v_iter->co[SORT_AXIS] < v_origin->co[SORT_AXIS])) {
            BLI_SMALLSTACK_PUSH(vert_search, v_iter);
            BLI_SMALLSTACK_PUSH(vert_blacklist, v_iter);
            BM_elem_flag_disable(v_iter, VERT_IS_VALID);
          }
        }
      }
      v_other_fallback = v_other;

    } while ((v_other = BLI_SMALLSTACK_POP(vert_search)) &&
             (e_hit = test_edges_isect_2d_vert(args, v_origin, v_other)));

    if (v_other == NULL) {
      printf("Using fallback\n");
      v_other = v_other_fallback;
    }

    /* reset the blacklist flag, for future use */
    BMVert *v;
    while ((v = BLI_SMALLSTACK_POP(vert_blacklist))) {
      BM_elem_flag_enable(v, VERT_IS_VALID);
    }
  }

  /* if we reach this line, v_other is either the best vertex or its NULL */
  return v_other ? BM_elem_index_get(v_other) : -1;
}

/**
 * Support for connecting islands that have single-edge connections.
 * This options is not very optimal (however its not needed for booleans either).
 */
#ifdef USE_PARTIAL_CONNECT

/**
 * Used to identify edges that  get split off when making island from partial connection.
 * fptr should be a BMFace*, but is a void* for general interface to BM_vert_separate_tested_edges
 */
static bool test_tagged_and_notface(BMEdge *e, void *fptr)
{
  BMFace *f = (BMFace *)fptr;
  return BM_elem_flag_test(e, BM_ELEM_INTERNAL_TAG) && !BM_edge_in_face(e, f);
}

/**
 * Split vertices which are part of a partial connection
 * (only a single vertex connecting an island).
 *
 * \note All edges and vertices must have their #BM_ELEM_INTERNAL_TAG flag enabled.
 * This function leaves all the flags set as well.
 */
static BMVert *bm_face_split_edgenet_partial_connect(BMesh *bm, BMVert *v_delimit, BMFace *f)
{
  /* -------------------------------------------------------------------- */
  /* Initial check that we may be a delimiting vert (keep this fast) */

  /* initial check - see if we have 3+ flagged edges attached to 'v_delimit'
   * if not, we can early exit */
  LinkNode *e_delimit_list = NULL;
  uint e_delimit_list_len = 0;

#  define EDGE_NOT_IN_STACK BM_ELEM_INTERNAL_TAG
#  define VERT_NOT_IN_STACK BM_ELEM_INTERNAL_TAG

#  define FOREACH_VERT_EDGE(v_, e_, body_) \
    { \
      BMEdge *e_ = v_->e; \
      do { \
        body_ \
      } while ((e_ = BM_DISK_EDGE_NEXT(e_, v_)) != v_->e); \
    } \
    ((void)0)

  /* start with face edges, since we need to split away wire-only edges */
  BMEdge *e_face_init = NULL;

  FOREACH_VERT_EDGE(v_delimit, e_iter, {
    if (BM_elem_flag_test(e_iter, EDGE_NOT_IN_STACK)) {
      BLI_assert(BM_elem_flag_test(BM_edge_other_vert(e_iter, v_delimit), VERT_NOT_IN_STACK));
      BLI_linklist_prepend_alloca(&e_delimit_list, e_iter);
      e_delimit_list_len++;
      if (e_iter->l != NULL && BM_edge_in_face(e_iter, f)) {
        e_face_init = e_iter;
      }
    }
  });

  /* skip typical edge-chain verts */
  if (LIKELY(e_delimit_list_len <= 2)) {
    return NULL;
  }

  /* -------------------------------------------------------------------- */
  /* Complicated stuff starts now! */

  /* Store connected vertices for restoring the flag */
  LinkNode *vert_stack = NULL;
  BLI_linklist_prepend_alloca(&vert_stack, v_delimit);
  BM_elem_flag_disable(v_delimit, VERT_NOT_IN_STACK);

  /* Walk the net... */
  {
    BLI_SMALLSTACK_DECLARE(search, BMVert *);
    BMVert *v_other = BM_edge_other_vert(e_face_init ? e_face_init : v_delimit->e, v_delimit);

    BLI_SMALLSTACK_PUSH(search, v_other);
    BM_elem_flag_disable(v_other, VERT_NOT_IN_STACK);

    while ((v_other = BLI_SMALLSTACK_POP(search))) {
      BLI_assert(BM_elem_flag_test(v_other, VERT_NOT_IN_STACK) == false);
      BLI_linklist_prepend_alloca(&vert_stack, v_other);
      BMEdge *e_iter = v_other->e;
      do {
        BMVert *v_step = BM_edge_other_vert(e_iter, v_other);
        if (BM_elem_flag_test(e_iter, EDGE_NOT_IN_STACK)) {
          if (BM_elem_flag_test(v_step, VERT_NOT_IN_STACK)) {
            BM_elem_flag_disable(v_step, VERT_NOT_IN_STACK);
            BLI_SMALLSTACK_PUSH(search, v_step);
          }
        }
      } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v_other)) != v_other->e);
    }
  }

  /* Detect if this is a delimiter
   * by checking if we didn't walk any of edges connected to 'v_delimit'. */
  bool is_delimit = false;
  FOREACH_VERT_EDGE(v_delimit, e_iter, {
    BMVert *v_step = BM_edge_other_vert(e_iter, v_delimit);
    if (BM_elem_flag_test(v_step, VERT_NOT_IN_STACK) && !BM_edge_in_face(e_iter, f)) {
      is_delimit = true; /* if one vertex is valid - we have a mix */
    }
    else {
      /* match the vertex flag (only for edges around 'v_delimit') */
      BM_elem_flag_disable(e_iter, EDGE_NOT_IN_STACK);
    }
  });

#  undef FOREACH_VERT_EDGE

  /* Execute the split */
  BMVert *v_split = NULL;
  if (is_delimit) {
    v_split = BM_vert_create(bm, v_delimit->co, NULL, 0);
    BM_vert_separate_tested_edges(bm, v_split, v_delimit, test_tagged_and_notface, f);
    BM_elem_flag_enable(v_split, VERT_NOT_IN_STACK);

    BLI_assert(v_delimit->e != NULL);

    /* Degenerate, avoid eternal loop, see: T59074. */
#  if 0
    BLI_assert(v_split->e != NULL);
#  else
    if (UNLIKELY(v_split->e == NULL)) {
      BM_vert_kill(bm, v_split);
      v_split = NULL;
    }
#  endif
  }

  /* Restore flags */
  do {
    BM_elem_flag_enable((BMVert *)vert_stack->link, VERT_NOT_IN_STACK);
  } while ((vert_stack = vert_stack->next));

  do {
    BM_elem_flag_enable((BMEdge *)e_delimit_list->link, EDGE_NOT_IN_STACK);
  } while ((e_delimit_list = e_delimit_list->next));

#  undef EDGE_NOT_IN_STACK
#  undef VERT_NOT_IN_STACK

  return v_split;
}

/**
 * Check if connecting vertices would cause an edge with duplicate verts.
 */
static bool bm_vert_partial_connect_check_overlap(const int *remap,
                                                  const int v_a_index,
                                                  const int v_b_index)
{
  /* connected to eachother */
  if (UNLIKELY((remap[v_a_index] == v_b_index) || (remap[v_b_index] == v_a_index))) {
    return true;
  }
  else {
    return false;
  }
}

#endif /* USE_PARTIAL_CONNECT */

/**
 * For when the edge-net has holes in it-this connects them.
 *
 * \param use_partial_connect: Support for handling islands connected by only a single edge,
 * \note that this is quite slow so avoid using where possible.
 * \param mem_arena: Avoids many small allocs & should be cleared after each use.
 * take care since \a r_edge_net_new is stored in \a r_edge_net_new.
 */
bool BM_face_split_edgenet_connect_islands(BMesh *bm,
                                           BMFace *f,
                                           BMEdge **edge_net_init,
                                           const uint edge_net_init_len,
                                           bool use_partial_connect,
                                           MemArena *mem_arena,
                                           BMEdge ***r_edge_net_new,
                                           uint *r_edge_net_new_len)
{
  /* -------------------------------------------------------------------- */
  /* This function has 2 main parts.
   *
   * - Check if there are any holes.
   * - Connect the holes with edges (if any are found).
   *
   * Keep the first part fast since it will run very often for edge-nets that have no holes.
   *
   * \note Don't use the mem_arena unless he have holes to fill.
   * (avoid thrashing the area when the initial check isn't so intensive on the stack).
   */

  const uint edge_arr_len = (uint)edge_net_init_len + (uint)f->len;
  BMEdge **edge_arr = BLI_memarena_alloc(mem_arena, sizeof(*edge_arr) * edge_arr_len);
  bool ok = false;
  uint edge_net_new_len = (uint)edge_net_init_len;

  memcpy(edge_arr, edge_net_init, sizeof(*edge_arr) * (size_t)edge_net_init_len);

  /* _must_ clear on exit */
#define EDGE_NOT_IN_STACK BM_ELEM_INTERNAL_TAG
#define VERT_NOT_IN_STACK BM_ELEM_INTERNAL_TAG

  {
    uint i = edge_net_init_len;
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BLI_assert(!BM_elem_flag_test(l_iter->v, VERT_NOT_IN_STACK));
      BLI_assert(!BM_elem_flag_test(l_iter->e, EDGE_NOT_IN_STACK));
      edge_arr[i++] = l_iter->e;
    } while ((l_iter = l_iter->next) != l_first);
    BLI_assert(i == edge_arr_len);
  }

  for (uint i = 0; i < edge_arr_len; i++) {
    BM_elem_flag_enable(edge_arr[i], EDGE_NOT_IN_STACK);
    BM_elem_flag_enable(edge_arr[i]->v1, VERT_NOT_IN_STACK);
    BM_elem_flag_enable(edge_arr[i]->v2, VERT_NOT_IN_STACK);
  }

#ifdef USE_PARTIAL_CONNECT
  /* Split-out delimiting vertices */
  struct TempVertPair {
    struct TempVertPair *next;
    BMVert *v_temp;
    BMVert *v_orig;
  };

  struct {
    struct TempVertPair *list;
    uint len;
    int *remap; /* temp -> orig mapping */
  } temp_vert_pairs = {NULL};

  if (use_partial_connect) {
    for (uint i = 0; i < edge_net_init_len; i++) {
      for (unsigned j = 0; j < 2; j++) {
        BMVert *v_delimit = (&edge_arr[i]->v1)[j];
        BMVert *v_other;

        /* note, remapping will _never_ map a vertex to an already mapped vertex */
        while (UNLIKELY((v_other = bm_face_split_edgenet_partial_connect(bm, v_delimit, f)))) {
          struct TempVertPair *tvp = BLI_memarena_alloc(mem_arena, sizeof(*tvp));
          tvp->next = temp_vert_pairs.list;
          tvp->v_orig = v_delimit;
          tvp->v_temp = v_other;
          temp_vert_pairs.list = tvp;
          temp_vert_pairs.len++;
        }
      }
    }

    if (temp_vert_pairs.len == 0) {
      use_partial_connect = false;
    }
  }
#endif /* USE_PARTIAL_CONNECT */

  uint group_arr_len = 0;
  LinkNode *group_head = NULL;
  {
    /* scan 'edge_arr' backwards so the outer face boundary is handled first
     * (since its likely to be the largest) */
    uint edge_index = (edge_arr_len - 1);
    uint edge_in_group_tot = 0;

    BLI_SMALLSTACK_DECLARE(vstack, BMVert *);

    while (true) {
      LinkNode *edge_links = NULL;
      uint unique_verts_in_group = 0, unique_edges_in_group = 0;

      /* list of groups */
      BLI_assert(BM_elem_flag_test(edge_arr[edge_index]->v1, VERT_NOT_IN_STACK));
      BLI_SMALLSTACK_PUSH(vstack, edge_arr[edge_index]->v1);
      BM_elem_flag_disable(edge_arr[edge_index]->v1, VERT_NOT_IN_STACK);

      BMVert *v_iter;
      while ((v_iter = BLI_SMALLSTACK_POP(vstack))) {
        unique_verts_in_group++;

        BMEdge *e_iter = v_iter->e;
        do {
          if (BM_elem_flag_test(e_iter, EDGE_NOT_IN_STACK)) {
            BM_elem_flag_disable(e_iter, EDGE_NOT_IN_STACK);
            unique_edges_in_group++;

            BLI_linklist_prepend_arena(&edge_links, e_iter, mem_arena);

            BMVert *v_other = BM_edge_other_vert(e_iter, v_iter);
            if (BM_elem_flag_test(v_other, VERT_NOT_IN_STACK)) {
              BLI_SMALLSTACK_PUSH(vstack, v_other);
              BM_elem_flag_disable(v_other, VERT_NOT_IN_STACK);
            }
          }
        } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v_iter)) != v_iter->e);
      }

      struct EdgeGroupIsland *g = BLI_memarena_alloc(mem_arena, sizeof(*g));
      g->vert_len = unique_verts_in_group;
      g->edge_len = unique_edges_in_group;
      edge_in_group_tot += unique_edges_in_group;

      BLI_linklist_prepend_nlink(&group_head, edge_links, (LinkNode *)g);

      group_arr_len++;

      if (edge_in_group_tot == edge_arr_len) {
        break;
      }

      /* skip edges in the stack */
      while (BM_elem_flag_test(edge_arr[edge_index], EDGE_NOT_IN_STACK) == false) {
        BLI_assert(edge_index != 0);
        edge_index--;
      }
    }
  }

  /* single group - no holes */
  if (group_arr_len == 1) {
    goto finally;
  }

  /* -------------------------------------------------------------------- */
  /* Previous checks need to be kept fast, since they will run very often,
   * now we know there are holes, so calculate a spatial lookup info and
   * other per-group data.
   */

  float axis_mat[3][3];
  axis_dominant_v3_to_m3(axis_mat, f->no);

#define VERT_IN_ARRAY BM_ELEM_INTERNAL_TAG

  struct EdgeGroupIsland **group_arr = BLI_memarena_alloc(mem_arena,
                                                          sizeof(*group_arr) * group_arr_len);
  uint vert_arr_len = 0;
  /* sort groups by lowest value vertex */
  {
    /* fill 'groups_arr' in reverse order so the boundary face is first */
    struct EdgeGroupIsland **group_arr_p = &group_arr[group_arr_len];

    for (struct EdgeGroupIsland *g = (void *)group_head; g;
         g = (struct EdgeGroupIsland *)g->edge_links.next) {
      LinkNode *edge_links = g->edge_links.link;

      /* init with *any* different verts */
      g->vert_span.min = ((BMEdge *)edge_links->link)->v1;
      g->vert_span.max = ((BMEdge *)edge_links->link)->v2;
      float min_axis[2] = {FLT_MAX, FLT_MAX};
      float max_axis[2] = {-FLT_MAX, -FLT_MAX};

      do {
        BMEdge *e = edge_links->link;
        BLI_assert(e->head.htype == BM_EDGE);

        for (int j = 0; j < 2; j++) {
          BMVert *v_iter = (&e->v1)[j];
          BLI_assert(v_iter->head.htype == BM_VERT);
          /* ideally we could use 'v_iter->co[SORT_AXIS]' here,
           * but we need to sort the groups before setting the vertex array order */
          const float axis_value[2] = {
#if SORT_AXIS == 0
            dot_m3_v3_row_x(axis_mat, v_iter->co),
            dot_m3_v3_row_y(axis_mat, v_iter->co),
#else
            dot_m3_v3_row_y(axis_mat, v_iter->co),
            dot_m3_v3_row_x(axis_mat, v_iter->co),
#endif
          };

          if (axis_pt_cmp(axis_value, min_axis) == -1) {
            g->vert_span.min = v_iter;
            copy_v2_v2(min_axis, axis_value);
          }
          if (axis_pt_cmp(axis_value, max_axis) == 1) {
            g->vert_span.max = v_iter;
            copy_v2_v2(max_axis, axis_value);
          }
        }
      } while ((edge_links = edge_links->next));

      copy_v2_v2(g->vert_span.min_axis, min_axis);
      copy_v2_v2(g->vert_span.max_axis, max_axis);

      g->has_prev_edge = false;

      vert_arr_len += g->vert_len;

      *(--group_arr_p) = g;
    }
  }

  qsort(group_arr, group_arr_len, sizeof(*group_arr), group_min_cmp_fn);

  /* we don't know how many unique verts there are connecting the edges, so over-alloc */
  BMVert **vert_arr = BLI_memarena_alloc(mem_arena, sizeof(*vert_arr) * vert_arr_len);
  /* map vertex -> group index */
  uint *verts_group_table = BLI_memarena_alloc(mem_arena,
                                               sizeof(*verts_group_table) * vert_arr_len);

  float(*vert_coords_backup)[3] = BLI_memarena_alloc(mem_arena,
                                                     sizeof(*vert_coords_backup) * vert_arr_len);

  {
    /* relative location, for higher precision calculations */
    const float f_co_ref[3] = {UNPACK3(BM_FACE_FIRST_LOOP(f)->v->co)};

    int v_index = 0; /* global vert index */
    for (uint g_index = 0; g_index < group_arr_len; g_index++) {
      LinkNode *edge_links = group_arr[g_index]->edge_links.link;
      do {
        BMEdge *e = edge_links->link;
        for (int j = 0; j < 2; j++) {
          BMVert *v_iter = (&e->v1)[j];
          if (!BM_elem_flag_test(v_iter, VERT_IN_ARRAY)) {
            BM_elem_flag_enable(v_iter, VERT_IN_ARRAY);

            /* not nice, but alternatives aren't much better :S */
            {
              copy_v3_v3(vert_coords_backup[v_index], v_iter->co);

              /* for higher precision */
              sub_v3_v3(v_iter->co, f_co_ref);

              float co_2d[2];
              mul_v2_m3v3(co_2d, axis_mat, v_iter->co);
              v_iter->co[0] = co_2d[0];
              v_iter->co[1] = co_2d[1];
              v_iter->co[2] = 0.0f;
            }

            BM_elem_index_set(v_iter, v_index); /* set_dirty */

            vert_arr[v_index] = v_iter;
            verts_group_table[v_index] = g_index;
            v_index++;
          }
        }
      } while ((edge_links = edge_links->next));
    }
  }

  bm->elem_index_dirty |= BM_VERT;

  /* Now create bvh tree
   *
   * Note that a large epsilon is used because meshes with dimensions of around 100+ need it.
   * see T52329. */
  BVHTree *bvhtree = BLI_bvhtree_new(edge_arr_len, 1e-4f, 8, 8);
  for (uint i = 0; i < edge_arr_len; i++) {
    const float e_cos[2][3] = {
        {UNPACK2(edge_arr[i]->v1->co), 0.0f},
        {UNPACK2(edge_arr[i]->v2->co), 0.0f},
    };
    BLI_bvhtree_insert(bvhtree, i, (const float *)e_cos, 2);
  }
  BLI_bvhtree_balance(bvhtree);

#ifdef USE_PARTIAL_CONNECT
  if (use_partial_connect) {
    /* needs to be done once the vertex indices have been written into */
    temp_vert_pairs.remap = BLI_memarena_alloc(mem_arena,
                                               sizeof(*temp_vert_pairs.remap) * vert_arr_len);
    copy_vn_i(temp_vert_pairs.remap, vert_arr_len, -1);

    struct TempVertPair *tvp = temp_vert_pairs.list;
    do {
      temp_vert_pairs.remap[BM_elem_index_get(tvp->v_temp)] = BM_elem_index_get(tvp->v_orig);
    } while ((tvp = tvp->next));
  }
#endif /* USE_PARTIAL_CONNECT */

  /* Create connections between groups */

  /* may be an over-alloc, but not by much */
  edge_net_new_len = (uint)edge_net_init_len + ((group_arr_len - 1) * 2);
  BMEdge **edge_net_new = BLI_memarena_alloc(mem_arena, sizeof(*edge_net_new) * edge_net_new_len);
  memcpy(edge_net_new, edge_net_init, sizeof(*edge_net_new) * (size_t)edge_net_init_len);

  {
    uint edge_net_new_index = edge_net_init_len;
    /* start-end of the verts in the current group */

    uint vert_range[2];

    vert_range[0] = 0;
    vert_range[1] = group_arr[0]->vert_len;

    struct EdgeGroup_FindConnection_Args args = {
        .bvhtree = bvhtree,

        /* use the new edge array so we can scan edges which have been added */
        .edge_arr = edge_arr,
        .edge_arr_len = edge_arr_len,

        /* we only want to check newly created edges */
        .edge_arr_new = edge_net_new + edge_net_init_len,
        .edge_arr_new_len = 0,

        .vert_range = vert_range,
    };

    for (uint g_index = 1; g_index < group_arr_len; g_index++) {
      struct EdgeGroupIsland *g = group_arr[g_index];

      /* the range of verts this group uses in 'verts_arr' (not uncluding the last index) */
      vert_range[0] = vert_range[1];
      vert_range[1] += g->vert_len;

      if (g->has_prev_edge == false) {
        BMVert *v_origin = g->vert_span.min;

        const int index_other = bm_face_split_edgenet_find_connection(&args, v_origin, false);
        // BLI_assert(index_other >= 0 && index_other < (int)vert_arr_len);

        /* only for degenerate geometry */
        if (index_other != -1) {
#ifdef USE_PARTIAL_CONNECT
          if ((use_partial_connect == false) ||
              (bm_vert_partial_connect_check_overlap(
                   temp_vert_pairs.remap, BM_elem_index_get(v_origin), index_other) == false))
#endif
          {
            BMVert *v_end = vert_arr[index_other];

            edge_net_new[edge_net_new_index] = BM_edge_create(bm, v_origin, v_end, NULL, 0);
#ifdef USE_PARTIAL_CONNECT
            BM_elem_index_set(edge_net_new[edge_net_new_index], edge_net_new_index);
#endif
            edge_net_new_index++;
            args.edge_arr_new_len++;
          }
        }
      }

      {
        BMVert *v_origin = g->vert_span.max;

        const int index_other = bm_face_split_edgenet_find_connection(&args, v_origin, true);
        // BLI_assert(index_other >= 0 && index_other < (int)vert_arr_len);

        /* only for degenerate geometry */
        if (index_other != -1) {
#ifdef USE_PARTIAL_CONNECT
          if ((use_partial_connect == false) ||
              (bm_vert_partial_connect_check_overlap(
                   temp_vert_pairs.remap, BM_elem_index_get(v_origin), index_other) == false))
#endif
          {
            BMVert *v_end = vert_arr[index_other];
            edge_net_new[edge_net_new_index] = BM_edge_create(bm, v_origin, v_end, NULL, 0);
#ifdef USE_PARTIAL_CONNECT
            BM_elem_index_set(edge_net_new[edge_net_new_index], edge_net_new_index);
#endif
            edge_net_new_index++;
            args.edge_arr_new_len++;
          }

          /* tell the 'next' group it doesn't need to create its own back-link */
          uint g_index_other = verts_group_table[index_other];
          group_arr[g_index_other]->has_prev_edge = true;
        }
      }
    }
    BLI_assert(edge_net_new_len >= edge_net_new_index);
    edge_net_new_len = edge_net_new_index;
  }

  BLI_bvhtree_free(bvhtree);

  *r_edge_net_new = edge_net_new;
  *r_edge_net_new_len = edge_net_new_len;
  ok = true;

  for (uint i = 0; i < vert_arr_len; i++) {
    copy_v3_v3(vert_arr[i]->co, vert_coords_backup[i]);
  }

finally:

#ifdef USE_PARTIAL_CONNECT
  /* don't free 'vert_temp_pair_list', its part of the arena */
  if (use_partial_connect) {

    /* Sanity check: ensure we don't have connecting edges before splicing begins. */
#  ifdef DEBUG
    {
      struct TempVertPair *tvp = temp_vert_pairs.list;
      do {
        /* we must _never_ create connections here
         * (inface the islands can't have a connection at all) */
        BLI_assert(BM_edge_exists(tvp->v_orig, tvp->v_temp) == NULL);
      } while ((tvp = tvp->next));
    }
#  endif

    struct TempVertPair *tvp = temp_vert_pairs.list;
    do {
      /* its _very_ unlikely the edge exists,
       * however splicing may case this. see: T48012 */
      if (!BM_edge_exists(tvp->v_orig, tvp->v_temp)) {
        BM_vert_splice(bm, tvp->v_orig, tvp->v_temp);
      }
    } while ((tvp = tvp->next));

    /* Remove edges which have become doubles since splicing vertices together,
     * its less trouble then detecting future-doubles on edge-creation. */
    for (uint i = edge_net_init_len; i < edge_net_new_len; i++) {
      while (BM_edge_find_double(edge_net_new[i])) {
        BM_edge_kill(bm, edge_net_new[i]);
        edge_net_new_len--;
        if (i == edge_net_new_len) {
          break;
        }
        edge_net_new[i] = edge_net_new[edge_net_new_len];
      }
    }

    *r_edge_net_new_len = edge_net_new_len;
  }
#endif

  for (uint i = 0; i < edge_arr_len; i++) {
    BM_elem_flag_disable(edge_arr[i], EDGE_NOT_IN_STACK);
    BM_elem_flag_disable(edge_arr[i]->v1, VERT_NOT_IN_STACK);
    BM_elem_flag_disable(edge_arr[i]->v2, VERT_NOT_IN_STACK);
  }

#undef VERT_IN_ARRAY
#undef VERT_NOT_IN_STACK
#undef EDGE_NOT_IN_STACK

  return ok;
}

#undef SORT_AXIS

/** \} */
