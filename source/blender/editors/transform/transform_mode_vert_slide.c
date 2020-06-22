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

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_unit.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_constraints.h"
#include "transform_convert.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Vert Slide) */

/** \name Transform Vert Slide
 * \{ */

typedef struct TransDataVertSlideVert {
  /** #TransDataGenericSlideVert (header) */
  struct BMVert *v;
  struct LinkNode **cd_loop_groups;
  float co_orig_3d[3];
  /* end generic */

  float (*co_link_orig_3d)[3];
  int co_link_tot;
  int co_link_curr;
} TransDataVertSlideVert;

typedef struct VertSlideData {
  TransDataVertSlideVert *sv;
  int totsv;
  int curr_sv_index;

  /* result of ED_view3d_ob_project_mat_get */
  float proj_mat[4][4];
} VertSlideData;

typedef struct VertSlideParams {
  float perc;

  bool use_even;
  bool flipped;
} VertSlideParams;

static void calcVertSlideCustomPoints(struct TransInfo *t)
{
  VertSlideParams *slp = t->custom.mode.data;
  VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
  TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];

  const float *co_orig_3d = sv->co_orig_3d;
  const float *co_curr_3d = sv->co_link_orig_3d[sv->co_link_curr];

  float co_curr_2d[2], co_orig_2d[2];

  int mval_ofs[2], mval_start[2], mval_end[2];

  ED_view3d_project_float_v2_m4(t->region, co_orig_3d, co_orig_2d, sld->proj_mat);
  ED_view3d_project_float_v2_m4(t->region, co_curr_3d, co_curr_2d, sld->proj_mat);

  ARRAY_SET_ITEMS(mval_ofs, t->mouse.imval[0] - co_orig_2d[0], t->mouse.imval[1] - co_orig_2d[1]);
  ARRAY_SET_ITEMS(mval_start, co_orig_2d[0] + mval_ofs[0], co_orig_2d[1] + mval_ofs[1]);
  ARRAY_SET_ITEMS(mval_end, co_curr_2d[0] + mval_ofs[0], co_curr_2d[1] + mval_ofs[1]);

  if (slp->flipped && slp->use_even) {
    setCustomPoints(t, &t->mouse, mval_start, mval_end);
  }
  else {
    setCustomPoints(t, &t->mouse, mval_end, mval_start);
  }

  /* setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

/**
 * Run once when initializing vert slide to find the reference edge
 */
static void calcVertSlideMouseActiveVert(struct TransInfo *t, const int mval[2])
{
  /* Active object may have no selected vertices. */
  VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
  float mval_fl[2] = {UNPACK2(mval)};
  TransDataVertSlideVert *sv;

  /* set the vertex to use as a reference for the mouse direction 'curr_sv_index' */
  float dist_sq = 0.0f;
  float dist_min_sq = FLT_MAX;
  int i;

  for (i = 0, sv = sld->sv; i < sld->totsv; i++, sv++) {
    float co_2d[2];

    ED_view3d_project_float_v2_m4(t->region, sv->co_orig_3d, co_2d, sld->proj_mat);

    dist_sq = len_squared_v2v2(mval_fl, co_2d);
    if (dist_sq < dist_min_sq) {
      dist_min_sq = dist_sq;
      sld->curr_sv_index = i;
    }
  }
}

/**
 * Run while moving the mouse to slide along the edge matching the mouse direction
 */
static void calcVertSlideMouseActiveEdges(struct TransInfo *t, const int mval[2])
{
  VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
  float imval_fl[2] = {UNPACK2(t->mouse.imval)};
  float mval_fl[2] = {UNPACK2(mval)};

  float dir[3];
  TransDataVertSlideVert *sv;
  int i;

  /* note: we could save a matrix-multiply for each vertex
   * by finding the closest edge in local-space.
   * However this skews the outcome with non-uniform-scale. */

  /* first get the direction of the original mouse position */
  sub_v2_v2v2(dir, imval_fl, mval_fl);
  ED_view3d_win_to_delta(t->region, dir, dir, t->zfac);
  normalize_v3(dir);

  for (i = 0, sv = sld->sv; i < sld->totsv; i++, sv++) {
    if (sv->co_link_tot > 1) {
      float dir_dot_best = -FLT_MAX;
      int co_link_curr_best = -1;
      int j;

      for (j = 0; j < sv->co_link_tot; j++) {
        float tdir[3];
        float dir_dot;

        sub_v3_v3v3(tdir, sv->co_orig_3d, sv->co_link_orig_3d[j]);
        mul_mat3_m4_v3(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat, tdir);
        project_plane_v3_v3v3(tdir, tdir, t->viewinv[2]);

        normalize_v3(tdir);
        dir_dot = dot_v3v3(dir, tdir);
        if (dir_dot > dir_dot_best) {
          dir_dot_best = dir_dot;
          co_link_curr_best = j;
        }
      }

      if (co_link_curr_best != -1) {
        sv->co_link_curr = co_link_curr_best;
      }
    }
  }
}

static VertSlideData *createVertSlideVerts(TransInfo *t, const TransDataContainer *tc)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  BMIter iter;
  BMIter eiter;
  BMEdge *e;
  BMVert *v;
  TransDataVertSlideVert *sv_array;
  VertSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
  int j;

  sld->curr_sv_index = 0;

  j = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    bool ok = false;
    if (BM_elem_flag_test(v, BM_ELEM_SELECT) && v->e) {
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          ok = true;
          break;
        }
      }
    }

    if (ok) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
      j += 1;
    }
    else {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
    }
  }

  if (!j) {
    MEM_freeN(sld);
    return NULL;
  }

  sv_array = MEM_callocN(sizeof(TransDataVertSlideVert) * j, "sv_array");

  j = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      int k;
      sv_array[j].v = v;
      copy_v3_v3(sv_array[j].co_orig_3d, v->co);

      k = 0;
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          k++;
        }
      }

      sv_array[j].co_link_orig_3d = MEM_mallocN(sizeof(*sv_array[j].co_link_orig_3d) * k,
                                                __func__);
      sv_array[j].co_link_tot = k;

      k = 0;
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          BMVert *v_other = BM_edge_other_vert(e, v);
          copy_v3_v3(sv_array[j].co_link_orig_3d[k], v_other->co);
          k++;
        }
      }
      j++;
    }
  }

  sld->sv = sv_array;
  sld->totsv = j;

  /* most likely will be set below */
  unit_m4(sld->proj_mat);

  if (t->spacetype == SPACE_VIEW3D) {
    /* view vars */
    RegionView3D *rv3d = NULL;
    ARegion *region = t->region;

    rv3d = region ? region->regiondata : NULL;
    if (rv3d) {
      ED_view3d_ob_project_mat_get(rv3d, tc->obedit, sld->proj_mat);
    }
  }

  return sld;
}

static void freeVertSlideVerts(TransInfo *UNUSED(t),
                               TransDataContainer *UNUSED(tc),
                               TransCustomData *custom_data)
{
  VertSlideData *sld = custom_data->data;

  if (!sld) {
    return;
  }

  if (sld->totsv > 0) {
    TransDataVertSlideVert *sv = sld->sv;
    int i = 0;
    for (i = 0; i < sld->totsv; i++, sv++) {
      MEM_freeN(sv->co_link_orig_3d);
    }
  }

  MEM_freeN(sld->sv);
  MEM_freeN(sld);

  custom_data->data = NULL;
}

static eRedrawFlag handleEventVertSlide(struct TransInfo *t, const struct wmEvent *event)
{
  if (t->mode == TFM_VERT_SLIDE) {
    VertSlideParams *slp = t->custom.mode.data;

    if (slp) {
      switch (event->type) {
        case EVT_EKEY:
          if (event->val == KM_PRESS) {
            slp->use_even = !slp->use_even;
            if (slp->flipped) {
              calcVertSlideCustomPoints(t);
            }
            return TREDRAW_HARD;
          }
          break;
        case EVT_FKEY:
          if (event->val == KM_PRESS) {
            slp->flipped = !slp->flipped;
            calcVertSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case EVT_CKEY:
          /* use like a modifier key */
          if (event->val == KM_PRESS) {
            t->flag ^= T_ALT_TRANSFORM;
            calcVertSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
#if 0
        case EVT_MODAL_MAP:
          switch (event->val) {
            case TFM_MODAL_EDGESLIDE_DOWN:
              sld->curr_sv_index = ((sld->curr_sv_index - 1) + sld->totsv) % sld->totsv;
              break;
            case TFM_MODAL_EDGESLIDE_UP:
              sld->curr_sv_index = (sld->curr_sv_index + 1) % sld->totsv;
              break;
          }
          break;
#endif
        case MOUSEMOVE: {
          /* don't recalculate the best edge */
          const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
          if (is_clamp) {
            calcVertSlideMouseActiveEdges(t, event->mval);
          }
          calcVertSlideCustomPoints(t);
          break;
        }
        default:
          break;
      }
    }
  }
  return TREDRAW_NOTHING;
}

void drawVertSlide(TransInfo *t)
{
  if ((t->mode == TFM_VERT_SLIDE) && TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data) {
    const VertSlideParams *slp = t->custom.mode.data;
    VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

    /* Non-Prop mode */
    {
      TransDataVertSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
      TransDataVertSlideVert *sv;
      const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
      const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;
      const int alpha_shade = -160;
      int i;

      GPU_depth_test(false);

      GPU_blend(true);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

      GPU_matrix_push();
      GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);

      GPU_line_width(line_size);

      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);

      immBegin(GPU_PRIM_LINES, sld->totsv * 2);
      if (is_clamp) {
        sv = sld->sv;
        for (i = 0; i < sld->totsv; i++, sv++) {
          immVertex3fv(shdr_pos, sv->co_orig_3d);
          immVertex3fv(shdr_pos, sv->co_link_orig_3d[sv->co_link_curr]);
        }
      }
      else {
        sv = sld->sv;
        for (i = 0; i < sld->totsv; i++, sv++) {
          float a[3], b[3];
          sub_v3_v3v3(a, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
          mul_v3_fl(a, 100.0f);
          negate_v3_v3(b, a);
          add_v3_v3(a, sv->co_orig_3d);
          add_v3_v3(b, sv->co_orig_3d);

          immVertex3fv(shdr_pos, a);
          immVertex3fv(shdr_pos, b);
        }
      }
      immEnd();

      GPU_point_size(ctrl_size);

      immBegin(GPU_PRIM_POINTS, 1);
      immVertex3fv(shdr_pos,
                   (slp->flipped && slp->use_even) ?
                       curr_sv->co_link_orig_3d[curr_sv->co_link_curr] :
                       curr_sv->co_orig_3d);
      immEnd();

      immUnbindProgram();

      /* direction from active vertex! */
      if ((t->mval[0] != t->mouse.imval[0]) || (t->mval[1] != t->mouse.imval[1])) {
        float zfac;
        float mval_ofs[2];
        float co_orig_3d[3];
        float co_dest_3d[3];

        mval_ofs[0] = t->mval[0] - t->mouse.imval[0];
        mval_ofs[1] = t->mval[1] - t->mouse.imval[1];

        mul_v3_m4v3(
            co_orig_3d, TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat, curr_sv->co_orig_3d);
        zfac = ED_view3d_calc_zfac(t->region->regiondata, co_orig_3d, NULL);

        ED_view3d_win_to_delta(t->region, mval_ofs, co_dest_3d, zfac);

        invert_m4_m4(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->imat,
                     TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);
        mul_mat3_m4_v3(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->imat, co_dest_3d);

        add_v3_v3(co_dest_3d, curr_sv->co_orig_3d);

        GPU_line_width(1.0f);

        immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

        float viewport_size[4];
        GPU_viewport_size_get_f(viewport_size);
        immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

        immUniform1i("colors_len", 0); /* "simple" mode */
        immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        immUniform1f("dash_width", 6.0f);
        immUniform1f("dash_factor", 0.5f);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex3fv(shdr_pos, curr_sv->co_orig_3d);
        immVertex3fv(shdr_pos, co_dest_3d);
        immEnd();

        immUnbindProgram();
      }

      GPU_matrix_pop();

      GPU_depth_test(true);
    }
  }
}

void doVertSlide(TransInfo *t, float perc)
{
  VertSlideParams *slp = t->custom.mode.data;

  slp->perc = perc;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    VertSlideData *sld = tc->custom.mode.data;
    TransDataVertSlideVert *svlist = sld->sv, *sv;
    int i;

    sv = svlist;

    if (slp->use_even == false) {
      for (i = 0; i < sld->totsv; i++, sv++) {
        interp_v3_v3v3(sv->v->co, sv->co_orig_3d, sv->co_link_orig_3d[sv->co_link_curr], perc);
      }
    }
    else {
      TransDataVertSlideVert *sv_curr = &sld->sv[sld->curr_sv_index];
      const float edge_len_curr = len_v3v3(sv_curr->co_orig_3d,
                                           sv_curr->co_link_orig_3d[sv_curr->co_link_curr]);
      const float tperc = perc * edge_len_curr;

      for (i = 0; i < sld->totsv; i++, sv++) {
        float edge_len;
        float dir[3];

        sub_v3_v3v3(dir, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
        edge_len = normalize_v3(dir);

        if (edge_len > FLT_EPSILON) {
          if (slp->flipped) {
            madd_v3_v3v3fl(sv->v->co, sv->co_link_orig_3d[sv->co_link_curr], dir, -tperc);
          }
          else {
            madd_v3_v3v3fl(sv->v->co, sv->co_orig_3d, dir, tperc);
          }
        }
        else {
          copy_v3_v3(sv->v->co, sv->co_orig_3d);
        }
      }
    }
  }
}

static void vert_slide_snap_apply(TransInfo *t, float *value)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
  VertSlideData *sld = tc->custom.mode.data;
  TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];

  float snap_point[3], co_orig_3d[3], co_curr_3d[3], dvec[3];
  copy_v3_v3(co_orig_3d, sv->co_orig_3d);
  copy_v3_v3(co_curr_3d, sv->co_link_orig_3d[sv->co_link_curr]);
  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, co_orig_3d);
    mul_m4_v3(tc->mat, co_curr_3d);
  }

  getSnapPoint(t, dvec);
  sub_v3_v3(dvec, t->tsnap.snapTarget);
  if (t->tsnap.snapElem & (SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE)) {
    float co_dir_3d[3];
    sub_v3_v3v3(co_dir_3d, co_curr_3d, co_orig_3d);
    if (t->tsnap.snapElem & SCE_SNAP_MODE_EDGE) {
      transform_constraint_snap_axis_to_edge(t, co_dir_3d, dvec);
    }
    else {
      transform_constraint_snap_axis_to_face(t, co_dir_3d, dvec);
    }
  }

  add_v3_v3v3(snap_point, co_orig_3d, dvec);
  *value = line_point_factor_v3(snap_point, co_orig_3d, co_curr_3d);
}

static void applyVertSlide(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  VertSlideParams *slp = t->custom.mode.data;
  const bool flipped = slp->flipped;
  const bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

  final = t->values[0];

  applySnapping(t, &final);
  snapGridIncrement(t, &final);

  /* only do this so out of range values are not displayed */
  if (is_constrained) {
    CLAMP(final, 0.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values_final[0] = final;

  /* header string */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Vert Slide: "), sizeof(str) - ofs);
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
  doVertSlide(t, final);

  recalcData(t);

  ED_area_status_text(t->area, str);
}

void initVertSlide_ex(TransInfo *t, bool use_even, bool flipped, bool use_clamp)
{

  t->mode = TFM_VERT_SLIDE;
  t->transform = applyVertSlide;
  t->handleEvent = handleEventVertSlide;
  t->tsnap.applySnap = vert_slide_snap_apply;
  t->tsnap.distance = transform_snap_distance_len_squared_fn;

  {
    VertSlideParams *slp = MEM_callocN(sizeof(*slp), __func__);
    slp->use_even = use_even;
    slp->flipped = flipped;
    slp->perc = 0.0f;

    if (!use_clamp) {
      t->flag |= T_ALT_TRANSFORM;
    }

    t->custom.mode.data = slp;
    t->custom.mode.use_free = true;
  }

  bool ok = false;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    VertSlideData *sld = createVertSlideVerts(t, tc);
    if (sld) {
      tc->custom.mode.data = sld;
      tc->custom.mode.free_cb = freeVertSlideVerts;
      trans_mesh_customdata_correction_init(t, tc);
      ok = true;
    }
  }

  if (ok == false) {
    t->state = TRANS_CANCEL;
    return;
  }

  calcVertSlideMouseActiveVert(t, t->mval);
  calcVertSlideMouseActiveEdges(t, t->mval);

  /* set custom point first if you want value to be initialized by init */
  calcVertSlideCustomPoints(t);
  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);

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

void initVertSlide(TransInfo *t)
{
  initVertSlide_ex(t, false, false, true);
}
/** \} */
