/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

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

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"
#include "transform_snap.hh"

/* -------------------------------------------------------------------- */
/** \name Transform (Vert Slide)
 * \{ */

struct TransDataVertSlideVert {
  /** #TransDataGenericSlideVert (header) */
  BMVert *v;
  LinkNode **cd_loop_groups;
  float co_orig_3d[3];
  /* end generic */

  float (*co_link_orig_3d)[3];
  int co_link_tot;
  int co_link_curr;
};

struct VertSlideData {
  TransDataVertSlideVert *sv;
  int totsv;
  int curr_sv_index;

  /* result of ED_view3d_ob_project_mat_get */
  float proj_mat[4][4];
};

struct VertSlideParams {
  float perc;

  bool use_even;
  bool flipped;
};

static void vert_slide_update_input(TransInfo *t)
{
  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);
  VertSlideData *sld = static_cast<VertSlideData *>(
      TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data);
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
}

static void calcVertSlideCustomPoints(TransInfo *t)
{
  vert_slide_update_input(t);

  /* setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

/**
 * Run once when initializing vert slide to find the reference edge
 */
static void calcVertSlideMouseActiveVert(TransInfo *t, const int mval[2])
{
  /* Active object may have no selected vertices. */
  VertSlideData *sld = static_cast<VertSlideData *>(
      TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data);
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
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
static void calcVertSlideMouseActiveEdges(TransInfo *t, const int mval[2])
{
  VertSlideData *sld = static_cast<VertSlideData *>(
      TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data);
  const float imval_fl[2] = {float(t->mouse.imval[0]), float(t->mouse.imval[1])};
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};

  float dir[3];
  TransDataVertSlideVert *sv;
  int i;

  /* NOTE: we could save a matrix-multiply for each vertex
   * by finding the closest edge in local-space.
   * However this skews the outcome with non-uniform-scale. */

  /* First get the direction of the original mouse position. */
  sub_v2_v2v2(dir, imval_fl, mval_fl);
  ED_view3d_win_to_delta(t->region, dir, t->zfac, dir);
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
        mul_mat3_m4_v3(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->object_to_world, tdir);
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
  VertSlideData *sld = static_cast<VertSlideData *>(MEM_callocN(sizeof(*sld), "sld"));
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
    return nullptr;
  }

  sv_array = static_cast<TransDataVertSlideVert *>(
      MEM_callocN(sizeof(TransDataVertSlideVert) * j, "sv_array"));

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

      sv_array[j].co_link_orig_3d = static_cast<float(*)[3]>(
          MEM_mallocN(sizeof(*sv_array[j].co_link_orig_3d) * k, __func__));
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
    RegionView3D *rv3d = nullptr;
    ARegion *region = t->region;

    rv3d = static_cast<RegionView3D *>(region ? region->regiondata : nullptr);
    if (rv3d) {
      ED_view3d_ob_project_mat_get(rv3d, tc->obedit, sld->proj_mat);
    }
  }

  return sld;
}

static void freeVertSlideVerts(TransInfo * /*t*/,
                               TransDataContainer * /*tc*/,
                               TransCustomData *custom_data)
{
  VertSlideData *sld = static_cast<VertSlideData *>(custom_data->data);

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

  custom_data->data = nullptr;
}

static eRedrawFlag handleEventVertSlide(TransInfo *t, const wmEvent *event)
{
  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);

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
  return TREDRAW_NOTHING;
}

static void drawVertSlide(TransInfo *t)
{
  if (TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data) {
    const VertSlideParams *slp = static_cast<const VertSlideParams *>(t->custom.mode.data);
    VertSlideData *sld = static_cast<VertSlideData *>(
        TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data);
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

    /* Non-Prop mode */
    {
      TransDataVertSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
      TransDataVertSlideVert *sv;
      const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
      const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;
      const int alpha_shade = -160;
      int i;

      GPU_depth_test(GPU_DEPTH_NONE);

      GPU_blend(GPU_BLEND_ALPHA);

      GPU_matrix_push();
      GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->object_to_world);

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
        float xy_delta[2];
        float co_orig_3d[3];
        float co_dest_3d[3];

        xy_delta[0] = t->mval[0] - t->mouse.imval[0];
        xy_delta[1] = t->mval[1] - t->mouse.imval[1];

        mul_v3_m4v3(co_orig_3d,
                    TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->object_to_world,
                    curr_sv->co_orig_3d);
        zfac = ED_view3d_calc_zfac(static_cast<const RegionView3D *>(t->region->regiondata),
                                   co_orig_3d);

        ED_view3d_win_to_delta(t->region, xy_delta, zfac, co_dest_3d);

        invert_m4_m4(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->world_to_object,
                     TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->object_to_world);
        mul_mat3_m4_v3(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->world_to_object, co_dest_3d);

        add_v3_v3(co_dest_3d, curr_sv->co_orig_3d);

        GPU_line_width(1.0f);

        immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

        float viewport_size[4];
        GPU_viewport_size_get_f(viewport_size);
        immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

        immUniform1i("colors_len", 0); /* "simple" mode */
        immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        immUniform1f("dash_width", 6.0f);
        immUniform1f("udash_factor", 0.5f);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex3fv(shdr_pos, curr_sv->co_orig_3d);
        immVertex3fv(shdr_pos, co_dest_3d);
        immEnd();

        immUnbindProgram();
      }

      GPU_matrix_pop();

      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }
  }
}

static void vert_slide_apply_elem(const TransDataVertSlideVert *sv,
                                  const float perc,
                                  const bool use_even,
                                  const bool use_flip,
                                  float r_co[3])
{
  if (use_even == false) {
    interp_v3_v3v3(r_co, sv->co_orig_3d, sv->co_link_orig_3d[sv->co_link_curr], perc);
  }
  else {
    float dir[3];
    sub_v3_v3v3(dir, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
    float edge_len = normalize_v3(dir);
    if (edge_len > FLT_EPSILON) {
      if (use_flip) {
        madd_v3_v3v3fl(r_co, sv->co_link_orig_3d[sv->co_link_curr], dir, -perc);
      }
      else {
        madd_v3_v3v3fl(r_co, sv->co_orig_3d, dir, perc);
      }
    }
    else {
      copy_v3_v3(r_co, sv->co_orig_3d);
    }
  }
}

static void doVertSlide(TransInfo *t, float perc)
{
  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);

  slp->perc = perc;

  const bool use_even = slp->use_even;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
    if (sld == nullptr) {
      continue;
    }

    float tperc = perc;
    if (use_even) {
      TransDataVertSlideVert *sv_curr = &sld->sv[sld->curr_sv_index];
      const float edge_len_curr = len_v3v3(sv_curr->co_orig_3d,
                                           sv_curr->co_link_orig_3d[sv_curr->co_link_curr]);
      tperc *= edge_len_curr;
    }

    TransDataVertSlideVert *sv = sld->sv;
    for (int i = 0; i < sld->totsv; i++, sv++) {
      vert_slide_apply_elem(sv, tperc, use_even, slp->flipped, sv->v->co);
    }
  }
}

static void vert_slide_snap_apply(TransInfo *t, float *value)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
  VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
  TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];

  float snap_point[3], co_orig_3d[3], co_curr_3d[3], dvec[3];
  copy_v3_v3(co_orig_3d, sv->co_orig_3d);
  copy_v3_v3(co_curr_3d, sv->co_link_orig_3d[sv->co_link_curr]);
  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, co_orig_3d);
    mul_m4_v3(tc->mat, co_curr_3d);
  }

  getSnapPoint(t, dvec);
  sub_v3_v3(dvec, t->tsnap.snap_source);
  if (t->tsnap.target_type & (SCE_SNAP_TO_EDGE | SCE_SNAP_TO_FACE)) {
    float co_dir[3];
    sub_v3_v3v3(co_dir, co_curr_3d, co_orig_3d);
    normalize_v3(co_dir);
    if (t->tsnap.target_type & SCE_SNAP_TO_EDGE) {
      transform_constraint_snap_axis_to_edge(t, co_dir, dvec);
    }
    else {
      transform_constraint_snap_axis_to_face(t, co_dir, dvec);
    }
  }

  add_v3_v3v3(snap_point, co_orig_3d, dvec);
  *value = line_point_factor_v3(snap_point, co_orig_3d, co_curr_3d);
}

static void applyVertSlide(TransInfo *t, const int[2] /*mval*/)
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);
  const bool flipped = slp->flipped;
  const bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

  final = t->values[0] + t->values_modal_offset[0];

  transform_snap_mixed_apply(t, &final);
  if (!validSnap(t)) {
    transform_snap_increment(t, &final);
  }

  /* only do this so out of range values are not displayed */
  if (is_constrained) {
    CLAMP(final, 0.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values_final[0] = final;

  /* header string */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Vertex Slide: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, &t->scene->unit);
    ofs += BLI_strncpy_rlen(str + ofs, &c[0], sizeof(str) - ofs);
  }
  else {
    ofs += BLI_snprintf_rlen(str + ofs, sizeof(str) - ofs, "%.4f ", final);
  }
  ofs += BLI_snprintf_rlen(
      str + ofs, sizeof(str) - ofs, TIP_("(E)ven: %s, "), WM_bool_as_string(use_even));
  if (use_even) {
    ofs += BLI_snprintf_rlen(
        str + ofs, sizeof(str) - ofs, TIP_("(F)lipped: %s, "), WM_bool_as_string(flipped));
  }
  ofs += BLI_snprintf_rlen(
      str + ofs, sizeof(str) - ofs, TIP_("Alt or (C)lamp: %s"), WM_bool_as_string(is_clamp));
  /* done with header string */

  /* do stuff here */
  doVertSlide(t, final);

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void vert_slide_transform_matrix_fn(TransInfo *t, float mat_xform[4][4])
{
  float delta[3], orig_co[3], final_co[3];

  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
  VertSlideData *sld_active = static_cast<VertSlideData *>(tc->custom.mode.data);
  TransDataVertSlideVert *sv_active = &sld_active->sv[sld_active->curr_sv_index];

  copy_v3_v3(orig_co, sv_active->co_orig_3d);

  float tperc = t->values_final[0];
  if (slp->use_even) {
    const float edge_len_curr = len_v3v3(sv_active->co_orig_3d,
                                         sv_active->co_link_orig_3d[sv_active->co_link_curr]);
    tperc *= edge_len_curr;
  }

  vert_slide_apply_elem(sv_active, tperc, slp->use_even, slp->flipped, final_co);

  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, orig_co);
    mul_m4_v3(tc->mat, final_co);
  }

  sub_v3_v3v3(delta, final_co, orig_co);
  add_v3_v3(mat_xform[3], delta);
}

static void initVertSlide_ex(TransInfo *t, bool use_even, bool flipped, bool use_clamp)
{

  t->mode = TFM_VERT_SLIDE;

  {
    VertSlideParams *slp = static_cast<VertSlideParams *>(MEM_callocN(sizeof(*slp), __func__));
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
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
}

static void initVertSlide(TransInfo *t, wmOperator *op)
{
  bool use_even = false;
  bool flipped = false;
  bool use_clamp = true;
  if (op) {
    use_even = RNA_boolean_get(op->ptr, "use_even");
    flipped = RNA_boolean_get(op->ptr, "flipped");
    use_clamp = RNA_boolean_get(op->ptr, "use_clamp");
  }
  initVertSlide_ex(t, use_even, flipped, use_clamp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Input Utilities
 * \{ */

void transform_mode_vert_slide_reproject_input(TransInfo *t)
{
  if (t->spacetype == SPACE_VIEW3D) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
      ED_view3d_ob_project_mat_get(rv3d, tc->obedit, sld->proj_mat);
    }
  }

  vert_slide_update_input(t);
}

/** \} */

TransModeInfo TransMode_vertslide = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initVertSlide,
    /*transform_fn*/ applyVertSlide,
    /*transform_matrix_fn*/ vert_slide_transform_matrix_fn,
    /*handle_event_fn*/ handleEventVertSlide,
    /*snap_distance_fn*/ transform_snap_distance_len_squared_fn,
    /*snap_apply_fn*/ vert_slide_snap_apply,
    /*draw_fn*/ drawVertSlide,
};
