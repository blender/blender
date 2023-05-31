/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Find the region defined by the path(s) between 2 UV elements.
 * (path isn't ordered).
 *
 * \note This uses the same behavior as bmesh_path_region.c
 * however walking UVs causes enough differences that it's
 * impractical to share the code.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_utildefines_stack.h"

#include "bmesh.h"
#include "bmesh_path_region_uv.h" /* own include */

/**
 * Special handling of vertices with 2 edges
 * (act as if the edge-chain is a single edge).
 *
 * \note Regarding manifold edge stepping: #BM_vert_is_edge_pair_manifold usage.
 * Logic to skip a chain of vertices is not applied at boundaries because it gives
 * strange behavior from a user perspective especially with boundary quads, see: #52701
 *
 * Restrict walking over a vertex chain to cases where the edges share the same faces.
 * This is more typical of what a user would consider a vertex chain.
 */
#define USE_EDGE_CHAIN

#ifdef USE_EDGE_CHAIN
/**
 * Takes a vertex with 2 edge users and assigns the vertices at each end-point,
 *
 * \return Success when \a l_end_pair values are set or false if the edges loop back on themselves.
 */
static bool bm_loop_pair_ends(BMLoop *l_pivot, BMLoop *l_end_pair[2])
{
  int j;
  for (j = 0; j < 2; j++) {
    BMLoop *l_other = j ? l_pivot->next : l_pivot->prev;
    while (BM_vert_is_edge_pair_manifold(l_other->v)) {
      l_other = j ? l_other->next : l_other->prev;
      if (l_other == l_pivot) {
        return false;
      }
    }
    l_end_pair[j] = l_other;
  }
  BLI_assert(j == 2);
  return true;
}
#endif /* USE_EDGE_CHAIN */

/* -------------------------------------------------------------------- */
/** \name Loop Vertex in Region Checks
 * \{ */

static bool bm_loop_region_test(BMLoop *l, int *const depths[2], const int pass)
{
  const int index = BM_elem_index_get(l);
  return (((depths[0][index] != -1) && (depths[1][index] != -1)) &&
          ((depths[0][index] + depths[1][index]) < pass));
}

#ifdef USE_EDGE_CHAIN
static bool bm_loop_region_test_chain(BMLoop *l, int *const depths[2], const int pass)
{
  BMLoop *l_end_pair[2];
  if (bm_loop_region_test(l, depths, pass)) {
    return true;
  }
  if (BM_vert_is_edge_pair_manifold(l->v) && bm_loop_pair_ends(l, l_end_pair) &&
      bm_loop_region_test(l_end_pair[0], depths, pass) &&
      bm_loop_region_test(l_end_pair[1], depths, pass))
  {
    return true;
  }

  return false;
}
#else
static bool bm_loop_region_test_chain(BMLoop *l, int *const depths[2], const int pass)
{
  return bm_loop_region_test(l, depths, pass);
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
                                            const int cd_loop_uv_offset,
                                            const char path_htype)
{
  BLI_assert(cd_loop_uv_offset >= 0);
  int ele_loops_len[2];
  BMLoop **ele_loops[2];

  /* Get vertices from any `ele_src/ele_dst` elements. */
  for (int side = 0; side < 2; side++) {
    BMElem *ele = side ? ele_dst : ele_src;
    int j = 0;

    if (ele->head.htype == BM_FACE) {
      BMFace *f = (BMFace *)ele;
      ele_loops[side] = BLI_array_alloca(ele_loops[side], f->len);

      BMLoop *l_first, *l_iter;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        ele_loops[side][j++] = l_iter;
      } while ((l_iter = l_iter->next) != l_first);
    }
    else if (ele->head.htype == BM_LOOP) {
      if (path_htype == BM_EDGE) {
        BMLoop *l = (BMLoop *)ele;
        ele_loops[side] = BLI_array_alloca(ele_loops[side], 2);
        ele_loops[side][j++] = l;
        ele_loops[side][j++] = l->next;
      }
      else if (path_htype == BM_VERT) {
        BMLoop *l = (BMLoop *)ele;
        ele_loops[side] = BLI_array_alloca(ele_loops[side], 1);

        ele_loops[side][j++] = l;
      }
      else {
        BLI_assert(0);
      }
    }
    else {
      BLI_assert(0);
    }
    ele_loops_len[side] = j;
  }

  int *depths[2] = {NULL};
  int pass = 0;

  BMLoop **stack = MEM_mallocN(sizeof(*stack) * bm->totloop, __func__);
  BMLoop **stack_other = MEM_mallocN(sizeof(*stack_other) * bm->totloop, __func__);

  STACK_DECLARE(stack);
  STACK_INIT(stack, bm->totloop);

  STACK_DECLARE(stack_other);
  STACK_INIT(stack_other, bm->totloop);

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  /* After exhausting all possible elements, we should have found all elements on the 'side_other'.
   * otherwise, exit early. */
  bool found_all = false;

  for (int side = 0; side < 2; side++) {
    const int side_other = !side;

    /* initialize depths to -1 (un-touched), fill in with the depth as we walk over the edges. */
    depths[side] = MEM_mallocN(sizeof(*depths[side]) * bm->totloop, __func__);
    copy_vn_i(depths[side], bm->totloop, -1);

    /* needed for second side */
    STACK_CLEAR(stack);
    STACK_CLEAR(stack_other);

    for (int i = 0; i < ele_loops_len[side]; i++) {
      BMLoop *l = ele_loops[side][i];
      depths[side][BM_elem_index_get(l)] = 0;
      if (!BM_elem_flag_test(l, BM_ELEM_TAG)) {
        STACK_PUSH(stack, l);
      }
    }

#ifdef USE_EDGE_CHAIN
    /* Expand initial state to end-point vertices when they only have 2x edges,
     * this prevents odd behavior when source or destination are in the middle
     * of a long chain of edges. */
    if (ELEM(path_htype, BM_VERT, BM_EDGE)) {
      for (int i = 0; i < ele_loops_len[side]; i++) {
        BMLoop *l = ele_loops[side][i];
        BMLoop *l_end_pair[2];
        if (BM_vert_is_edge_pair_manifold(l->v) && bm_loop_pair_ends(l, l_end_pair)) {
          for (int j = 0; j < 2; j++) {
            const int l_end_index = BM_elem_index_get(l_end_pair[j]);
            if (depths[side][l_end_index] == -1) {
              depths[side][l_end_index] = 0;
              if (!BM_elem_flag_test(l_end_pair[j], BM_ELEM_TAG)) {
                STACK_PUSH(stack, l_end_pair[j]);
              }
            }
          }
        }
      }
    }
#endif /* USE_EDGE_CHAIN */

    /* Keep walking over connected geometry until we find all the vertices in
     * `ele_loops[side_other]`, or exit the loop when there's no connection. */
    found_all = false;
    for (pass = 1; (STACK_SIZE(stack) != 0); pass++) {
      while (STACK_SIZE(stack) != 0) {
        BMLoop *l_a = STACK_POP(stack);
        const int l_a_index = BM_elem_index_get(l_a);

        BMIter iter;
        BMLoop *l_iter;

        BM_ITER_ELEM (l_iter, &iter, l_a->v, BM_LOOPS_OF_VERT) {
          if (BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
            continue;
          }
          if (!BM_loop_uv_share_vert_check(l_a, l_iter, cd_loop_uv_offset)) {
            continue;
          }

          /* Flush the depth to connected loops (only needed for UVs). */
          if (depths[side][BM_elem_index_get(l_iter)] == -1) {
            depths[side][BM_elem_index_get(l_iter)] = depths[side][l_a_index];
          }

          for (int j = 0; j < 2; j++) {
            BMLoop *l_b = j ? l_iter->next : l_iter->prev;
            int l_b_index = BM_elem_index_get(l_b);
            if (depths[side][l_b_index] == -1) {

#ifdef USE_EDGE_CHAIN
              /* Walk along the chain, fill in values until we reach a vertex with 3+ edges. */
              {
                while (BM_vert_is_edge_pair_manifold(l_b->v) &&
                       ((depths[side][l_b_index] == -1) &&
                        /* Don't walk back to the beginning */
                        (l_b != (j ? l_iter->prev : l_iter->next))))
                {
                  depths[side][l_b_index] = pass;

                  l_b = j ? l_b->next : l_b->prev;
                  l_b_index = BM_elem_index_get(l_b);
                }
              }
#endif /* USE_EDGE_CHAIN */

              /* Add the other vertex to the stack, to be traversed in the next pass. */
              if (depths[side][l_b_index] == -1) {
#ifdef USE_EDGE_CHAIN
                BLI_assert(!BM_vert_is_edge_pair_manifold(l_b->v));
#endif
                BLI_assert(pass == depths[side][BM_elem_index_get(l_a)] + 1);
                depths[side][l_b_index] = pass;
                if (!BM_elem_flag_test(l_b, BM_ELEM_TAG)) {
                  STACK_PUSH(stack_other, l_b);
                }
              }
            }
          }
        }
      }

      /* Stop searching once there's none left.
       * Note that this looks in-efficient, however until the target elements reached,
       * it will exit immediately.
       * After that, it takes as many passes as the element has edges to finish off. */
      found_all = true;
      for (int i = 0; i < ele_loops_len[side_other]; i++) {
        if (depths[side][BM_elem_index_get(ele_loops[side_other][i])] == -1) {
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
          if (!bm_loop_region_test_chain(l_iter->v, depths, pass)) {
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
          if (!bm_loop_region_test_chain(l_iter, depths, pass)) {
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
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BMIter liter;
      BMLoop *l;
      /* Check the current and next loop vertices are in the region. */
      bool l_in_chain_next = bm_loop_region_test_chain(BM_FACE_FIRST_LOOP(f), depths, pass);
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        const bool l_in_chain = l_in_chain_next;
        l_in_chain_next = bm_loop_region_test_chain(l->next, depths, pass);
        if (l_in_chain && l_in_chain_next) {
          BLI_linklist_prepend(&path, l);
        }
      }
    }
  }
  else if (path_htype == BM_VERT) {
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BMIter liter;
      BMLoop *l;
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (bm_loop_region_test_chain(l, depths, pass)) {
          BLI_linklist_prepend(&path, l);
        }
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

/* -------------------------------------------------------------------- */
/** \name Main Functions (exposed externally).
 * \{ */

LinkNode *BM_mesh_calc_path_uv_region_vert(BMesh *bm,
                                           BMElem *ele_src,
                                           BMElem *ele_dst,
                                           const int cd_loop_uv_offset,
                                           bool (*filter_fn)(BMLoop *, void *user_data),
                                           void *user_data)
{
  LinkNode *path = NULL;
  /* BM_ELEM_TAG flag is used to store visited verts */
  BMFace *f;
  BMIter fiter;
  int i = 0;

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l;
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      BM_elem_flag_set(l, BM_ELEM_TAG, !filter_fn(l, user_data));
      BM_elem_index_set(l, i); /* set_inline */
      i += 1;
    }
  }
  bm->elem_index_dirty &= ~BM_LOOP;

  path = mesh_calc_path_region_elem(bm, ele_src, ele_dst, cd_loop_uv_offset, BM_VERT);

  return path;
}

LinkNode *BM_mesh_calc_path_uv_region_edge(BMesh *bm,
                                           BMElem *ele_src,
                                           BMElem *ele_dst,
                                           const int cd_loop_uv_offset,
                                           bool (*filter_fn)(BMLoop *, void *user_data),
                                           void *user_data)
{
  LinkNode *path = NULL;
  /* BM_ELEM_TAG flag is used to store visited verts */
  BMFace *f;
  BMIter fiter;
  int i = 0;

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l;
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      BM_elem_flag_set(l, BM_ELEM_TAG, !filter_fn(l, user_data));
      BM_elem_index_set(l, i); /* set_inline */
      i += 1;
    }
  }
  bm->elem_index_dirty &= ~BM_LOOP;

  path = mesh_calc_path_region_elem(bm, ele_src, ele_dst, cd_loop_uv_offset, BM_EDGE);

  return path;
}

LinkNode *BM_mesh_calc_path_uv_region_face(BMesh *bm,
                                           BMElem *ele_src,
                                           BMElem *ele_dst,
                                           const int cd_loop_uv_offset,
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

  path = mesh_calc_path_region_elem(bm, ele_src, ele_dst, cd_loop_uv_offset, BM_FACE);

  return path;
}

/** \} */
