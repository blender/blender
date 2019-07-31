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
 * Find the region defined by the path(s) between 2 elements.
 * (path isn't ordered).
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines_stack.h"
#include "BLI_alloca.h"

#include "bmesh.h"
#include "bmesh_path_region.h" /* own include */

/**
 * Special handling of vertices with 2 edges
 * (act as if the edge-chain is a single edge).
 *
 * \note Regarding manifold edge stepping: #BM_vert_is_edge_pair_manifold usage.
 * Logic to skip a chain of vertices is not applied at boundaries because it gives
 * strange behavior from a user perspective especially with boundary quads, see: T52701
 *
 * Restrict walking over a vertex chain to cases where the edges share the same faces.
 * This is more typical of what a user would consider a vertex chain.
 */
#define USE_EDGE_CHAIN

#ifdef USE_EDGE_CHAIN
/**
 * Takes a vertex with 2 edge users and assigns the vertices at each end-point,
 *
 * \return Success when \a v_end_pair values are set or false if the edges loop back on themselves.
 */
static bool bm_vert_pair_ends(BMVert *v_pivot, BMVert *v_end_pair[2])
{
  BMEdge *e = v_pivot->e;
  int j = 0;
  do {
    BMEdge *e_chain = e;
    BMVert *v_other = BM_edge_other_vert(e_chain, v_pivot);
    while (BM_vert_is_edge_pair_manifold(v_other)) {
      BMEdge *e_chain_next = BM_DISK_EDGE_NEXT(e_chain, v_other);
      BLI_assert(BM_DISK_EDGE_NEXT(e_chain_next, v_other) == e_chain);
      v_other = BM_edge_other_vert(e_chain_next, v_other);
      if (v_other == v_pivot) {
        return false;
      }
      e_chain = e_chain_next;
    }
    v_end_pair[j++] = v_other;
  } while ((e = BM_DISK_EDGE_NEXT(e, v_pivot)) != v_pivot->e);

  BLI_assert(j == 2);
  return true;
}
#endif /* USE_EDGE_CHAIN */

/** \name Vertex in Region Checks
 * \{ */

static bool bm_vert_region_test(BMVert *v, int *const depths[2], const int pass)
{
  const int index = BM_elem_index_get(v);
  return (((depths[0][index] != -1) && (depths[1][index] != -1)) &&
          ((depths[0][index] + depths[1][index]) < pass));
}

#ifdef USE_EDGE_CHAIN
static bool bm_vert_region_test_chain(BMVert *v, int *const depths[2], const int pass)
{
  BMVert *v_end_pair[2];
  if (bm_vert_region_test(v, depths, pass)) {
    return true;
  }
  else if (BM_vert_is_edge_pair_manifold(v) && bm_vert_pair_ends(v, v_end_pair) &&
           bm_vert_region_test(v_end_pair[0], depths, pass) &&
           bm_vert_region_test(v_end_pair[1], depths, pass)) {
    return true;
  }

  return false;
}
#else
static bool bm_vert_region_test_chain(BMVert *v, int *const depths[2], const int pass)
{
  return bm_vert_region_test(v, depths, pass);
}
#endif

/** \} */

/**
 * Main logic for calculating region between 2 elements.
 *
 * This method works walking (breadth first) over all vertices,
 * keeping track of topological distance from the source.
 *
 * This is done in both directions, after that each vertices 'depth' is added to check
 * if its less than the number of passes needed to complete the search.
 * When it is, we know the path is one of possible paths
 * that have the minimum topological distance.
 *
 * \note Only verts without BM_ELEM_TAG will be walked over.
 */
static LinkNode *mesh_calc_path_region_elem(BMesh *bm,
                                            BMElem *ele_src,
                                            BMElem *ele_dst,
                                            const char path_htype)
{
  int ele_verts_len[2];
  BMVert **ele_verts[2];

  /* Get vertices from any `ele_src/ele_dst` elements. */
  for (int side = 0; side < 2; side++) {
    BMElem *ele = side ? ele_dst : ele_src;
    int j = 0;

    if (ele->head.htype == BM_FACE) {
      BMFace *f = (BMFace *)ele;
      ele_verts[side] = BLI_array_alloca(ele_verts[side], f->len);

      BMLoop *l_first, *l_iter;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        ele_verts[side][j++] = l_iter->v;
      } while ((l_iter = l_iter->next) != l_first);
    }
    else if (ele->head.htype == BM_EDGE) {
      BMEdge *e = (BMEdge *)ele;
      ele_verts[side] = BLI_array_alloca(ele_verts[side], 2);

      ele_verts[side][j++] = e->v1;
      ele_verts[side][j++] = e->v2;
    }
    else if (ele->head.htype == BM_VERT) {
      BMVert *v = (BMVert *)ele;
      ele_verts[side] = BLI_array_alloca(ele_verts[side], 1);

      ele_verts[side][j++] = v;
    }
    else {
      BLI_assert(0);
    }
    ele_verts_len[side] = j;
  }

  int *depths[2] = {NULL};
  int pass = 0;

  BMVert **stack = MEM_mallocN(sizeof(*stack) * bm->totvert, __func__);
  BMVert **stack_other = MEM_mallocN(sizeof(*stack_other) * bm->totvert, __func__);

  STACK_DECLARE(stack);
  STACK_INIT(stack, bm->totvert);

  STACK_DECLARE(stack_other);
  STACK_INIT(stack_other, bm->totvert);

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  /* After exhausting all possible elements, we should have found all elements on the 'side_other'.
   * otherwise, exit early. */
  bool found_all = false;

  for (int side = 0; side < 2; side++) {
    const int side_other = !side;

    /* initialize depths to -1 (un-touched), fill in with the depth as we walk over the edges. */
    depths[side] = MEM_mallocN(sizeof(*depths[side]) * bm->totvert, __func__);
    copy_vn_i(depths[side], bm->totvert, -1);

    /* needed for second side */
    STACK_CLEAR(stack);
    STACK_CLEAR(stack_other);

    for (int i = 0; i < ele_verts_len[side]; i++) {
      BMVert *v = ele_verts[side][i];
      depths[side][BM_elem_index_get(v)] = 0;
      if (v->e && !BM_elem_flag_test(v, BM_ELEM_TAG)) {
        STACK_PUSH(stack, v);
      }
    }

#ifdef USE_EDGE_CHAIN
    /* Expand initial state to end-point vertices when they only have 2x edges,
     * this prevents odd behavior when source or destination are in the middle
     * of a long chain of edges. */
    if (ELEM(path_htype, BM_VERT, BM_EDGE)) {
      for (int i = 0; i < ele_verts_len[side]; i++) {
        BMVert *v = ele_verts[side][i];
        BMVert *v_end_pair[2];
        if (BM_vert_is_edge_pair_manifold(v) && bm_vert_pair_ends(v, v_end_pair)) {
          for (int j = 0; j < 2; j++) {
            const int v_end_index = BM_elem_index_get(v_end_pair[j]);
            if (depths[side][v_end_index] == -1) {
              depths[side][v_end_index] = 0;
              if (!BM_elem_flag_test(v_end_pair[j], BM_ELEM_TAG)) {
                STACK_PUSH(stack, v_end_pair[j]);
              }
            }
          }
        }
      }
    }
#endif /* USE_EDGE_CHAIN */

    /* Keep walking over connected geometry until we find all the vertices in
     * `ele_verts[side_other]`, or exit the loop when there's no connection. */
    found_all = false;
    for (pass = 1; (STACK_SIZE(stack) != 0); pass++) {
      while (STACK_SIZE(stack) != 0) {
        BMVert *v_a = STACK_POP(stack);
        // const int v_a_index = BM_elem_index_get(v_a);  /* only for assert */
        BMEdge *e = v_a->e;

        do {
          BMVert *v_b = BM_edge_other_vert(e, v_a);
          int v_b_index = BM_elem_index_get(v_b);
          if (depths[side][v_b_index] == -1) {

#ifdef USE_EDGE_CHAIN
            /* Walk along the chain, fill in values until we reach a vertex with 3+ edges. */
            {
              BMEdge *e_chain = e;
              while (BM_vert_is_edge_pair_manifold(v_b) && ((depths[side][v_b_index] == -1))) {
                depths[side][v_b_index] = pass;

                BMEdge *e_chain_next = BM_DISK_EDGE_NEXT(e_chain, v_b);
                BLI_assert(BM_DISK_EDGE_NEXT(e_chain_next, v_b) == e_chain);
                v_b = BM_edge_other_vert(e_chain_next, v_b);
                v_b_index = BM_elem_index_get(v_b);
                e_chain = e_chain_next;
              }
            }
#endif /* USE_EDGE_CHAIN */

            /* Add the other vertex to the stack, to be traversed in the next pass. */
            if (depths[side][v_b_index] == -1) {
#ifdef USE_EDGE_CHAIN
              BLI_assert(!BM_vert_is_edge_pair_manifold(v_b));
#endif
              BLI_assert(pass == depths[side][BM_elem_index_get(v_a)] + 1);
              depths[side][v_b_index] = pass;
              if (!BM_elem_flag_test(v_b, BM_ELEM_TAG)) {
                STACK_PUSH(stack_other, v_b);
              }
            }
          }
        } while ((e = BM_DISK_EDGE_NEXT(e, v_a)) != v_a->e);
      }

      /* Stop searching once there's none left.
       * Note that this looks in-efficient, however until the target elements reached,
       * it will exit immediately.
       * After that, it takes as many passes as the element has edges to finish off. */
      found_all = true;
      for (int i = 0; i < ele_verts_len[side_other]; i++) {
        if (depths[side][BM_elem_index_get(ele_verts[side_other][i])] == -1) {
          found_all = false;
          break;
        }
      }
      if (found_all == true) {
        pass++;
        break;
      }

      STACK_SWAP(stack, stack_other);
    }

    /* if we have nothing left, and didn't find all elements on the other side,
     * exit early and don't continue */
    if (found_all == false) {
      break;
    }
  }

  MEM_freeN(stack);
  MEM_freeN(stack_other);

  /* Now we have depths recorded from both sides,
   * select elements that use tagged verts. */
  LinkNode *path = NULL;

  if (found_all == false) {
    /* fail! (do nothing) */
  }
  else if (path_htype == BM_FACE) {
    BMIter fiter;
    BMFace *f;

    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
        /* check all verts in face are tagged */
        BMLoop *l_first, *l_iter;
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        bool ok = true;
#if 0
        do {
          if (!bm_vert_region_test_chain(l_iter->v, depths, pass)) {
            ok = false;
            break;
          }
        } while ((l_iter = l_iter->next) != l_first);
#else
        /* Allowing a single failure on a face gives fewer 'gaps'.
         * While correct, in practice they're often part of what
         * a user would consider the 'region'. */
        int ok_tests = f->len > 3 ? 1 : 0; /* how many times we may fail */
        do {
          if (!bm_vert_region_test_chain(l_iter->v, depths, pass)) {
            if (ok_tests == 0) {
              ok = false;
              break;
            }
            ok_tests--;
          }
        } while ((l_iter = l_iter->next) != l_first);
#endif

        if (ok) {
          BLI_linklist_prepend(&path, f);
        }
      }
    }
  }
  else if (path_htype == BM_EDGE) {
    BMIter eiter;
    BMEdge *e;

    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
        /* check all verts in edge are tagged */
        bool ok = true;
        for (int j = 0; j < 2; j++) {
          if (!bm_vert_region_test_chain(*((&e->v1) + j), depths, pass)) {
            ok = false;
            break;
          }
        }

        if (ok) {
          BLI_linklist_prepend(&path, e);
        }
      }
    }
  }
  else if (path_htype == BM_VERT) {
    BMIter viter;
    BMVert *v;

    BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
      if (bm_vert_region_test_chain(v, depths, pass)) {
        BLI_linklist_prepend(&path, v);
      }
    }
  }

  for (int side = 0; side < 2; side++) {
    if (depths[side]) {
      MEM_freeN(depths[side]);
    }
  }

  return path;
}

#undef USE_EDGE_CHAIN

/** \name Main Functions (exposed externally).
 * \{ */

LinkNode *BM_mesh_calc_path_region_vert(BMesh *bm,
                                        BMElem *ele_src,
                                        BMElem *ele_dst,
                                        bool (*filter_fn)(BMVert *, void *user_data),
                                        void *user_data)
{
  LinkNode *path = NULL;
  /* BM_ELEM_TAG flag is used to store visited verts */
  BMVert *v;
  BMIter viter;
  int i;

  BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
    BM_elem_flag_set(v, BM_ELEM_TAG, !filter_fn(v, user_data));
    BM_elem_index_set(v, i); /* set_inline */
  }
  bm->elem_index_dirty &= ~BM_VERT;

  path = mesh_calc_path_region_elem(bm, ele_src, ele_dst, BM_VERT);

  return path;
}

LinkNode *BM_mesh_calc_path_region_edge(BMesh *bm,
                                        BMElem *ele_src,
                                        BMElem *ele_dst,
                                        bool (*filter_fn)(BMEdge *, void *user_data),
                                        void *user_data)
{
  LinkNode *path = NULL;
  /* BM_ELEM_TAG flag is used to store visited verts */
  BMEdge *e;
  BMIter eiter;
  int i;

  /* flush flag to verts */
  BM_mesh_elem_hflag_enable_all(bm, BM_VERT, BM_ELEM_TAG, false);

  BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
    bool test;
    BM_elem_flag_set(e, BM_ELEM_TAG, test = !filter_fn(e, user_data));

    /* flush tag to verts */
    if (test == false) {
      for (int j = 0; j < 2; j++) {
        BM_elem_flag_disable(*((&e->v1) + j), BM_ELEM_TAG);
      }
    }
  }

  path = mesh_calc_path_region_elem(bm, ele_src, ele_dst, BM_EDGE);

  return path;
}

LinkNode *BM_mesh_calc_path_region_face(BMesh *bm,
                                        BMElem *ele_src,
                                        BMElem *ele_dst,
                                        bool (*filter_fn)(BMFace *, void *user_data),
                                        void *user_data)
{
  LinkNode *path = NULL;
  /* BM_ELEM_TAG flag is used to store visited verts */
  BMFace *f;
  BMIter fiter;
  int i;

  /* flush flag to verts */
  BM_mesh_elem_hflag_enable_all(bm, BM_VERT, BM_ELEM_TAG, false);

  BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, i) {
    bool test;
    BM_elem_flag_set(f, BM_ELEM_TAG, test = !filter_fn(f, user_data));

    /* flush tag to verts */
    if (test == false) {
      BMLoop *l_first, *l_iter;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BM_elem_flag_disable(l_iter->v, BM_ELEM_TAG);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  path = mesh_calc_path_region_elem(bm, ele_src, ele_dst, BM_FACE);

  return path;
}

/** \} */
