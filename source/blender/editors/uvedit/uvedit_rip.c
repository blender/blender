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
 * \ingroup eduv
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_uvedit.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

/* -------------------------------------------------------------------- */
/** \name UV Loop Rip Data Struct
 * \{ */

/** Unordered loop data, stored in #BMLoop.head.index. */
typedef struct ULData {
  /** When this UV is selected as well as the next UV. */
  uint is_select_edge : 1;
  /**
   * When only this UV is selected and none of the other UV's
   * around the connected fan are attached to an edge.
   *
   * In this case there is no need to detect contiguous loops,
   * each isolated case is handled on it's own, no need to walk over selected edges.
   *
   * \note This flag isn't flushed to other loops which could also have this enabled.
   * Currently it's not necessary since we can start off on any one of these loops,
   * then walk onto the other loops around the uv-fan, without having the flag to be
   * set on all loops.
   */
  uint is_select_vert_single : 1;
  /** This could be a face-tag. */
  uint is_select_all : 1;
  /** Use when building the rip-pairs stack. */
  uint in_stack : 1;
  /** Set once this has been added into a #UVRipPairs. */
  uint in_rip_pairs : 1;
  /** The side this loop is part of. */
  uint side : 1;
  /**
   * Paranoid check to ensure we don't enter eternal loop swapping sides,
   * this could happen with float precision error, making a swap to measure as slightly better
   * depending on the order of addition.
   */
  uint side_was_swapped : 1;
} ULData;

/** Ensure this fits in an int (loop index). */
BLI_STATIC_ASSERT(sizeof(ULData) <= sizeof(int), "");

BLI_INLINE ULData *UL(BMLoop *l)
{
  return (ULData *)&l->head.index;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Utilities
 * \{ */

static BMLoop *bm_loop_find_other_radial_loop_with_visible_face(BMLoop *l_src,
                                                                const int cd_loop_uv_offset)
{
  BMLoop *l_other = NULL;
  BMLoop *l_iter = l_src->radial_next;
  if (l_iter != l_src) {
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG) && UL(l_iter)->is_select_edge &&
          BM_loop_uv_share_edge_check(l_src, l_iter, cd_loop_uv_offset)) {
        /* Check UV's are contiguous. */
        if (l_other == NULL) {
          l_other = l_iter;
        }
        else {
          /* Only use when there is a single alternative. */
          l_other = NULL;
          break;
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_src);
  }
  return l_other;
}

static BMLoop *bm_loop_find_other_fan_loop_with_visible_face(BMLoop *l_src,
                                                             BMVert *v_src,
                                                             const int cd_loop_uv_offset)
{
  BLI_assert(BM_vert_in_edge(l_src->e, v_src));
  BMLoop *l_other = NULL;
  BMLoop *l_iter = l_src->radial_next;
  if (l_iter != l_src) {
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG) &&
          BM_loop_uv_share_edge_check(l_src, l_iter, cd_loop_uv_offset)) {
        /* Check UV's are contiguous. */
        if (l_other == NULL) {
          l_other = l_iter;
        }
        else {
          /* Only use when there is a single alternative. */
          l_other = NULL;
          break;
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_src);
  }
  if (l_other != NULL) {
    if (l_other->v == v_src) {
      /* do nothing. */
    }
    else if (l_other->next->v == v_src) {
      l_other = l_other->next;
    }
    else if (l_other->prev->v == v_src) {
      l_other = l_other->prev;
    }
    else {
      BLI_assert(0);
    }
  }
  return l_other;
}

/**
 * A version of #BM_vert_step_fan_loop that checks UV's.
 */
static BMLoop *bm_vert_step_fan_loop_uv(BMLoop *l, BMEdge **e_step, const int cd_loop_uv_offset)
{
  BMEdge *e_prev = *e_step;
  BMLoop *l_next;
  if (l->e == e_prev) {
    l_next = l->prev;
  }
  else if (l->prev->e == e_prev) {
    l_next = l;
  }
  else {
    BLI_assert(0);
    return NULL;
  }

  *e_step = l_next->e;

  return bm_loop_find_other_fan_loop_with_visible_face(l_next, l->v, cd_loop_uv_offset);
}

static void bm_loop_uv_select_single_vert_validate(BMLoop *l_init, const int cd_loop_uv_offset)
{
  const MLoopUV *luv_init = BM_ELEM_CD_GET_VOID_P(l_init, cd_loop_uv_offset);
  BMIter liter;
  BMLoop *l;
  bool is_single_vert = true;
  BM_ITER_ELEM (l, &liter, l_init->v, BM_LOOPS_OF_VERT) {
    const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    if (equals_v2v2(luv_init->uv, luv->uv)) {
      if (UL(l->prev)->is_select_edge || UL(l)->is_select_edge) {
        is_single_vert = false;
        break;
      }
    }
  }
  if (is_single_vert == false) {
    BM_ITER_ELEM (l, &liter, l_init->v, BM_LOOPS_OF_VERT) {
      if (UL(l)->is_select_vert_single) {
        const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (equals_v2v2(luv_init->uv, luv->uv)) {
          UL(l)->is_select_vert_single = false;
        }
      }
    }
  }
}

/**
 * The corner return values calculate the angle between both loops,
 * the edge values pick the closest to the either edge (ignoring the center).
 *
 * \param dir: Direction to calculate the angle to (normalized and aspect corrected).
 */
static void bm_loop_calc_uv_angle_from_dir(BMLoop *l,
                                           const float dir[2],
                                           const float aspect_y,
                                           const int cd_loop_uv_offset,
                                           float *r_corner_angle,
                                           float *r_edge_angle,
                                           int *r_edge_index)
{
  /* Calculate 3 directions, return the shortest angle. */
  float dir_test[3][2];
  const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  const MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset);
  const MLoopUV *luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

  sub_v2_v2v2(dir_test[0], luv->uv, luv_prev->uv);
  sub_v2_v2v2(dir_test[2], luv->uv, luv_next->uv);
  dir_test[0][1] /= aspect_y;
  dir_test[2][1] /= aspect_y;

  normalize_v2(dir_test[0]);
  normalize_v2(dir_test[2]);

  /* Calculate the orthogonal line (same as negating one, then adding). */
  sub_v2_v2v2(dir_test[1], dir_test[0], dir_test[2]);
  normalize_v2(dir_test[1]);

  /* Rotate 90 degrees. */
  SWAP(float, dir_test[1][0], dir_test[1][1]);
  dir_test[1][1] *= -1.0f;

  if (BM_face_uv_calc_cross(l->f, cd_loop_uv_offset) > 0.0f) {
    negate_v2(dir_test[1]);
  }

  const float angles[3] = {
      angle_v2v2(dir, dir_test[0]),
      angle_v2v2(dir, dir_test[1]),
      angle_v2v2(dir, dir_test[2]),
  };

  /* Set the corner values. */
  *r_corner_angle = angles[1];

  /* Set the edge values. */
  if (angles[0] < angles[2]) {
    *r_edge_angle = angles[0];
    *r_edge_index = -1;
  }
  else {
    *r_edge_angle = angles[2];
    *r_edge_index = 1;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Rip Single
 * \{ */

typedef struct UVRipSingle {
  /** Walk around the selected UV point, store #BMLoop. */
  GSet *loops;
} UVRipSingle;

/**
 * Handle single loop, the following cases:
 *
 * - An isolated fan, without a shared UV edge to other fans which share the same coordinate,
 *   in this case we just need to pick the closest fan to \a co.
 *
 * - In the case of contiguous loops (part of the same fan).
 *   Rip away the loops connected to the closest edge.
 *
 * - In the case of 2 contiguous loops.
 *   Rip the closest loop away.
 *
 * \note This matches the behavior of edit-mesh rip tool.
 */
static UVRipSingle *uv_rip_single_from_loop(BMLoop *l_init_orig,
                                            const float co[2],
                                            const float aspect_y,
                                            const int cd_loop_uv_offset)
{
  UVRipSingle *rip = MEM_callocN(sizeof(*rip), __func__);
  const float *co_center =
      (((const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_init_orig, cd_loop_uv_offset))->uv);
  rip->loops = BLI_gset_ptr_new(__func__);

  /* Track the closest loop, start walking from this so in the event we have multiple
   * disconnected fans, we can rip away loops connected to this one. */
  BMLoop *l_init = NULL;
  BMLoop *l_init_edge = NULL;
  float corner_angle_best = FLT_MAX;
  float edge_angle_best = FLT_MAX;
  int edge_index_best = 0; /* -1 or +1 (never center). */

  /* Calculate the direction from the cursor with aspect correction. */
  float dir_co[2];
  sub_v2_v2v2(dir_co, co_center, co);
  dir_co[1] /= aspect_y;
  if (UNLIKELY(normalize_v2(dir_co) == 0.0)) {
    dir_co[1] = 1.0f;
  }

  int uv_fan_count_all = 0;
  {
    BMIter liter;
    BMLoop *l;
    BM_ITER_ELEM (l, &liter, l_init_orig->v, BM_LOOPS_OF_VERT) {
      if (BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
        const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (equals_v2v2(co_center, luv->uv)) {
          uv_fan_count_all += 1;
          /* Clear at the same time. */
          UL(l)->is_select_vert_single = true;
          UL(l)->side = 0;
          BLI_gset_add(rip->loops, l);

          /* Update `l_init_close` */
          float corner_angle_test;
          float edge_angle_test;
          int edge_index_test;
          bm_loop_calc_uv_angle_from_dir(l,
                                         dir_co,
                                         aspect_y,
                                         cd_loop_uv_offset,
                                         &corner_angle_test,
                                         &edge_angle_test,
                                         &edge_index_test);
          if ((corner_angle_best == FLT_MAX) || (corner_angle_test < corner_angle_best)) {
            corner_angle_best = corner_angle_test;
            l_init = l;
          }

          /* Trick so we don't consider concave corners further away than they should be. */
          edge_angle_test = min_ff(corner_angle_test, edge_angle_test);

          if ((edge_angle_best == FLT_MAX) || (edge_angle_test < edge_angle_best)) {
            edge_angle_best = edge_angle_test;
            edge_index_best = edge_index_test;
            l_init_edge = l;
          }
        }
      }
    }
  }

  /* Walk around the `l_init` in both directions of the UV fan. */
  int uv_fan_count_contiguous = 1;
  UL(l_init)->side = 1;
  for (int i = 0; i < 2; i += 1) {
    BMEdge *e_prev = i ? l_init->e : l_init->prev->e;
    BMLoop *l_iter = l_init;
    while (((l_iter = bm_vert_step_fan_loop_uv(l_iter, &e_prev, cd_loop_uv_offset)) != l_init) &&
           (l_iter != NULL) && (UL(l_iter)->side == 0)) {
      uv_fan_count_contiguous += 1;
      /* Keep. */
      UL(l_iter)->side = 1;
    }
    /* May be useful to know if the fan is closed, currently it's not needed. */
#if 0
    if (l_iter == l_init) {
      is_closed = true;
    }
#endif
  }

  if (uv_fan_count_contiguous != uv_fan_count_all) {
    /* Simply rip off the current fan, all tagging is done. */
  }
  else {
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, rip->loops) {
      BMLoop *l = BLI_gsetIterator_getKey(&gs_iter);
      UL(l)->side = 0;
    }

    if (uv_fan_count_contiguous <= 2) {
      /* Simple case, rip away the closest loop. */
      UL(l_init)->side = 1;
    }
    else {
      /* Rip away from the closest edge. */
      BMLoop *l_radial_init = (edge_index_best == -1) ? l_init_edge->prev : l_init_edge;
      BMLoop *l_radial_iter = l_radial_init;
      do {
        if (BM_loop_uv_share_edge_check(l_radial_init, l_radial_iter, cd_loop_uv_offset)) {
          BMLoop *l = (l_radial_iter->v == l_init->v) ? l_radial_iter : l_radial_iter->next;
          BLI_assert(l->v == l_init->v);
          /* Keep. */
          UL(l)->side = 1;
        }
      } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_init);
    }
  }

  return rip;
}

static void uv_rip_single_free(UVRipSingle *rip)
{
  BLI_gset_free(rip->loops, NULL);
  MEM_freeN(rip);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Rip Loop Pairs
 * \{ */

typedef struct UVRipPairs {
  /** Walk along the UV selection, store #BMLoop. */
  GSet *loops;
} UVRipPairs;

static void uv_rip_pairs_add(UVRipPairs *rip, BMLoop *l)
{
  ULData *ul = UL(l);
  BLI_assert(!BLI_gset_haskey(rip->loops, l));
  BLI_assert(ul->in_rip_pairs == false);
  ul->in_rip_pairs = true;
  BLI_gset_add(rip->loops, l);
}

static void uv_rip_pairs_remove(UVRipPairs *rip, BMLoop *l)
{
  ULData *ul = UL(l);
  BLI_assert(BLI_gset_haskey(rip->loops, l));
  BLI_assert(ul->in_rip_pairs == true);
  ul->in_rip_pairs = false;
  BLI_gset_remove(rip->loops, l, NULL);
}

/**
 * \note While this isn't especially efficient,
 * this is only needed for rip-pairs end-points (only two per contiguous selection loop).
 */
static float uv_rip_pairs_calc_uv_angle(BMLoop *l_init,
                                        uint side,
                                        const float aspect_y,
                                        const int cd_loop_uv_offset)
{
  BMIter liter;
  const MLoopUV *luv_init = BM_ELEM_CD_GET_VOID_P(l_init, cd_loop_uv_offset);
  float angle_of_side = 0.0f;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, l_init->v, BM_LOOPS_OF_VERT) {
    if (UL(l)->in_rip_pairs) {
      if (UL(l)->side == side) {
        const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (equals_v2v2(luv_init->uv, luv->uv)) {
          const MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset);
          const MLoopUV *luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
          float dir_prev[2], dir_next[2];
          sub_v2_v2v2(dir_prev, luv_prev->uv, luv->uv);
          sub_v2_v2v2(dir_next, luv_next->uv, luv->uv);
          dir_prev[1] /= aspect_y;
          dir_next[1] /= aspect_y;
          const float luv_angle = angle_v2v2(dir_prev, dir_next);
          if (LIKELY(isfinite(luv_angle))) {
            angle_of_side += luv_angle;
          }
        }
      }
    }
  }
  return angle_of_side;
}

static int uv_rip_pairs_loop_count_on_side(BMLoop *l_init, uint side, const int cd_loop_uv_offset)
{
  const MLoopUV *luv_init = BM_ELEM_CD_GET_VOID_P(l_init, cd_loop_uv_offset);
  int count = 0;
  BMIter liter;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, l_init->v, BM_LOOPS_OF_VERT) {
    if (UL(l)->in_rip_pairs) {
      if (UL(l)->side == side) {
        const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (equals_v2v2(luv_init->uv, luv->uv)) {
          count += 1;
        }
      }
    }
  }
  return count;
}

static bool uv_rip_pairs_loop_change_sides_test(BMLoop *l_switch,
                                                BMLoop *l_target,
                                                const float aspect_y,
                                                const int cd_loop_uv_offset)
{
  const int side_a = UL(l_switch)->side;
  const int side_b = UL(l_target)->side;

  BLI_assert(UL(l_switch)->side != UL(l_target)->side);

  /* First, check if this is a simple grid topology,
   * in that case always choose the adjacent edge. */
  const int count_a = uv_rip_pairs_loop_count_on_side(l_switch, side_a, cd_loop_uv_offset);
  const int count_b = uv_rip_pairs_loop_count_on_side(l_target, side_b, cd_loop_uv_offset);
  if (count_a + count_b == 4) {
    return count_a > count_b;
  }
  else {
    const float angle_a_before = uv_rip_pairs_calc_uv_angle(
        l_switch, side_a, aspect_y, cd_loop_uv_offset);
    const float angle_b_before = uv_rip_pairs_calc_uv_angle(
        l_target, side_b, aspect_y, cd_loop_uv_offset);

    UL(l_switch)->side = side_b;

    const float angle_a_after = uv_rip_pairs_calc_uv_angle(
        l_switch, side_a, aspect_y, cd_loop_uv_offset);
    const float angle_b_after = uv_rip_pairs_calc_uv_angle(
        l_target, side_b, aspect_y, cd_loop_uv_offset);

    UL(l_switch)->side = side_a;

    return fabsf(angle_a_before - angle_b_before) > fabsf(angle_a_after - angle_b_after);
  }
}

/**
 * Create 2x sides of a UV rip-pairs, the result is unordered, supporting non-contiguous rails.
 *
 * \param l_init: A loop on a boundary which can be used to initialize flood-filling.
 * This will always be added to the first side. Other loops will be added to the second side.
 *
 * \note We could have more than two sides, however in practice this almost never happens.
 */
static UVRipPairs *uv_rip_pairs_from_loop(BMLoop *l_init,
                                          const float aspect_y,
                                          const int cd_loop_uv_offset)
{
  UVRipPairs *rip = MEM_callocN(sizeof(*rip), __func__);
  rip->loops = BLI_gset_ptr_new(__func__);

  /* We can rely on this stack being small, as we're walking down two sides of an edge loop,
   * so the stack wont be much larger than the total number of fans at any one vertex. */
  BLI_SMALLSTACK_DECLARE(stack, BMLoop *);

  /* Needed for cases when we walk onto loops which already have a side assigned,
   * in this case we need to pick a better side (see #uv_rip_pairs_loop_change_sides_test)
   * and put the loop back in the stack,
   * which is needed in the case adjacent loops should also switch sides. */
#define UV_SET_SIDE_AND_REMOVE_FROM_RAIL(loop, side_value) \
  { \
    BLI_assert(UL(loop)->side_was_swapped == false); \
    BLI_assert(UL(loop)->side != side_value); \
    if (!UL(loop)->in_stack) { \
      BLI_SMALLSTACK_PUSH(stack, loop); \
      UL(loop)->in_stack = true; \
    } \
    if (UL(loop)->in_rip_pairs) { \
      uv_rip_pairs_remove(rip, loop); \
    } \
    UL(loop)->side = side_value; \
    UL(loop)->side_was_swapped = true; \
  }

  /* Initialize the stack. */
  BLI_SMALLSTACK_PUSH(stack, l_init);
  UL(l_init)->in_stack = true;

  BMLoop *l_step;
  while ((l_step = BLI_SMALLSTACK_POP(stack))) {
    int side = UL(l_step)->side;
    UL(l_step)->in_stack = false;

    /* Note that we could add all loops into the rip-pairs when adding into the stack,
     * however this complicates removal, so add into the rip-pairs when popping from the stack. */
    uv_rip_pairs_add(rip, l_step);

    /* Add to the other side if it exists. */
    if (UL(l_step)->is_select_edge) {
      BMLoop *l_other = bm_loop_find_other_radial_loop_with_visible_face(l_step,
                                                                         cd_loop_uv_offset);
      if (l_other != NULL) {
        if (!UL(l_other)->in_rip_pairs && !UL(l_other)->in_stack) {
          BLI_SMALLSTACK_PUSH(stack, l_other);
          UL(l_other)->in_stack = true;
          UL(l_other)->side = !side;
        }
        else {
          if (UL(l_other)->side == side) {
            if (UL(l_other)->side_was_swapped == false) {
              UV_SET_SIDE_AND_REMOVE_FROM_RAIL(l_other, !side);
            }
          }
        }
      }

      /* Add the next loop along the edge on the same side. */
      l_other = l_step->next;
      if (!UL(l_other)->in_rip_pairs && !UL(l_other)->in_stack) {
        BLI_SMALLSTACK_PUSH(stack, l_other);
        UL(l_other)->in_stack = true;
        UL(l_other)->side = side;
      }
      else {
        if (UL(l_other)->side != side) {
          if ((UL(l_other)->side_was_swapped == false) &&
              uv_rip_pairs_loop_change_sides_test(l_other, l_step, aspect_y, cd_loop_uv_offset)) {
            UV_SET_SIDE_AND_REMOVE_FROM_RAIL(l_other, side);
          }
        }
      }
    }

    /* Walk over the fan of loops, starting from `l_step` in both directions. */
    for (int i = 0; i < 2; i++) {
      BMLoop *l_radial_first = i ? l_step : l_step->prev;
      if (l_radial_first != l_radial_first->radial_next) {
        BMEdge *e_radial = l_radial_first->e;
        BMLoop *l_radial_iter = l_radial_first->radial_next;
        do {
          /* Not a boundary and visible. */
          if (!UL(l_radial_iter)->is_select_edge &&
              BM_elem_flag_test(l_radial_iter->f, BM_ELEM_TAG)) {
            BMLoop *l_other = (l_radial_iter->v == l_step->v) ? l_radial_iter :
                                                                l_radial_iter->next;
            BLI_assert(l_other->v == l_step->v);
            if (BM_edge_uv_share_vert_check(e_radial, l_other, l_step, cd_loop_uv_offset)) {
              if (!UL(l_other)->in_rip_pairs && !UL(l_other)->in_stack) {
                BLI_SMALLSTACK_PUSH(stack, l_other);
                UL(l_other)->in_stack = true;
                UL(l_other)->side = side;
              }
              else {
                if (UL(l_other)->side != side) {
                  if ((UL(l_other)->side_was_swapped == false) &&
                      uv_rip_pairs_loop_change_sides_test(
                          l_other, l_step, aspect_y, cd_loop_uv_offset)) {
                    UV_SET_SIDE_AND_REMOVE_FROM_RAIL(l_other, side);
                  }
                }
              }
            }
          }
        } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
      }
    }
  }

#undef UV_SET_SIDE_AND_REMOVE_FROM_RAIL

  return rip;
}

static void uv_rip_pairs_free(UVRipPairs *rip)
{
  BLI_gset_free(rip->loops, NULL);
  MEM_freeN(rip);
}

/**
 * This is an approximation, it's easily good enough for our purpose.
 */
static bool uv_rip_pairs_calc_center_and_direction(UVRipPairs *rip,
                                                   const int cd_loop_uv_offset,
                                                   float r_center[2],
                                                   float r_dir_side[2][2])
{
  zero_v2(r_center);
  int center_total = 0;
  int side_total[2] = {0, 0};

  for (int i = 0; i < 2; i++) {
    zero_v2(r_dir_side[i]);
  }
  GSetIterator gs_iter;
  GSET_ITER (gs_iter, rip->loops) {
    BMLoop *l = BLI_gsetIterator_getKey(&gs_iter);
    int side = UL(l)->side;
    const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    add_v2_v2(r_center, luv->uv);

    float dir[2];
    if (!UL(l)->is_select_edge) {
      const MLoopUV *luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
      sub_v2_v2v2(dir, luv_next->uv, luv->uv);
      add_v2_v2(r_dir_side[side], dir);
    }
    if (!UL(l->prev)->is_select_edge) {
      const MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset);
      sub_v2_v2v2(dir, luv_prev->uv, luv->uv);
      add_v2_v2(r_dir_side[side], dir);
    }
    side_total[side] += 1;
  }
  center_total += BLI_gset_len(rip->loops);

  for (int i = 0; i < 2; i++) {
    normalize_v2(r_dir_side[i]);
  }
  mul_v2_fl(r_center, 1.0f / center_total);

  /* If only a single side is selected, don't handle this rip-pairs. */
  return side_total[0] && side_total[1];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Rip Main Function
 * \{ */

/**
 * \return true when a change was made.
 */
static bool uv_rip_object(Scene *scene, Object *obedit, const float co[2], const float aspect_y)
{
  Mesh *me = (Mesh *)obedit->data;
  BMEditMesh *em = me->edit_mesh;
  BMesh *bm = em->bm;
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BMFace *efa;
  BMIter iter, liter;
  BMLoop *l;

  const ULData ul_clear = {0};

  bool changed = false;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BM_elem_flag_set(efa, BM_ELEM_TAG, uvedit_face_visible_test(scene, efa));
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      ULData *ul = UL(l);
      *ul = ul_clear;
    }
  }
  bm->elem_index_dirty |= BM_LOOP;

  bool is_select_all_any = false;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
      bool is_all = true;
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (luv->flag & MLOOPUV_VERTSEL) {
          const MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset);
          const MLoopUV *luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
          if (luv_next->flag & MLOOPUV_VERTSEL) {
            UL(l)->is_select_edge = true;
          }
          else {
            if ((luv_prev->flag & MLOOPUV_VERTSEL) == 0) {
              /* #bm_loop_uv_select_single_vert_validate validates below. */
              UL(l)->is_select_vert_single = true;
            }
          }
        }
        else {
          is_all = false;
        }
      }
      if (is_all) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          UL(l)->is_select_all = true;
        }
        is_select_all_any = true;
      }
    }
  }

  /* Remove #ULData.is_select_vert_single when connected to selected edges. */
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (UL(l)->is_select_vert_single) {
          bm_loop_uv_select_single_vert_validate(l, cd_loop_uv_offset);
        }
      }
    }
  }

  /* Special case: if we have selected faces, isolated them.
   * This isn't a rip, however it's useful for users as a quick way
   * to detech the selection.
   *
   * We could also extract an edge loop from the boundary
   * however in practice it's not that useful, see T78751. */
  if (is_select_all_any) {
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (!UL(l)->is_select_all) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (luv->flag & MLOOPUV_VERTSEL) {
            luv->flag &= ~MLOOPUV_VERTSEL;
            changed = true;
          }
        }
      }
    }
    return changed;
  }

  /* Extract loop pairs or single loops. */
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (UL(l)->is_select_edge) {
          if (!UL(l)->in_rip_pairs) {
            UVRipPairs *rip = uv_rip_pairs_from_loop(l, aspect_y, cd_loop_uv_offset);
            float center[2];
            float dir_cursor[2];
            float dir_side[2][2];
            int side_from_cursor = -1;
            if (uv_rip_pairs_calc_center_and_direction(rip, cd_loop_uv_offset, center, dir_side)) {
              for (int i = 0; i < 2; i++) {
                sub_v2_v2v2(dir_cursor, center, co);
                normalize_v2(dir_cursor);
              }
              side_from_cursor = (dot_v2v2(dir_side[0], dir_cursor) -
                                  dot_v2v2(dir_side[1], dir_cursor)) < 0.0f;
            }
            GSetIterator gs_iter;
            GSET_ITER (gs_iter, rip->loops) {
              BMLoop *l_iter = BLI_gsetIterator_getKey(&gs_iter);
              ULData *ul = UL(l_iter);
              if (ul->side == side_from_cursor) {
                uvedit_uv_select_disable(em, scene, l_iter, cd_loop_uv_offset);
                changed = true;
              }
              /* Ensure we don't operate on these again. */
              *ul = ul_clear;
            }
            uv_rip_pairs_free(rip);
          }
        }
        else if (UL(l)->is_select_vert_single) {
          UVRipSingle *rip = uv_rip_single_from_loop(l, co, aspect_y, cd_loop_uv_offset);
          /* We only ever use one side. */
          const int side_from_cursor = 0;
          GSetIterator gs_iter;
          GSET_ITER (gs_iter, rip->loops) {
            BMLoop *l_iter = BLI_gsetIterator_getKey(&gs_iter);
            ULData *ul = UL(l_iter);
            if (ul->side == side_from_cursor) {
              uvedit_uv_select_disable(em, scene, l_iter, cd_loop_uv_offset);
              changed = true;
            }
            /* Ensure we don't operate on these again. */
            *ul = ul_clear;
          }
          uv_rip_single_free(rip);
        }
      }
    }
  }
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Rip Operator
 * \{ */

static int uv_rip_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  bool changed_multi = false;

  float co[2];
  RNA_float_get_array(op->ptr, "location", co);

  float aspx, aspy;
  {
    /* Note that we only want to run this on the  */
    Object *obedit = CTX_data_edit_object(C);
    ED_uvedit_get_aspect(obedit, &aspx, &aspy);
  }
  const float aspect_y = aspx / aspy;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];

    if (uv_rip_object(scene, obedit, co, aspect_y)) {
      changed_multi = true;
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(obedit->data, 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }
  MEM_freeN(objects);

  if (!changed_multi) {
    BKE_report(op->reports, RPT_ERROR, "Rip failed");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static int uv_rip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  return uv_rip_exec(C, op);
}

void UV_OT_rip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "UV Rip";
  ot->description = "Rip selected vertices or a selected region";
  ot->idname = "UV_OT_rip";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_rip_exec;
  ot->invoke = uv_rip_invoke;
  ot->poll = ED_operator_uvedit;

  /* translation data */
  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR_DUMMY);

  /* properties */
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
}

/** \} */
