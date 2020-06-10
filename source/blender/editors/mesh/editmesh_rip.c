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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "mesh_intern.h" /* own include */

/**
 * helper to find edge for edge_rip,
 *
 * \param inset: is used so we get some useful distance
 * when comparing multiple edges that meet at the same
 * point and would result in the same distance.
 */
#define INSET_DEFAULT 0.00001f
static float edbm_rip_edgedist_squared(ARegion *region,
                                       float mat[4][4],
                                       const float co1[3],
                                       const float co2[3],
                                       const float mvalf[2],
                                       const float inset)
{
  float vec1[2], vec2[2], dist_sq;

  ED_view3d_project_float_v2_m4(region, co1, vec1, mat);
  ED_view3d_project_float_v2_m4(region, co2, vec2, mat);

  if (inset != 0.0f) {
    const float dist_2d = len_v2v2(vec1, vec2);
    if (dist_2d > FLT_EPSILON) {
      const float dist = inset / dist_2d;
      BLI_assert(isfinite(dist));
      interp_v2_v2v2(vec1, vec1, vec2, dist);
      interp_v2_v2v2(vec2, vec2, vec1, dist);
    }
  }

  dist_sq = dist_squared_to_line_segment_v2(mvalf, vec1, vec2);
  BLI_assert(isfinite(dist_sq));

  return dist_sq;
}

#if 0
static float edbm_rip_linedist(
    ARegion *region, float mat[4][4], const float co1[3], const float co2[3], const float mvalf[2])
{
  float vec1[2], vec2[2];

  ED_view3d_project_float_v2_m4(region, co1, vec1, mat);
  ED_view3d_project_float_v2_m4(region, co2, vec2, mat);

  return dist_to_line_v2(mvalf, vec1, vec2);
}
#endif

/* calculaters a point along the loop tangent which can be used to measure against edges */
static void edbm_calc_loop_co(BMLoop *l, float l_mid_co[3])
{
  BM_loop_calc_face_tangent(l, l_mid_co);

  /* scale to average of surrounding edge size, only needs to be approx, but should
   * be roughly equivalent to the check below which uses the middle of the edge. */
  mul_v3_fl(l_mid_co, (BM_edge_calc_length(l->e) + BM_edge_calc_length(l->prev->e)) / 2.0f);

  add_v3_v3(l_mid_co, l->v->co);
}

static float edbm_rip_edge_side_measure(
    BMEdge *e, BMLoop *e_l, ARegion *region, float projectMat[4][4], const float fmval[2])
{
  float cent[3] = {0, 0, 0}, mid[3];

  float vec[2];
  float fmval_tweak[2];
  float e_v1_co[2], e_v2_co[2];
  float score;

  BMVert *v1_other;
  BMVert *v2_other;

  BLI_assert(BM_vert_in_edge(e, e_l->v));

  /* method for calculating distance:
   *
   * for each edge: calculate face center, then made a vector
   * from edge midpoint to face center.  offset edge midpoint
   * by a small amount along this vector. */

  /* rather then the face center, get the middle of
   * both edge verts connected to this one */
  v1_other = BM_face_other_vert_loop(e_l->f, e->v2, e->v1)->v;
  v2_other = BM_face_other_vert_loop(e_l->f, e->v1, e->v2)->v;
  mid_v3_v3v3(cent, v1_other->co, v2_other->co);
  mid_v3_v3v3(mid, e->v1->co, e->v2->co);

  ED_view3d_project_float_v2_m4(region, cent, cent, projectMat);
  ED_view3d_project_float_v2_m4(region, mid, mid, projectMat);

  ED_view3d_project_float_v2_m4(region, e->v1->co, e_v1_co, projectMat);
  ED_view3d_project_float_v2_m4(region, e->v2->co, e_v2_co, projectMat);

  sub_v2_v2v2(vec, cent, mid);
  normalize_v2_length(vec, 0.01f);

  /* rather then adding to both verts, subtract from the mouse */
  sub_v2_v2v2(fmval_tweak, fmval, vec);

  score = len_v2v2(e_v1_co, e_v2_co);

  if (dist_squared_to_line_segment_v2(fmval_tweak, e_v1_co, e_v2_co) >
      dist_squared_to_line_segment_v2(fmval, e_v1_co, e_v2_co)) {
    return score;
  }
  else {
    return -score;
  }
}

/* - Advanced selection handling 'ripsel' functions ----- */

/**
 * How rip selection works
 *
 * Firstly - rip is basically edge split with side-selection & grab.
 * Things would be much more simple if we didn't have to worry about side selection
 *
 * The method used for checking the side of selection is as follows...
 * - First tag all rip-able edges.
 * - Build a contiguous edge list by looping over tagged edges and following each ones tagged
 *   siblings in both directions.
 *   - The loops are not stored in an array, Instead both loops on either side of each edge has
 *     its index values set to count down from the last edge, this way, once we have the 'last'
 *     edge its very easy to walk down the connected edge loops.
 *     The reason for using loops like this is because when the edges are split we don't which
 *     face user gets the newly created edge
 *     (its as good as random so we cant assume new edges will be on once side).
 *     After splitting, its very simple to walk along boundary loops since each only has one edge
 *     from a single side.
 * - The end loop pairs are stored in an array however to support multiple edge-selection-islands,
 *   so you can rip multiple selections at once.
 * - * Execute the split *
 * - For each #EdgeLoopPair walk down both sides of the split using the loops and measure
 *   which is facing the mouse.
 * - Deselect the edge loop facing away.
 *
 * Limitation!
 * This currently works very poorly with intersecting edge islands
 * (verts with more than 2 tagged edges). This is nice to but for now not essential.
 *
 * - campbell.
 */

#define IS_VISIT_POSSIBLE(e) (BM_edge_is_manifold(e) && BM_elem_flag_test(e, BM_ELEM_TAG))
#define IS_VISIT_DONE(e) ((e)->l && (BM_elem_index_get((e)->l) != INVALID_UID))
#define INVALID_UID INT_MIN

/* mark, assign uid and step */
static BMEdge *edbm_ripsel_edge_mark_step(BMVert *v, const int uid)
{
  BMIter iter;
  BMEdge *e;
  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    if (IS_VISIT_POSSIBLE(e) && !IS_VISIT_DONE(e)) {
      BMLoop *l_a, *l_b;

      BM_edge_loop_pair(e, &l_a, &l_b); /* no need to check, we know this will be true */

      /* so (IS_VISIT_DONE == true) */
      BM_elem_index_set(l_a, uid); /* set_dirty */
      BM_elem_index_set(l_b, uid); /* set_dirty */

      return e;
    }
  }
  return NULL;
}

typedef struct EdgeLoopPair {
  BMLoop *l_a;
  BMLoop *l_b;
} EdgeLoopPair;

static EdgeLoopPair *edbm_ripsel_looptag_helper(BMesh *bm)
{
  BMIter fiter;
  BMIter liter;

  BMFace *f;
  BMLoop *l;

  int uid_start;
  int uid_end;
  int uid = bm->totedge; /* can start anywhere */

  EdgeLoopPair *eloop_pairs = NULL;
  BLI_array_declare(eloop_pairs);
  EdgeLoopPair *lp;

  /* initialize loops with dummy invalid index values */
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      BM_elem_index_set(l, INVALID_UID); /* set_dirty */
    }
  }
  bm->elem_index_dirty |= BM_LOOP;

  /* set contiguous loops ordered 'uid' values for walking after split */
  while (true) {
    int tot = 0;
    BMIter eiter;
    BMEdge *e_step;
    BMVert *v_step;
    BMEdge *e;
    BMEdge *e_first;
    BMEdge *e_last;

    e_first = NULL;
    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
      if (IS_VISIT_POSSIBLE(e) && !IS_VISIT_DONE(e)) {
        e_first = e;
        break;
      }
    }

    if (e_first == NULL) {
      break;
    }

    /* initialize  */
    e_first = e;
    v_step = e_first->v1;
    e_step = NULL; /* quiet warning, will never remain this value */

    uid_start = uid;
    while ((e = edbm_ripsel_edge_mark_step(v_step, uid))) {
      v_step = BM_edge_other_vert((e_step = e), v_step);
      uid++; /* only different line */
      tot++;
    }

    /* this edges loops have the highest uid's, store this to walk down later */
    e_last = e_step;

    /* always store the highest 'uid' edge for the stride */
    uid_end = uid - 1;
    uid = uid_start - 1;

    /* initialize */
    v_step = e_first->v1;

    while ((e = edbm_ripsel_edge_mark_step(v_step, uid))) {
      v_step = BM_edge_other_vert((e_step = e), v_step);
      uid--; /* only different line */
      tot++;
    }

    /* stride far enough not to _ever_ overlap range */
    uid_start = uid;
    uid = uid_end + bm->totedge;

    lp = BLI_array_append_ret(eloop_pairs);
    /* no need to check, we know this will be true */
    BM_edge_loop_pair(e_last, &lp->l_a, &lp->l_b);

    BLI_assert(tot == uid_end - uid_start);

#if 0
    printf("%s: found contiguous edge loop of (%d)\n", __func__, uid_end - uid_start);
#endif
  }

  /* null terminate */
  lp = BLI_array_append_ret(eloop_pairs);
  lp->l_a = lp->l_b = NULL;

  return eloop_pairs;
}

/* - De-Select the worst rip-edge side -------------------------------- */

static BMEdge *edbm_ripsel_edge_uid_step(BMEdge *e_orig, BMVert **v_prev)
{
  BMIter eiter;
  BMEdge *e;
  BMVert *v = BM_edge_other_vert(e_orig, *v_prev);
  const int uid_cmp = BM_elem_index_get(e_orig->l) - 1;

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    if (BM_elem_index_get(e->l) == uid_cmp) {
      *v_prev = v;
      return e;
    }
  }
  return NULL;
}

static BMVert *edbm_ripsel_edloop_pair_start_vert(BMEdge *e)
{
  /* try step in a direction, if it fails we know do go the other way */
  BMVert *v_test = e->v1;
  return (edbm_ripsel_edge_uid_step(e, &v_test)) ? e->v1 : e->v2;
}

static void edbm_ripsel_deselect_helper(
    BMesh *bm, EdgeLoopPair *eloop_pairs, ARegion *region, float projectMat[4][4], float fmval[2])
{
  EdgeLoopPair *lp;

  for (lp = eloop_pairs; lp->l_a; lp++) {
    BMEdge *e;
    BMVert *v_prev;

    float score_a = 0.0f;
    float score_b = 0.0f;

    e = lp->l_a->e;
    v_prev = edbm_ripsel_edloop_pair_start_vert(e);
    for (; e; e = edbm_ripsel_edge_uid_step(e, &v_prev)) {
      score_a += edbm_rip_edge_side_measure(e, e->l, region, projectMat, fmval);
    }
    e = lp->l_b->e;
    v_prev = edbm_ripsel_edloop_pair_start_vert(e);
    for (; e; e = edbm_ripsel_edge_uid_step(e, &v_prev)) {
      score_b += edbm_rip_edge_side_measure(e, e->l, region, projectMat, fmval);
    }

    e = (score_a > score_b) ? lp->l_a->e : lp->l_b->e;
    v_prev = edbm_ripsel_edloop_pair_start_vert(e);
    for (; e; e = edbm_ripsel_edge_uid_step(e, &v_prev)) {
      BM_edge_select_set(bm, e, false);
    }
  }
}
/* --- end 'ripsel' selection handling code --- */

/* --- face-fill code --- */
/**
 * return an un-ordered array of loop pairs
 * use for rebuilding face-fill
 *
 * \note the method currently used fails for edges with 3+ face users and gives
 *       nasty holes in the mesh, there isnt a good way of knowing ahead of time
 *       which loops will be split apart (its possible to figure out but quite involved).
 *       So for now this is a known limitation of current rip-fill option.
 */

typedef struct UnorderedLoopPair {
  BMLoop *l_pair[2];
  char flag;
} UnorderedLoopPair;
enum {
  ULP_FLIP_0 = (1 << 0),
  ULP_FLIP_1 = (1 << 1),
};

static UnorderedLoopPair *edbm_tagged_loop_pairs_to_fill(BMesh *bm)
{
  BMIter iter;
  BMEdge *e;

  uint total_tag = 0;
  /* count tags, could be pre-calculated */
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
      total_tag++;
    }
  }

  if (total_tag) {
    UnorderedLoopPair *uloop_pairs = MEM_mallocN(total_tag * sizeof(UnorderedLoopPair), __func__);
    UnorderedLoopPair *ulp = uloop_pairs;

    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
        BMLoop *l1, *l2;
        if (BM_edge_loop_pair(e, &l1, &l2)) {
          BMVert *v_cmp = l1->e->v1;
          ulp->flag = (((l1->v != v_cmp) ? ULP_FLIP_0 : 0) | ((l2->v == v_cmp) ? ULP_FLIP_1 : 0));
        }
        else {
          ulp->flag = 0;
        }
        ulp->l_pair[0] = l1;
        ulp->l_pair[1] = l2;

        ulp++;
      }
    }

    return uloop_pairs;
  }
  else {
    return NULL;
  }
}

static void edbm_tagged_loop_pairs_do_fill_faces(BMesh *bm, UnorderedLoopPair *uloop_pairs)
{
  UnorderedLoopPair *ulp;
  uint total_tag = MEM_allocN_len(uloop_pairs) / sizeof(UnorderedLoopPair);
  uint i;

  for (i = 0, ulp = uloop_pairs; i < total_tag; i++, ulp++) {
    if ((ulp->l_pair[0] && ulp->l_pair[1]) && (ulp->l_pair[0]->e != ulp->l_pair[1]->e)) {
      /* time has come to make a face! */
      BMVert *v_shared = BM_edge_share_vert(ulp->l_pair[0]->e, ulp->l_pair[1]->e);
      BMFace *f, *f_example = ulp->l_pair[0]->f;
      BMLoop *l_iter;
      BMVert *f_verts[4];

      if (v_shared == NULL) {
        /* quad */
        f_verts[0] = ulp->l_pair[0]->e->v1;
        f_verts[1] = ulp->l_pair[1]->e->v1;
        f_verts[2] = ulp->l_pair[1]->e->v2;
        f_verts[3] = ulp->l_pair[0]->e->v2;

        if (ulp->flag & ULP_FLIP_0) {
          SWAP(BMVert *, f_verts[0], f_verts[3]);
        }
        if (ulp->flag & ULP_FLIP_1) {
          SWAP(BMVert *, f_verts[1], f_verts[2]);
        }
      }
      else {
        /* tri */
        f_verts[0] = v_shared;
        f_verts[1] = BM_edge_other_vert(ulp->l_pair[0]->e, v_shared);
        f_verts[2] = BM_edge_other_vert(ulp->l_pair[1]->e, v_shared);
        f_verts[3] = NULL;

        /* don't use the flip flags */
        if (v_shared == ulp->l_pair[0]->v) {
          SWAP(BMVert *, f_verts[0], f_verts[1]);
        }
      }

      /* face should never exist */
      BLI_assert(!BM_face_exists(f_verts, f_verts[3] ? 4 : 3));

      f = BM_face_create_verts(bm, f_verts, f_verts[3] ? 4 : 3, f_example, BM_CREATE_NOP, true);

      l_iter = BM_FACE_FIRST_LOOP(f);

      if (f_verts[3]) {
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[0]->e, l_iter), l_iter);
        l_iter = l_iter->next;
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[1]->e, l_iter), l_iter);
        l_iter = l_iter->next;
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[1]->e, l_iter), l_iter);
        l_iter = l_iter->next;
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[0]->e, l_iter), l_iter);
      }
      else {
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[0]->e, l_iter), l_iter);
        l_iter = l_iter->next;
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[0]->e, l_iter), l_iter);
        l_iter = l_iter->next;
        BM_elem_attrs_copy(bm, bm, BM_edge_other_loop(ulp->l_pair[1]->e, l_iter), l_iter);
      }
    }
  }
}

/* --- end 'face-fill' code --- */

/**
 * This is the main vert ripping function (rip when one vertex is selected)
 */
static int edbm_rip_invoke__vert(bContext *C, const wmEvent *event, Object *obedit, bool do_fill)
{
  UnorderedLoopPair *fill_uloop_pairs = NULL;
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  BMIter iter, liter;
  BMLoop *l;
  BMEdge *e_best;
  BMVert *v;
  const int totvert_orig = bm->totvert;
  int i;
  float projectMat[4][4], fmval[3] = {event->mval[0], event->mval[1]};
  float dist_sq = FLT_MAX;
  float d;
  bool is_wire, is_manifold_region;

  BMEditSelection ese;
  int totboundary_edge = 0;

  ED_view3d_ob_project_mat_get(rv3d, obedit, projectMat);

  /* find selected vert - same some time and check history first */
  if (BM_select_history_active_get(bm, &ese) && ese.htype == BM_VERT) {
    v = (BMVert *)ese.ele;
  }
  else {
    ese.ele = NULL;

    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        break;
      }
    }
  }

  /* (v == NULL) should be impossible */
  if ((v == NULL) || (v->e == NULL)) {
    return OPERATOR_CANCELLED;
  }

  is_wire = BM_vert_is_wire(v);
  is_manifold_region = BM_vert_is_manifold_region(v);

  e_best = NULL;

  {
    BMEdge *e;
    /* find closest edge to mouse cursor */
    BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
      /* consider wire as boundary for this purpose,
       * otherwise we can't a face away from a wire edge */
      totboundary_edge += (BM_edge_is_boundary(e) || BM_edge_is_wire(e));
      if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        if ((is_manifold_region == false) || BM_edge_is_manifold(e)) {
          d = edbm_rip_edgedist_squared(
              region, projectMat, e->v1->co, e->v2->co, fmval, INSET_DEFAULT);
          if ((e_best == NULL) || (d < dist_sq)) {
            dist_sq = d;
            e_best = e;
          }
        }
      }
    }
  }

  if (e_best && e_best->l && (is_manifold_region == false)) {
    /* Try to split off a non-manifold fan (when we have multiple disconnected fans) */
    BMLoop *l_sep = e_best->l->v == v ? e_best->l : e_best->l->next;
    BMVert *v_new;

    BLI_assert(l_sep->v == v);
    v_new = BM_face_loop_separate_multi_isolated(bm, l_sep);
    BLI_assert(BM_vert_find_first_loop(v));

    BM_vert_select_set(bm, v, false);
    BM_select_history_remove(bm, v);

    BM_vert_select_set(bm, v_new, true);
    if (ese.ele) {
      BM_select_history_store(bm, v_new);
    }

    if (do_fill) {
      BM_edge_create(bm, v, v_new, NULL, BM_CREATE_NOP);
    }

    return OPERATOR_FINISHED;
  }

  /* if we are ripping a single vertex from 3 faces,
   * then measure the distance to the face corner as well as the edge */
  if (BM_vert_face_count_is_equal(v, 3) && BM_vert_edge_count_is_equal(v, 3)) {
    BMEdge *e_all[3];
    BMLoop *l_all[3];
    int i1, i2;

    BM_iter_as_array(bm, BM_EDGES_OF_VERT, v, (void **)e_all, 3);
    BM_iter_as_array(bm, BM_LOOPS_OF_VERT, v, (void **)l_all, 3);

    /* not do a loop similar to the one above, but test against loops */
    for (i1 = 0; i1 < 3; i1++) {
      /* consider wire as boundary for this purpose,
       * otherwise we can't a face away from a wire edge */
      float l_mid_co[3];
      l = l_all[i1];
      edbm_calc_loop_co(l, l_mid_co);
      d = edbm_rip_edgedist_squared(region, projectMat, l->v->co, l_mid_co, fmval, INSET_DEFAULT);
      if ((e_best == NULL) || (d < dist_sq)) {
        dist_sq = d;

        /* find the edge that is not in this loop */
        e_best = NULL;
        for (i2 = 0; i2 < 3; i2++) {
          if (!BM_edge_in_loop(e_all[i2], l)) {
            e_best = e_all[i2];
            break;
          }
        }
        BLI_assert(e_best != NULL);
      }
    }
  }

  /* should we go ahead with edge rip or do we need to do special case, split off vertex?:
   * split off vertex if...
   * - we cant find an edge - this means we are ripping a faces vert that is connected to other
   *   geometry only at the vertex.
   * - the boundary edge total is greater than 2,
   *   in this case edge split _can_ work but we get far nicer results if we use this special case.
   * - there are only 2 edges but we are a wire vert. */
  if ((is_wire == false && totboundary_edge > 2) || (is_wire == true && totboundary_edge > 1)) {
    BMVert **vout;
    int vout_len;

    BM_vert_select_set(bm, v, false);

    bmesh_kernel_vert_separate(bm, v, &vout, &vout_len, true);

    if (vout_len < 2) {
      MEM_freeN(vout);
      /* set selection back to avoid active-unselected vertex */
      BM_vert_select_set(bm, v, true);
      /* should never happen */
      return OPERATOR_CANCELLED;
    }
    else {
      int vi_best = 0;

      if (ese.ele) {
        BM_select_history_remove(bm, ese.ele);
      }

      dist_sq = FLT_MAX;

      /* in the loop below we find the best vertex to drag based on its connected geometry,
       * either by its face corner, or connected edge (when no faces are attached) */
      for (i = 0; i < vout_len; i++) {

        if (BM_vert_is_wire(vout[i]) == false) {
          /* find the best face corner */
          BM_ITER_ELEM (l, &iter, vout[i], BM_LOOPS_OF_VERT) {
            if (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
              float l_mid_co[3];

              edbm_calc_loop_co(l, l_mid_co);
              d = edbm_rip_edgedist_squared(
                  region, projectMat, v->co, l_mid_co, fmval, INSET_DEFAULT);

              if (d < dist_sq) {
                dist_sq = d;
                vi_best = i;
              }
            }
          }
        }
        else {
          BMEdge *e;
          /* a wire vert, find the best edge */
          BM_ITER_ELEM (e, &iter, vout[i], BM_EDGES_OF_VERT) {
            if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
              float e_mid_co[3];

              mid_v3_v3v3(e_mid_co, e->v1->co, e->v2->co);
              d = edbm_rip_edgedist_squared(
                  region, projectMat, v->co, e_mid_co, fmval, INSET_DEFAULT);

              if (d < dist_sq) {
                dist_sq = d;
                vi_best = i;
              }
            }
          }
        }
      }

      /* vout[0]  == best
       * vout[1]  == glue
       * vout[2+] == splice with glue (when vout_len > 2)
       */
      if (vi_best != 0) {
        SWAP(BMVert *, vout[0], vout[vi_best]);
        vi_best = 0;
      }

      /* select the vert from the best region */
      v = vout[vi_best];
      BM_vert_select_set(bm, v, true);

      if (ese.ele) {
        BM_select_history_store(bm, v);
      }

      /* splice all others back together */
      if (vout_len > 2) {
        for (i = 2; i < vout_len; i++) {
          BM_vert_splice(bm, vout[1], vout[i]);
        }
      }

      if (do_fill) {
        /* match extrude vert-order */
        BM_edge_create(bm, vout[1], vout[0], NULL, BM_CREATE_NOP);
      }

      MEM_freeN(vout);

      return OPERATOR_FINISHED;
    }
  }

  if (!e_best) {
    return OPERATOR_CANCELLED;
  }

  /* *** Execute the split! *** */
  /* unlike edge split, for single vertex split we only use the operator in one of the cases
   * but both allocate fill */

  {
    BMVert *v_rip;
    BMLoop *larr[2];
    int larr_len = 0;

    /* rip two adjacent edges */
    if (BM_edge_is_boundary(e_best) || BM_vert_face_count_is_equal(v, 2)) {
      /* Don't run the edge split operator in this case */

      l = BM_edge_vert_share_loop(e_best->l, v);
      larr[larr_len] = l;
      larr_len++;

      /* only tag for face-fill (we don't call the operator) */
      if (BM_edge_is_boundary(e_best)) {
        BM_elem_flag_enable(e_best, BM_ELEM_TAG);
      }
      else {
        BM_elem_flag_enable(l->e, BM_ELEM_TAG);
        BM_elem_flag_enable(l->prev->e, BM_ELEM_TAG);
      }
    }
    else {
      if (BM_edge_is_manifold(e_best)) {
        BMLoop *l_iter, *l_first;
        l_iter = l_first = e_best->l;
        do {
          larr[larr_len] = BM_edge_vert_share_loop(l_iter, v);

          if (do_fill) {
            /* Only needed when filling...
             * Also, we never want to tag best edge,
             * that one won't change during split. See T44618. */
            if (larr[larr_len]->e == e_best) {
              BM_elem_flag_enable(larr[larr_len]->prev->e, BM_ELEM_TAG);
            }
            else {
              BM_elem_flag_enable(larr[larr_len]->e, BM_ELEM_TAG);
            }
          }
          larr_len++;
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
      else {
        /* looks like there are no split edges, we could just return/report-error? - Campbell */
      }
    }

    /* keep directly before edgesplit */
    if (do_fill) {
      fill_uloop_pairs = edbm_tagged_loop_pairs_to_fill(bm);
    }

    if (larr_len) {
      v_rip = BM_face_loop_separate_multi(bm, larr, larr_len);
    }
    else {
      v_rip = NULL;
    }

    if (v_rip) {
      BM_vert_select_set(bm, v_rip, true);
    }
    else {
      if (fill_uloop_pairs) {
        MEM_freeN(fill_uloop_pairs);
      }
      return OPERATOR_CANCELLED;
    }
  }

  {
    /* --- select which vert --- */
    BMVert *v_best = NULL;
    float l_corner_co[3];

    dist_sq = FLT_MAX;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        /* disable by default, re-enable winner at end */
        BM_vert_select_set(bm, v, false);
        BM_select_history_remove(bm, v);

        BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {

          /* check if v_best is null in the _rare_ case there are numeric issues */
          edbm_calc_loop_co(l, l_corner_co);
          d = edbm_rip_edgedist_squared(
              region, projectMat, l->v->co, l_corner_co, fmval, INSET_DEFAULT);
          if ((v_best == NULL) || (d < dist_sq)) {
            v_best = v;
            dist_sq = d;
          }
        }
      }
    }

    if (v_best) {
      BM_vert_select_set(bm, v_best, true);
      if (ese.ele) {
        BM_select_history_store(bm, v_best);
      }
    }
  }

  if (do_fill && fill_uloop_pairs) {
    edbm_tagged_loop_pairs_do_fill_faces(bm, fill_uloop_pairs);
    MEM_freeN(fill_uloop_pairs);
  }

  if (totvert_orig == bm->totvert) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

/**
 * This is the main edge ripping function
 */
static int edbm_rip_invoke__edge(bContext *C, const wmEvent *event, Object *obedit, bool do_fill)
{
  UnorderedLoopPair *fill_uloop_pairs = NULL;
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  BMIter iter, eiter;
  BMLoop *l;
  BMEdge *e_best;
  BMVert *v;
  const int totedge_orig = bm->totedge;
  float projectMat[4][4], fmval[3] = {event->mval[0], event->mval[1]};

  EdgeLoopPair *eloop_pairs;

  ED_view3d_ob_project_mat_get(rv3d, obedit, projectMat);

  /* important this runs on the original selection, before tampering with tagging */
  eloop_pairs = edbm_ripsel_looptag_helper(bm);

  /* expand edge selection */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BMEdge *e;
    bool all_manifold;
    int totedge_manifold; /* manifold, visible edges */
    int i;

    e_best = NULL;
    i = 0;
    totedge_manifold = 0;
    all_manifold = true;
    BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {

      if (!BM_edge_is_wire(e) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        /* important to check selection rather then tag here
         * else we get feedback loop */
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          e_best = e;
          i++;
          /* Tag the edge verts so we know which verts to rip */
          BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
        }
        totedge_manifold++;
      }

      /** #BM_vert_other_disk_edge has no hidden checks so don't check hidden here */
      if ((all_manifold == true) && (BM_edge_is_manifold(e) == false)) {
        all_manifold = false;
      }
    }

    /* single edge, extend */
    if (i == 1 && e_best->l) {
      /* note: if the case of 3 edges has one change in loop stepping,
       * if this becomes more involved we may be better off splitting
       * the 3 edge case into its own else-if branch */
      if ((totedge_manifold == 4 || totedge_manifold == 3) || (all_manifold == false)) {
        BMLoop *l_a = e_best->l;
        BMLoop *l_b = l_a->radial_next;

        /* find the best face to follow, this way the edge won't point away from
         * the mouse when there are more than 4 (takes the shortest face fan around) */
        l = (edbm_rip_edge_side_measure(e_best, l_a, region, projectMat, fmval) <
             edbm_rip_edge_side_measure(e_best, l_b, region, projectMat, fmval)) ?
                l_a :
                l_b;

        l = BM_loop_other_edge_loop(l, v);
        /* Important edge is manifold else we can be attempting to split off
         * a fan that don't budge, not crashing but adds duplicate edge. */
        if (BM_edge_is_manifold(l->e)) {
          l = l->radial_next;

          if (totedge_manifold != 3) {
            l = BM_loop_other_edge_loop(l, v);
          }

          if (l) {
            BLI_assert(!BM_elem_flag_test(l->e, BM_ELEM_TAG));
            BM_elem_flag_enable(l->e, BM_ELEM_TAG);
          }
        }
      }
      else {
        e = BM_vert_other_disk_edge(v, e_best);

        if (e) {
          BLI_assert(!BM_elem_flag_test(e, BM_ELEM_TAG));
          BM_elem_flag_enable(e, BM_ELEM_TAG);
        }
      }
    }
  }

  /* keep directly before edgesplit */
  if (do_fill) {
    fill_uloop_pairs = edbm_tagged_loop_pairs_to_fill(bm);
  }

  BM_mesh_edgesplit(em->bm, true, true, true);

  /* note: the output of the bmesh operator is ignored, since we built
   * the contiguous loop pairs to split already, its possible that some
   * edge did not split even though it was tagged which would not work
   * as expected (but not crash), however there are checks to ensure
   * tagged edges will split. So far its not been an issue. */
  edbm_ripsel_deselect_helper(bm, eloop_pairs, region, projectMat, fmval);
  MEM_freeN(eloop_pairs);

  /* deselect loose verts */
  BM_mesh_select_mode_clean_ex(bm, SCE_SELECT_EDGE);

  if (do_fill && fill_uloop_pairs) {
    edbm_tagged_loop_pairs_do_fill_faces(bm, fill_uloop_pairs);
    MEM_freeN(fill_uloop_pairs);
  }

  if (totedge_orig == bm->totedge) {
    return OPERATOR_CANCELLED;
  }

  BM_select_history_validate(bm);

  return OPERATOR_FINISHED;
}

/* based on mouse cursor position, it defines how is being ripped */
static int edbm_rip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  const bool do_fill = RNA_boolean_get(op->ptr, "use_fill");

  bool no_vertex_selected = true;
  bool error_face_selected = true;
  bool error_disconnected_vertices = true;
  bool error_rip_failed = true;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    BMesh *bm = em->bm;
    BMIter iter;
    BMEdge *e;
    const bool singlesel = (bm->totvertsel == 1 && bm->totedgesel == 0 && bm->totfacesel == 0);
    int ret;

    if (em->bm->totvertsel == 0) {
      continue;
    }
    no_vertex_selected = false;

    /* running in face mode hardly makes sense, so convert to region loop and rip */
    if (bm->totfacesel) {
      /* highly nifty but hard to support since the operator can fail and we're left
       * with modified selection */
      // WM_operator_name_call(C, "MESH_OT_region_to_loop", WM_OP_INVOKE_DEFAULT, NULL);
      continue;
    }
    error_face_selected = false;

    /* we could support this, but not for now */
    if ((bm->totvertsel > 1) && (bm->totedgesel == 0)) {
      continue;
    }
    error_disconnected_vertices = false;

    /* note on selection:
     * When calling edge split we operate on tagged edges rather then selected
     * this is important because the edges to operate on are extended by one,
     * but the selection is left alone.
     *
     * After calling edge split - the duplicated edges have the same selection state as the
     * original, so all we do is de-select the far side from the mouse and we have a
     * useful selection for grabbing.
     */

    BM_custom_loop_normals_to_vector_layer(bm);

    /* BM_ELEM_SELECT --> BM_ELEM_TAG */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
    }

    /* split 2 main parts of this operator out into vertex and edge ripping */
    if (singlesel) {
      ret = edbm_rip_invoke__vert(C, event, obedit, do_fill);
    }
    else {
      ret = edbm_rip_invoke__edge(C, event, obedit, do_fill);
    }

    if (ret != OPERATOR_FINISHED) {
      continue;
    }

    BM_custom_loop_normals_from_vector_layer(bm, false);

    BLI_assert(singlesel ? (bm->totvertsel > 0) : (bm->totedgesel > 0));

    if (bm->totvertsel == 0) {
      continue;
    }
    error_rip_failed = false;

    EDBM_update_generic(obedit->data, true, true);
  }

  MEM_freeN(objects);

  if (no_vertex_selected) {
    /* Ignore it. */
    return OPERATOR_CANCELLED;
  }
  else if (error_face_selected) {
    BKE_report(op->reports, RPT_ERROR, "Cannot rip selected faces");
    return OPERATOR_CANCELLED;
  }
  else if (error_disconnected_vertices) {
    BKE_report(op->reports, RPT_ERROR, "Cannot rip multiple disconnected vertices");
    return OPERATOR_CANCELLED;
  }
  else if (error_rip_failed) {
    BKE_report(op->reports, RPT_ERROR, "Rip failed");
    return OPERATOR_CANCELLED;
  }
  /* No errors, everything went fine. */
  return OPERATOR_FINISHED;
}

void MESH_OT_rip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rip";
  ot->idname = "MESH_OT_rip";
  ot->description = "Disconnect vertex or edges from connected geometry";

  /* api callbacks */
  ot->invoke = edbm_rip_invoke;
  ot->poll = EDBM_view3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR_DUMMY);
  RNA_def_boolean(ot->srna, "use_fill", false, "Fill", "Fill the ripped region");
}
