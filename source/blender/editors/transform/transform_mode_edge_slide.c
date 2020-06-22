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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines_stack.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_unit.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_mesh.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Edge Slide) */

/** \name Transform Edge Slide
 * \{ */

typedef struct TransDataEdgeSlideVert {
  /** #TransDataGenericSlideVert (header) */
  struct BMVert *v;
  struct LinkNode **cd_loop_groups;
  float v_co_orig[3];
  /* end generic */

  float edge_len;

  struct BMVert *v_side[2];

  /* add origvert.co to get the original locations */
  float dir_side[2][3];

  int loop_nr;
} TransDataEdgeSlideVert;

typedef struct EdgeSlideData {
  TransDataEdgeSlideVert *sv;
  int totsv;

  int mval_start[2], mval_end[2];
  int curr_sv_index;

  /** when un-clamped - use this index: #TransDataEdgeSlideVert.dir_side */
  int curr_side_unclamp;
} EdgeSlideData;

typedef struct EdgeSlideParams {
  float perc;

  bool use_even;
  bool flipped;
} EdgeSlideParams;

/**
 * Get the first valid TransDataContainer *.
 *
 * Note we cannot trust TRANS_DATA_CONTAINER_FIRST_OK because of multi-object that
 * may leave items with invalid custom data in the transform data container.
 */
static TransDataContainer *edge_slide_container_first_ok(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->custom.mode.data) {
      return tc;
    }
  }
  BLI_assert(!"Should never happen, at least one EdgeSlideData should be valid");
  return NULL;
}

static EdgeSlideData *edgeSlideFirstGet(TransInfo *t)
{
  TransDataContainer *tc = edge_slide_container_first_ok(t);
  return tc->custom.mode.data;
}

static void calcEdgeSlideCustomPoints(struct TransInfo *t)
{
  EdgeSlideData *sld = edgeSlideFirstGet(t);

  setCustomPoints(t, &t->mouse, sld->mval_end, sld->mval_start);

  /* setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

static BMEdge *get_other_edge(BMVert *v, BMEdge *e)
{
  BMIter iter;
  BMEdge *e_iter;

  BM_ITER_ELEM (e_iter, &iter, v, BM_EDGES_OF_VERT) {
    if (BM_elem_flag_test(e_iter, BM_ELEM_SELECT) && e_iter != e) {
      return e_iter;
    }
  }

  return NULL;
}

/* interpoaltes along a line made up of 2 segments (used for edge slide) */
static void interp_line_v3_v3v3v3(
    float p[3], const float v1[3], const float v2[3], const float v3[3], float t)
{
  float t_mid, t_delta;

  /* could be pre-calculated */
  t_mid = line_point_factor_v3(v2, v1, v3);

  t_delta = t - t_mid;
  if (t_delta < 0.0f) {
    if (UNLIKELY(fabsf(t_mid) < FLT_EPSILON)) {
      copy_v3_v3(p, v2);
    }
    else {
      interp_v3_v3v3(p, v1, v2, t / t_mid);
    }
  }
  else {
    t = t - t_mid;
    t_mid = 1.0f - t_mid;

    if (UNLIKELY(fabsf(t_mid) < FLT_EPSILON)) {
      copy_v3_v3(p, v3);
    }
    else {
      interp_v3_v3v3(p, v2, v3, t / t_mid);
    }
  }
}

/**
 * Find the closest point on the ngon on the opposite side.
 * used to set the edge slide distance for ngons.
 */
static bool bm_loop_calc_opposite_co(BMLoop *l_tmp, const float plane_no[3], float r_co[3])
{
  /* skip adjacent edges */
  BMLoop *l_first = l_tmp->next;
  BMLoop *l_last = l_tmp->prev;
  BMLoop *l_iter;
  float dist = FLT_MAX;
  bool found = false;

  l_iter = l_first;
  do {
    float tvec[3];
    if (isect_line_plane_v3(tvec, l_iter->v->co, l_iter->next->v->co, l_tmp->v->co, plane_no)) {
      const float fac = line_point_factor_v3(tvec, l_iter->v->co, l_iter->next->v->co);
      /* allow some overlap to avoid missing the intersection because of float precision */
      if ((fac > -FLT_EPSILON) && (fac < 1.0f + FLT_EPSILON)) {
        /* likelihood of multiple intersections per ngon is quite low,
         * it would have to loop back on its self, but better support it
         * so check for the closest opposite edge */
        const float tdist = len_v3v3(l_tmp->v->co, tvec);
        if (tdist < dist) {
          copy_v3_v3(r_co, tvec);
          dist = tdist;
          found = true;
        }
      }
    }
  } while ((l_iter = l_iter->next) != l_last);

  return found;
}

/**
 * Given 2 edges and a loop, step over the loops
 * and calculate a direction to slide along.
 *
 * \param r_slide_vec: the direction to slide,
 * the length of the vector defines the slide distance.
 */
static BMLoop *get_next_loop(
    BMVert *v, BMLoop *l, BMEdge *e_prev, BMEdge *e_next, float r_slide_vec[3])
{
  BMLoop *l_first;
  float vec_accum[3] = {0.0f, 0.0f, 0.0f};
  float vec_accum_len = 0.0f;
  int i = 0;

  BLI_assert(BM_edge_share_vert(e_prev, e_next) == v);
  BLI_assert(BM_vert_in_edge(l->e, v));

  l_first = l;
  do {
    l = BM_loop_other_edge_loop(l, v);

    if (l->e == e_next) {
      if (i) {
        normalize_v3_length(vec_accum, vec_accum_len / (float)i);
      }
      else {
        /* When there is no edge to slide along,
         * we must slide along the vector defined by the face we're attach to */
        BMLoop *l_tmp = BM_face_vert_share_loop(l_first->f, v);

        BLI_assert(ELEM(l_tmp->e, e_prev, e_next) && ELEM(l_tmp->prev->e, e_prev, e_next));

        if (l_tmp->f->len == 4) {
          /* we could use code below, but in this case
           * sliding diagonally across the quad works well */
          sub_v3_v3v3(vec_accum, l_tmp->next->next->v->co, v->co);
        }
        else {
          float tdir[3];
          BM_loop_calc_face_direction(l_tmp, tdir);
          cross_v3_v3v3(vec_accum, l_tmp->f->no, tdir);
#if 0
          /* rough guess, we can  do better! */
          normalize_v3_length(vec_accum,
                              (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f);
#else
          /* be clever, check the opposite ngon edge to slide into.
           * this gives best results */
          {
            float tvec[3];
            float dist;

            if (bm_loop_calc_opposite_co(l_tmp, tdir, tvec)) {
              dist = len_v3v3(l_tmp->v->co, tvec);
            }
            else {
              dist = (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f;
            }

            normalize_v3_length(vec_accum, dist);
          }
#endif
        }
      }

      copy_v3_v3(r_slide_vec, vec_accum);
      return l;
    }
    else {
      /* accumulate the normalized edge vector,
       * normalize so some edges don't skew the result */
      float tvec[3];
      sub_v3_v3v3(tvec, BM_edge_other_vert(l->e, v)->co, v->co);
      vec_accum_len += normalize_v3(tvec);
      add_v3_v3(vec_accum, tvec);
      i += 1;
    }

    if (BM_loop_other_edge_loop(l, v)->e == e_next) {
      if (i) {
        normalize_v3_length(vec_accum, vec_accum_len / (float)i);
      }

      copy_v3_v3(r_slide_vec, vec_accum);
      return BM_loop_other_edge_loop(l, v);
    }

  } while ((l != l->radial_next) && ((l = l->radial_next) != l_first));

  if (i) {
    normalize_v3_length(vec_accum, vec_accum_len / (float)i);
  }

  copy_v3_v3(r_slide_vec, vec_accum);

  return NULL;
}

/**
 * Calculate screenspace `mval_start` / `mval_end`, optionally slide direction.
 */
static void calcEdgeSlide_mval_range(TransInfo *t,
                                     TransDataContainer *tc,
                                     EdgeSlideData *sld,
                                     const int *sv_table,
                                     const int loop_nr,
                                     const float mval[2],
                                     const bool use_occlude_geometry,
                                     const bool use_calc_direction)
{
  TransDataEdgeSlideVert *sv;
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  ARegion *region = t->region;
  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;
  float projectMat[4][4];
  BMBVHTree *bmbvh;

  /* only for use_calc_direction */
  float(*loop_dir)[3] = NULL, *loop_maxdist = NULL;

  float mval_start[2], mval_end[2];
  float mval_dir[3], dist_best_sq;

  if (t->spacetype == SPACE_VIEW3D) {
    /* background mode support */
    v3d = t->area ? t->area->spacedata.first : NULL;
    rv3d = t->region ? t->region->regiondata : NULL;
  }

  if (!rv3d) {
    /* ok, let's try to survive this */
    unit_m4(projectMat);
  }
  else {
    ED_view3d_ob_project_mat_get(rv3d, tc->obedit, projectMat);
  }

  if (use_occlude_geometry) {
    bmbvh = BKE_bmbvh_new_from_editmesh(em, BMBVH_RESPECT_HIDDEN, NULL, false);
  }
  else {
    bmbvh = NULL;
  }

  /* find mouse vectors, the global one, and one per loop in case we have
   * multiple loops selected, in case they are oriented different */
  zero_v3(mval_dir);
  dist_best_sq = -1.0f;

  if (use_calc_direction) {
    loop_dir = MEM_callocN(sizeof(float[3]) * loop_nr, "sv loop_dir");
    loop_maxdist = MEM_mallocN(sizeof(float) * loop_nr, "sv loop_maxdist");
    copy_vn_fl(loop_maxdist, loop_nr, -1.0f);
  }

  sv = &sld->sv[0];
  for (int i = 0; i < sld->totsv; i++, sv++) {
    BMIter iter_other;
    BMEdge *e;
    BMVert *v = sv->v;

    UNUSED_VARS_NDEBUG(sv_table); /* silence warning */
    BLI_assert(i == sv_table[BM_elem_index_get(v)]);

    /* search cross edges for visible edge to the mouse cursor,
     * then use the shared vertex to calculate screen vector*/
    BM_ITER_ELEM (e, &iter_other, v, BM_EDGES_OF_VERT) {
      /* screen-space coords */
      float sco_a[3], sco_b[3];
      float dist_sq;
      int l_nr;

      if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
        continue;
      }

      /* This test is only relevant if object is not wire-drawn! See [#32068]. */
      bool is_visible = !use_occlude_geometry ||
                        BMBVH_EdgeVisible(bmbvh, e, t->depsgraph, region, v3d, tc->obedit);

      if (!is_visible && !use_calc_direction) {
        continue;
      }

      if (sv->v_side[1]) {
        ED_view3d_project_float_v3_m4(region, sv->v_side[1]->co, sco_b, projectMat);
      }
      else {
        add_v3_v3v3(sco_b, v->co, sv->dir_side[1]);
        ED_view3d_project_float_v3_m4(region, sco_b, sco_b, projectMat);
      }

      if (sv->v_side[0]) {
        ED_view3d_project_float_v3_m4(region, sv->v_side[0]->co, sco_a, projectMat);
      }
      else {
        add_v3_v3v3(sco_a, v->co, sv->dir_side[0]);
        ED_view3d_project_float_v3_m4(region, sco_a, sco_a, projectMat);
      }

      /* global direction */
      dist_sq = dist_squared_to_line_segment_v2(mval, sco_b, sco_a);
      if (is_visible) {
        if ((dist_best_sq == -1.0f) ||
            /* intentionally use 2d size on 3d vector */
            (dist_sq < dist_best_sq && (len_squared_v2v2(sco_b, sco_a) > 0.1f))) {
          dist_best_sq = dist_sq;
          sub_v3_v3v3(mval_dir, sco_b, sco_a);
        }
      }

      if (use_calc_direction) {
        /* per loop direction */
        l_nr = sv->loop_nr;
        if (loop_maxdist[l_nr] == -1.0f || dist_sq < loop_maxdist[l_nr]) {
          loop_maxdist[l_nr] = dist_sq;
          sub_v3_v3v3(loop_dir[l_nr], sco_b, sco_a);
        }
      }
    }
  }

  if (use_calc_direction) {
    int i;
    sv = &sld->sv[0];
    for (i = 0; i < sld->totsv; i++, sv++) {
      /* switch a/b if loop direction is different from global direction */
      int l_nr = sv->loop_nr;
      if (dot_v3v3(loop_dir[l_nr], mval_dir) < 0.0f) {
        swap_v3_v3(sv->dir_side[0], sv->dir_side[1]);
        SWAP(BMVert *, sv->v_side[0], sv->v_side[1]);
      }
    }

    MEM_freeN(loop_dir);
    MEM_freeN(loop_maxdist);
  }

  /* possible all of the edge loops are pointing directly at the view */
  if (UNLIKELY(len_squared_v2(mval_dir) < 0.1f)) {
    mval_dir[0] = 0.0f;
    mval_dir[1] = 100.0f;
  }

  /* zero out start */
  zero_v2(mval_start);

  /* dir holds a vector along edge loop */
  copy_v2_v2(mval_end, mval_dir);
  mul_v2_fl(mval_end, 0.5f);

  sld->mval_start[0] = t->mval[0] + mval_start[0];
  sld->mval_start[1] = t->mval[1] + mval_start[1];

  sld->mval_end[0] = t->mval[0] + mval_end[0];
  sld->mval_end[1] = t->mval[1] + mval_end[1];

  if (bmbvh) {
    BKE_bmbvh_free(bmbvh);
  }
}

static void calcEdgeSlide_even(TransInfo *t,
                               TransDataContainer *tc,
                               EdgeSlideData *sld,
                               const float mval[2])
{
  TransDataEdgeSlideVert *sv = sld->sv;

  if (sld->totsv > 0) {
    ARegion *region = t->region;
    RegionView3D *rv3d = NULL;
    float projectMat[4][4];

    int i = 0;

    float v_proj[2];
    float dist_sq = 0;
    float dist_min_sq = FLT_MAX;

    if (t->spacetype == SPACE_VIEW3D) {
      /* background mode support */
      rv3d = t->region ? t->region->regiondata : NULL;
    }

    if (!rv3d) {
      /* ok, let's try to survive this */
      unit_m4(projectMat);
    }
    else {
      ED_view3d_ob_project_mat_get(rv3d, tc->obedit, projectMat);
    }

    for (i = 0; i < sld->totsv; i++, sv++) {
      /* Set length */
      sv->edge_len = len_v3v3(sv->dir_side[0], sv->dir_side[1]);

      ED_view3d_project_float_v2_m4(region, sv->v->co, v_proj, projectMat);
      dist_sq = len_squared_v2v2(mval, v_proj);
      if (dist_sq < dist_min_sq) {
        dist_min_sq = dist_sq;
        sld->curr_sv_index = i;
      }
    }
  }
  else {
    sld->curr_sv_index = 0;
  }
}

static EdgeSlideData *createEdgeSlideVerts_double_side(TransInfo *t, TransDataContainer *tc)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  BMIter iter;
  BMEdge *e;
  BMVert *v;
  TransDataEdgeSlideVert *sv_array;
  int sv_tot;
  int *sv_table; /* BMVert -> sv_array index */
  EdgeSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
  float mval[2] = {(float)t->mval[0], (float)t->mval[1]};
  int numsel, i, loop_nr;
  bool use_occlude_geometry = false;
  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;

  sld->curr_sv_index = 0;

  /*ensure valid selection*/
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      BMIter iter2;
      numsel = 0;
      BM_ITER_ELEM (e, &iter2, v, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          /* BMESH_TODO: this is probably very evil,
           * set v->e to a selected edge*/
          v->e = e;

          numsel++;
        }
      }

      if (numsel == 0 || numsel > 2) {
        /* Invalid edge selection. */
        MEM_freeN(sld);
        return NULL;
      }
    }
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      /* note, any edge with loops can work, but we won't get predictable results, so bail out */
      if (!BM_edge_is_manifold(e) && !BM_edge_is_boundary(e)) {
        /* can edges with at least once face user */
        MEM_freeN(sld);
        return NULL;
      }
    }
  }

  sv_table = MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

#define INDEX_UNSET -1
#define INDEX_INVALID -2

  {
    int j = 0;
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        BM_elem_flag_enable(v, BM_ELEM_TAG);
        sv_table[i] = INDEX_UNSET;
        j += 1;
      }
      else {
        BM_elem_flag_disable(v, BM_ELEM_TAG);
        sv_table[i] = INDEX_INVALID;
      }
      BM_elem_index_set(v, i); /* set_inline */
    }
    bm->elem_index_dirty &= ~BM_VERT;

    if (!j) {
      MEM_freeN(sld);
      MEM_freeN(sv_table);
      return NULL;
    }
    sv_tot = j;
  }

  sv_array = MEM_callocN(sizeof(TransDataEdgeSlideVert) * sv_tot, "sv_array");
  loop_nr = 0;

  STACK_DECLARE(sv_array);
  STACK_INIT(sv_array, sv_tot);

  while (1) {
    float vec_a[3], vec_b[3];
    BMLoop *l_a, *l_b;
    BMLoop *l_a_prev, *l_b_prev;
    BMVert *v_first;
    /* If this succeeds call get_next_loop()
     * which calculates the direction to slide based on clever checks.
     *
     * otherwise we simply use 'e_dir' as an edge-rail.
     * (which is better when the attached edge is a boundary, see: T40422)
     */
#define EDGESLIDE_VERT_IS_INNER(v, e_dir) \
  ((BM_edge_is_boundary(e_dir) == false) && (BM_vert_edge_count_nonwire(v) == 2))

    v = NULL;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        break;
      }
    }

    if (!v) {
      break;
    }

    if (!v->e) {
      continue;
    }

    v_first = v;

    /*walk along the edge loop*/
    e = v->e;

    /*first, rewind*/
    do {
      e = get_other_edge(v, e);
      if (!e) {
        e = v->e;
        break;
      }

      if (!BM_elem_flag_test(BM_edge_other_vert(e, v), BM_ELEM_TAG)) {
        break;
      }

      v = BM_edge_other_vert(e, v);
    } while (e != v_first->e);

    BM_elem_flag_disable(v, BM_ELEM_TAG);

    l_a = e->l;
    l_b = e->l->radial_next;

    /* regarding e_next, use get_next_loop()'s improved interpolation where possible */
    {
      BMEdge *e_next = get_other_edge(v, e);
      if (e_next) {
        get_next_loop(v, l_a, e, e_next, vec_a);
      }
      else {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
        if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
          get_next_loop(v, l_a, e, l_tmp->e, vec_a);
        }
        else {
          sub_v3_v3v3(vec_a, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
        }
      }
    }

    /* !BM_edge_is_boundary(e); */
    if (l_b != l_a) {
      BMEdge *e_next = get_other_edge(v, e);
      if (e_next) {
        get_next_loop(v, l_b, e, e_next, vec_b);
      }
      else {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
        if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
          get_next_loop(v, l_b, e, l_tmp->e, vec_b);
        }
        else {
          sub_v3_v3v3(vec_b, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
        }
      }
    }
    else {
      l_b = NULL;
    }

    l_a_prev = NULL;
    l_b_prev = NULL;

#define SV_FROM_VERT(v) \
  ((sv_table[BM_elem_index_get(v)] == INDEX_UNSET) ? \
       ((void)(sv_table[BM_elem_index_get(v)] = STACK_SIZE(sv_array)), \
        STACK_PUSH_RET_PTR(sv_array)) : \
       (&sv_array[sv_table[BM_elem_index_get(v)]]))

    /*iterate over the loop*/
    v_first = v;
    do {
      bool l_a_ok_prev;
      bool l_b_ok_prev;
      TransDataEdgeSlideVert *sv;
      BMVert *v_prev;
      BMEdge *e_prev;

      /* XXX, 'sv' will initialize multiple times, this is suspicious. see [#34024] */
      BLI_assert(v != NULL);
      BLI_assert(sv_table[BM_elem_index_get(v)] != INDEX_INVALID);
      sv = SV_FROM_VERT(v);
      sv->v = v;
      copy_v3_v3(sv->v_co_orig, v->co);
      sv->loop_nr = loop_nr;

      if (l_a || l_a_prev) {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_a ? l_a : l_a_prev, v);
        sv->v_side[0] = BM_edge_other_vert(l_tmp->e, v);
        copy_v3_v3(sv->dir_side[0], vec_a);
      }

      if (l_b || l_b_prev) {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_b ? l_b : l_b_prev, v);
        sv->v_side[1] = BM_edge_other_vert(l_tmp->e, v);
        copy_v3_v3(sv->dir_side[1], vec_b);
      }

      v_prev = v;
      v = BM_edge_other_vert(e, v);

      e_prev = e;
      e = get_other_edge(v, e);

      if (!e) {
        BLI_assert(v != NULL);

        BLI_assert(sv_table[BM_elem_index_get(v)] != INDEX_INVALID);
        sv = SV_FROM_VERT(v);

        sv->v = v;
        copy_v3_v3(sv->v_co_orig, v->co);
        sv->loop_nr = loop_nr;

        if (l_a) {
          BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
          sv->v_side[0] = BM_edge_other_vert(l_tmp->e, v);
          if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
            get_next_loop(v, l_a, e_prev, l_tmp->e, sv->dir_side[0]);
          }
          else {
            sub_v3_v3v3(sv->dir_side[0], sv->v_side[0]->co, v->co);
          }
        }

        if (l_b) {
          BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
          sv->v_side[1] = BM_edge_other_vert(l_tmp->e, v);
          if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
            get_next_loop(v, l_b, e_prev, l_tmp->e, sv->dir_side[1]);
          }
          else {
            sub_v3_v3v3(sv->dir_side[1], sv->v_side[1]->co, v->co);
          }
        }

        BM_elem_flag_disable(v, BM_ELEM_TAG);
        BM_elem_flag_disable(v_prev, BM_ELEM_TAG);

        break;
      }
      l_a_ok_prev = (l_a != NULL);
      l_b_ok_prev = (l_b != NULL);

      l_a_prev = l_a;
      l_b_prev = l_b;

      if (l_a) {
        l_a = get_next_loop(v, l_a, e_prev, e, vec_a);
      }
      else {
        zero_v3(vec_a);
      }

      if (l_b) {
        l_b = get_next_loop(v, l_b, e_prev, e, vec_b);
      }
      else {
        zero_v3(vec_b);
      }

      if (l_a && l_b) {
        /* pass */
      }
      else {
        if (l_a || l_b) {
          /* find the opposite loop if it was missing previously */
          if (l_a == NULL && l_b && (l_b->radial_next != l_b)) {
            l_a = l_b->radial_next;
          }
          else if (l_b == NULL && l_a && (l_a->radial_next != l_a)) {
            l_b = l_a->radial_next;
          }
        }
        else if (e->l != NULL) {
          /* if there are non-contiguous faces, we can still recover
           * the loops of the new edges faces */

          /* note!, the behavior in this case means edges may move in opposite directions,
           * this could be made to work more usefully. */

          if (l_a_ok_prev) {
            l_a = e->l;
            l_b = (l_a->radial_next != l_a) ? l_a->radial_next : NULL;
          }
          else if (l_b_ok_prev) {
            l_b = e->l;
            l_a = (l_b->radial_next != l_b) ? l_b->radial_next : NULL;
          }
        }

        if (!l_a_ok_prev && l_a) {
          get_next_loop(v, l_a, e, e_prev, vec_a);
        }
        if (!l_b_ok_prev && l_b) {
          get_next_loop(v, l_b, e, e_prev, vec_b);
        }
      }

      BM_elem_flag_disable(v, BM_ELEM_TAG);
      BM_elem_flag_disable(v_prev, BM_ELEM_TAG);
    } while ((e != v_first->e) && (l_a || l_b));

#undef SV_FROM_VERT
#undef INDEX_UNSET
#undef INDEX_INVALID

    loop_nr++;

#undef EDGESLIDE_VERT_IS_INNER
  }

  /* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

  BLI_assert(STACK_SIZE(sv_array) == (uint)sv_tot);

  sld->sv = sv_array;
  sld->totsv = sv_tot;

  /* use for visibility checks */
  if (t->spacetype == SPACE_VIEW3D) {
    v3d = t->area ? t->area->spacedata.first : NULL;
    rv3d = t->region ? t->region->regiondata : NULL;
    use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE &&
                            !XRAY_ENABLED(v3d));
  }

  calcEdgeSlide_mval_range(t, tc, sld, sv_table, loop_nr, mval, use_occlude_geometry, true);

  if (rv3d) {
    calcEdgeSlide_even(t, tc, sld, mval);
  }

  MEM_freeN(sv_table);

  return sld;
}

/**
 * A simple version of #createEdgeSlideVerts_double_side
 * Which assumes the longest unselected.
 */
static EdgeSlideData *createEdgeSlideVerts_single_side(TransInfo *t, TransDataContainer *tc)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  BMIter iter;
  BMEdge *e;
  TransDataEdgeSlideVert *sv_array;
  int sv_tot;
  int *sv_table; /* BMVert -> sv_array index */
  EdgeSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
  float mval[2] = {(float)t->mval[0], (float)t->mval[1]};
  int loop_nr;
  bool use_occlude_geometry = false;
  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;

  if (t->spacetype == SPACE_VIEW3D) {
    /* background mode support */
    v3d = t->area ? t->area->spacedata.first : NULL;
    rv3d = t->region ? t->region->regiondata : NULL;
  }

  sld->curr_sv_index = 0;
  /* ensure valid selection */
  {
    int i = 0, j = 0;
    BMVert *v;

    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        float len_sq_max = -1.0f;
        BMIter iter2;
        BM_ITER_ELEM (e, &iter2, v, BM_EDGES_OF_VERT) {
          if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
            float len_sq = BM_edge_calc_length_squared(e);
            if (len_sq > len_sq_max) {
              len_sq_max = len_sq;
              v->e = e;
            }
          }
        }

        if (len_sq_max != -1.0f) {
          j++;
        }
      }
      BM_elem_index_set(v, i); /* set_inline */
    }
    bm->elem_index_dirty &= ~BM_VERT;

    if (!j) {
      MEM_freeN(sld);
      return NULL;
    }

    sv_tot = j;
  }

  BLI_assert(sv_tot != 0);
  /* over alloc */
  sv_array = MEM_callocN(sizeof(TransDataEdgeSlideVert) * bm->totvertsel, "sv_array");

  /* same loop for all loops, weak but we dont connect loops in this case */
  loop_nr = 1;

  sv_table = MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

  {
    int i = 0, j = 0;
    BMVert *v;

    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      sv_table[i] = -1;
      if ((v->e != NULL) && (BM_elem_flag_test(v, BM_ELEM_SELECT))) {
        if (BM_elem_flag_test(v->e, BM_ELEM_SELECT) == 0) {
          TransDataEdgeSlideVert *sv;
          sv = &sv_array[j];
          sv->v = v;
          copy_v3_v3(sv->v_co_orig, v->co);
          sv->v_side[0] = BM_edge_other_vert(v->e, v);
          sub_v3_v3v3(sv->dir_side[0], sv->v_side[0]->co, v->co);
          sv->loop_nr = 0;
          sv_table[i] = j;
          j += 1;
        }
      }
    }
  }

  /* check for wire vertices,
   * interpolate the directions of wire verts between non-wire verts */
  if (sv_tot != bm->totvert) {
    const int sv_tot_nowire = sv_tot;
    TransDataEdgeSlideVert *sv_iter = sv_array;

    for (int i = 0; i < sv_tot_nowire; i++, sv_iter++) {
      BMIter eiter;
      BM_ITER_ELEM (e, &eiter, sv_iter->v, BM_EDGES_OF_VERT) {
        /* walk over wire */
        TransDataEdgeSlideVert *sv_end = NULL;
        BMEdge *e_step = e;
        BMVert *v = sv_iter->v;
        int j;

        j = sv_tot;

        while (1) {
          BMVert *v_other = BM_edge_other_vert(e_step, v);
          int endpoint = ((sv_table[BM_elem_index_get(v_other)] != -1) +
                          (BM_vert_is_edge_pair(v_other) == false));

          if ((BM_elem_flag_test(e_step, BM_ELEM_SELECT) &&
               BM_elem_flag_test(v_other, BM_ELEM_SELECT)) &&
              (endpoint == 0)) {
            /* scan down the list */
            TransDataEdgeSlideVert *sv;
            BLI_assert(sv_table[BM_elem_index_get(v_other)] == -1);
            sv_table[BM_elem_index_get(v_other)] = j;
            sv = &sv_array[j];
            sv->v = v_other;
            copy_v3_v3(sv->v_co_orig, v_other->co);
            copy_v3_v3(sv->dir_side[0], sv_iter->dir_side[0]);
            j++;

            /* advance! */
            v = v_other;
            e_step = BM_DISK_EDGE_NEXT(e_step, v_other);
          }
          else {
            if ((endpoint == 2) && (sv_tot != j)) {
              BLI_assert(BM_elem_index_get(v_other) != -1);
              sv_end = &sv_array[sv_table[BM_elem_index_get(v_other)]];
            }
            break;
          }
        }

        if (sv_end) {
          int sv_tot_prev = sv_tot;
          const float *co_src = sv_iter->v->co;
          const float *co_dst = sv_end->v->co;
          const float *dir_src = sv_iter->dir_side[0];
          const float *dir_dst = sv_end->dir_side[0];
          sv_tot = j;

          while (j-- != sv_tot_prev) {
            float factor;
            factor = line_point_factor_v3(sv_array[j].v->co, co_src, co_dst);
            interp_v3_v3v3(sv_array[j].dir_side[0], dir_src, dir_dst, factor);
          }
        }
      }
    }
  }

  /* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

  sld->sv = sv_array;
  sld->totsv = sv_tot;

  /* use for visibility checks */
  if (t->spacetype == SPACE_VIEW3D) {
    v3d = t->area ? t->area->spacedata.first : NULL;
    rv3d = t->region ? t->region->regiondata : NULL;
    use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE &&
                            !XRAY_ENABLED(v3d));
  }

  calcEdgeSlide_mval_range(t, tc, sld, sv_table, loop_nr, mval, use_occlude_geometry, false);

  if (rv3d) {
    calcEdgeSlide_even(t, tc, sld, mval);
  }

  MEM_freeN(sv_table);

  return sld;
}

static void freeEdgeSlideVerts(TransInfo *UNUSED(t),
                               TransDataContainer *UNUSED(tc),
                               TransCustomData *custom_data)
{
  EdgeSlideData *sld = custom_data->data;

  if (sld == NULL) {
    return;
  }

  MEM_freeN(sld->sv);
  MEM_freeN(sld);

  custom_data->data = NULL;
}

static eRedrawFlag handleEventEdgeSlide(struct TransInfo *t, const struct wmEvent *event)
{
  if (t->mode == TFM_EDGE_SLIDE) {
    EdgeSlideParams *slp = t->custom.mode.data;

    if (slp) {
      switch (event->type) {
        case EVT_EKEY:
          if (event->val == KM_PRESS) {
            slp->use_even = !slp->use_even;
            calcEdgeSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case EVT_FKEY:
          if (event->val == KM_PRESS) {
            slp->flipped = !slp->flipped;
            calcEdgeSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case EVT_CKEY:
          /* use like a modifier key */
          if (event->val == KM_PRESS) {
            t->flag ^= T_ALT_TRANSFORM;
            calcEdgeSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case EVT_MODAL_MAP:
#if 0
          switch (event->val) {
            case TFM_MODAL_EDGESLIDE_DOWN:
              sld->curr_sv_index = ((sld->curr_sv_index - 1) + sld->totsv) % sld->totsv;
              return TREDRAW_HARD;
            case TFM_MODAL_EDGESLIDE_UP:
              sld->curr_sv_index = (sld->curr_sv_index + 1) % sld->totsv;
              return TREDRAW_HARD;
          }
#endif
          break;
        case MOUSEMOVE:
          calcEdgeSlideCustomPoints(t);
          break;
        default:
          break;
      }
    }
  }
  return TREDRAW_NOTHING;
}

void drawEdgeSlide(TransInfo *t)
{
  if ((t->mode == TFM_EDGE_SLIDE) && edgeSlideFirstGet(t)) {
    const EdgeSlideParams *slp = t->custom.mode.data;
    EdgeSlideData *sld = edgeSlideFirstGet(t);
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

    /* Even mode */
    if ((slp->use_even == true) || (is_clamp == false)) {
      const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;

      GPU_depth_test(false);

      GPU_blend(true);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

      GPU_matrix_push();
      GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);

      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      if (slp->use_even == true) {
        float co_a[3], co_b[3], co_mark[3];
        TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
        const float fac = (slp->perc + 1.0f) / 2.0f;
        const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
        const float guide_size = ctrl_size - 0.5f;
        const int alpha_shade = -30;

        add_v3_v3v3(co_a, curr_sv->v_co_orig, curr_sv->dir_side[0]);
        add_v3_v3v3(co_b, curr_sv->v_co_orig, curr_sv->dir_side[1]);

        GPU_line_width(line_size);
        immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
        immBeginAtMost(GPU_PRIM_LINES, 4);
        if (curr_sv->v_side[0]) {
          immVertex3fv(pos, curr_sv->v_side[0]->co);
          immVertex3fv(pos, curr_sv->v_co_orig);
        }
        if (curr_sv->v_side[1]) {
          immVertex3fv(pos, curr_sv->v_side[1]->co);
          immVertex3fv(pos, curr_sv->v_co_orig);
        }
        immEnd();

        {
          float *co_test = NULL;
          if (slp->flipped) {
            if (curr_sv->v_side[1]) {
              co_test = curr_sv->v_side[1]->co;
            }
          }
          else {
            if (curr_sv->v_side[0]) {
              co_test = curr_sv->v_side[0]->co;
            }
          }

          if (co_test != NULL) {
            immUniformThemeColorShadeAlpha(TH_SELECT, -30, alpha_shade);
            GPU_point_size(ctrl_size);
            immBegin(GPU_PRIM_POINTS, 1);
            immVertex3fv(pos, co_test);
            immEnd();
          }
        }

        immUniformThemeColorShadeAlpha(TH_SELECT, 255, alpha_shade);
        GPU_point_size(guide_size);
        immBegin(GPU_PRIM_POINTS, 1);
        interp_line_v3_v3v3v3(co_mark, co_b, curr_sv->v_co_orig, co_a, fac);
        immVertex3fv(pos, co_mark);
        immEnd();
      }
      else {
        if (is_clamp == false) {
          const int side_index = sld->curr_side_unclamp;
          TransDataEdgeSlideVert *sv;
          int i;
          const int alpha_shade = -160;

          GPU_line_width(line_size);
          immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
          immBegin(GPU_PRIM_LINES, sld->totsv * 2);

          /* TODO(campbell): Loop over all verts  */
          sv = sld->sv;
          for (i = 0; i < sld->totsv; i++, sv++) {
            float a[3], b[3];

            if (!is_zero_v3(sv->dir_side[side_index])) {
              copy_v3_v3(a, sv->dir_side[side_index]);
            }
            else {
              copy_v3_v3(a, sv->dir_side[!side_index]);
            }

            mul_v3_fl(a, 100.0f);
            negate_v3_v3(b, a);
            add_v3_v3(a, sv->v_co_orig);
            add_v3_v3(b, sv->v_co_orig);

            immVertex3fv(pos, a);
            immVertex3fv(pos, b);
          }
          immEnd();
        }
        else {
          BLI_assert(0);
        }
      }

      immUnbindProgram();

      GPU_matrix_pop();

      GPU_blend(false);

      GPU_depth_test(true);
    }
  }
}

static void edge_slide_snap_apply(TransInfo *t, float *value)
{
  TransDataContainer *tc = edge_slide_container_first_ok(t);
  EdgeSlideParams *slp = t->custom.mode.data;
  EdgeSlideData *sld_active = tc->custom.mode.data;
  TransDataEdgeSlideVert *sv = &sld_active->sv[sld_active->curr_sv_index];
  float snap_point[3], co_orig[3], co_dest[2][3], dvec[3];

  copy_v3_v3(co_orig, sv->v_co_orig);
  add_v3_v3v3(co_dest[0], co_orig, sv->dir_side[0]);
  add_v3_v3v3(co_dest[1], co_orig, sv->dir_side[1]);
  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, co_orig);
    mul_m4_v3(tc->mat, co_dest[0]);
    mul_m4_v3(tc->mat, co_dest[1]);
  }

  getSnapPoint(t, dvec);
  sub_v3_v3(dvec, t->tsnap.snapTarget);
  add_v3_v3v3(snap_point, co_orig, dvec);

  float perc = *value;
  int side_index;
  if (slp->use_even == false) {
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
    if (is_clamp) {
      side_index = perc < 0.0f;
    }
    else {
      side_index = sld_active->curr_side_unclamp;
    }
    perc = line_point_factor_v3(snap_point, co_orig, co_dest[side_index]);
    if (side_index) {
      perc *= -1;
    }
  }
  else {
    /* Could be pre-calculated. */
    float t_mid = line_point_factor_v3(
        (float[3]){0.0f, 0.0f, 0.0f}, sv->dir_side[0], sv->dir_side[1]);

    float t_snap = line_point_factor_v3(snap_point, co_dest[0], co_dest[1]);
    side_index = t_snap >= t_mid;
    perc = line_point_factor_v3(snap_point, co_orig, co_dest[side_index]);
    if (!side_index) {
      perc = (1.0f - perc) * t_mid;
    }
    else {
      perc = perc * (1.0f - t_mid) + t_mid;
    }

    if (slp->flipped) {
      perc = 1.0f - perc;
    }

    perc = (2 * perc) - 1.0f;

    if (!slp->flipped) {
      perc *= -1;
    }
  }

  *value = perc;
}

void doEdgeSlide(TransInfo *t, float perc)
{
  EdgeSlideParams *slp = t->custom.mode.data;
  EdgeSlideData *sld_active = edgeSlideFirstGet(t);

  slp->perc = perc;

  if (slp->use_even == false) {
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
    if (is_clamp) {
      const int side_index = (perc < 0.0f);
      const float perc_final = fabsf(perc);
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        EdgeSlideData *sld = tc->custom.mode.data;

        if (sld == NULL) {
          continue;
        }

        TransDataEdgeSlideVert *sv = sld->sv;
        for (int i = 0; i < sld->totsv; i++, sv++) {
          madd_v3_v3v3fl(sv->v->co, sv->v_co_orig, sv->dir_side[side_index], perc_final);
        }
        sld->curr_side_unclamp = side_index;
      }
    }
    else {
      const float perc_init = fabsf(perc) *
                              ((sld_active->curr_side_unclamp == (perc < 0.0f)) ? 1 : -1);
      const int side_index = sld_active->curr_side_unclamp;
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        EdgeSlideData *sld = tc->custom.mode.data;

        if (sld == NULL) {
          continue;
        }

        TransDataEdgeSlideVert *sv = sld->sv;
        for (int i = 0; i < sld->totsv; i++, sv++) {
          float dir_flip[3];
          float perc_final = perc_init;
          if (!is_zero_v3(sv->dir_side[side_index])) {
            copy_v3_v3(dir_flip, sv->dir_side[side_index]);
          }
          else {
            copy_v3_v3(dir_flip, sv->dir_side[!side_index]);
            perc_final *= -1;
          }
          madd_v3_v3v3fl(sv->v->co, sv->v_co_orig, dir_flip, perc_final);
        }
      }
    }
  }
  else {
    /**
     * Implementation note, even mode ignores the starting positions and uses
     * only the a/b verts, this could be changed/improved so the distance is
     * still met but the verts are moved along their original path (which may not be straight),
     * however how it works now is OK and matches 2.4x - Campbell
     *
     * \note `len_v3v3(curr_sv->dir_side[0], curr_sv->dir_side[1])`
     * is the same as the distance between the original vert locations,
     * same goes for the lines below.
     */
    TransDataEdgeSlideVert *curr_sv = &sld_active->sv[sld_active->curr_sv_index];
    const float curr_length_perc = curr_sv->edge_len *
                                   (((slp->flipped ? perc : -perc) + 1.0f) / 2.0f);

    float co_a[3];
    float co_b[3];

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      EdgeSlideData *sld = tc->custom.mode.data;

      if (sld == NULL) {
        continue;
      }

      TransDataEdgeSlideVert *sv = sld->sv;
      for (int i = 0; i < sld->totsv; i++, sv++) {
        if (sv->edge_len > FLT_EPSILON) {
          const float fac = min_ff(sv->edge_len, curr_length_perc) / sv->edge_len;

          add_v3_v3v3(co_a, sv->v_co_orig, sv->dir_side[0]);
          add_v3_v3v3(co_b, sv->v_co_orig, sv->dir_side[1]);

          if (slp->flipped) {
            interp_line_v3_v3v3v3(sv->v->co, co_b, sv->v_co_orig, co_a, fac);
          }
          else {
            interp_line_v3_v3v3v3(sv->v->co, co_a, sv->v_co_orig, co_b, fac);
          }
        }
      }
    }
  }
}

static void applyEdgeSlide(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  EdgeSlideParams *slp = t->custom.mode.data;
  bool flipped = slp->flipped;
  bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

  final = t->values[0];

  applySnapping(t, &final);
  snapGridIncrement(t, &final);

  /* only do this so out of range values are not displayed */
  if (is_constrained) {
    CLAMP(final, -1.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values_final[0] = final;

  /* header string */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Edge Slide: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, &t->scene->unit);
    ofs += BLI_strncpy_rlen(str + ofs, &c[0], sizeof(str) - ofs);
  }
  else {
    ofs += BLI_snprintf(str + ofs, sizeof(str) - ofs, "%.4f ", final);
  }
  ofs += BLI_snprintf(
      str + ofs, sizeof(str) - ofs, TIP_("(E)ven: %s, "), WM_bool_as_string(use_even));
  if (use_even) {
    ofs += BLI_snprintf(
        str + ofs, sizeof(str) - ofs, TIP_("(F)lipped: %s, "), WM_bool_as_string(flipped));
  }
  ofs += BLI_snprintf(
      str + ofs, sizeof(str) - ofs, TIP_("Alt or (C)lamp: %s"), WM_bool_as_string(is_clamp));
  /* done with header string */

  /* do stuff here */
  doEdgeSlide(t, final);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initEdgeSlide_ex(
    TransInfo *t, bool use_double_side, bool use_even, bool flipped, bool use_clamp)
{
  EdgeSlideData *sld;
  bool ok = false;

  t->mode = TFM_EDGE_SLIDE;
  t->transform = applyEdgeSlide;
  t->handleEvent = handleEventEdgeSlide;
  t->tsnap.applySnap = edge_slide_snap_apply;
  t->tsnap.distance = transform_snap_distance_len_squared_fn;

  {
    EdgeSlideParams *slp = MEM_callocN(sizeof(*slp), __func__);
    slp->use_even = use_even;
    slp->flipped = flipped;
    /* happens to be best for single-sided */
    if (use_double_side == false) {
      slp->flipped = !flipped;
    }
    slp->perc = 0.0f;

    if (!use_clamp) {
      t->flag |= T_ALT_TRANSFORM;
    }

    t->custom.mode.data = slp;
    t->custom.mode.use_free = true;
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sld = use_double_side ? createEdgeSlideVerts_double_side(t, tc) :
                            createEdgeSlideVerts_single_side(t, tc);
    if (sld) {
      tc->custom.mode.data = sld;
      tc->custom.mode.free_cb = freeEdgeSlideVerts;
      trans_mesh_customdata_correction_init(t, tc);
      ok = true;
    }
  }

  if (!ok) {
    t->state = TRANS_CANCEL;
    return;
  }

  /* set custom point first if you want value to be initialized by init */
  calcEdgeSlideCustomPoints(t);
  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO_FLIP);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

void initEdgeSlide(TransInfo *t)
{
  initEdgeSlide_ex(t, true, false, false, true);
}
/** \} */
