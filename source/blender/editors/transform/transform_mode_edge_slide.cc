/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_matrix.h"
#include "BLI_string.h"

#include "BKE_unit.hh"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "ED_screen.hh"
#include "ED_transform_snap_object_context.hh"

#include "WM_api.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"
#include "transform_snap.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Transform (Edge Slide)
 * \{ */

struct EdgeSlideData {
  Array<TransDataEdgeSlideVert> sv;

  int mval_start[2], mval_end[2];
  int curr_sv_index;

 private:
  float4x4 proj_mat;
  float2 win_half;

 public:
  void update_proj_mat(TransInfo *t, const TransDataContainer *tc)
  {
    ARegion *region = t->region;
    this->win_half = {region->winx / 2.0f, region->winy / 2.0f};

    if (t->spacetype == SPACE_VIEW3D) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      this->proj_mat = ED_view3d_ob_project_mat_get(rv3d, tc->obedit);

      for (int i = 0; i < 4; i++) {
        this->proj_mat[i][0] *= this->win_half[0];
        this->proj_mat[i][1] *= this->win_half[1];
      }
    }
    else {
      const View2D *v2d = static_cast<View2D *>(t->view);
      UI_view2d_view_to_region_m4(v2d, this->proj_mat.ptr());
      this->proj_mat.location()[0] -= this->win_half[0];
      this->proj_mat.location()[1] -= this->win_half[1];
    }
  }

  void project(const TransDataEdgeSlideVert *svert, float2 &r_sco_a, float2 &r_sco_b) const
  {
    float3 iloc = svert->v_co_orig();
    r_sco_a = math::project_point(this->proj_mat, iloc + svert->dir_side[0]).xy() + this->win_half;
    r_sco_b = math::project_point(this->proj_mat, iloc + svert->dir_side[1]).xy() + this->win_half;
  }
};

struct EdgeSlideParams {
  float perc;

  /** When un-clamped - use this index: #TransDataEdgeSlideVert.dir_side. */
  int curr_side_unclamp;

  bool use_even;
  bool flipped;
};

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
  BLI_assert_msg(0, "Should never happen, at least one EdgeSlideData should be valid");
  return nullptr;
}

static EdgeSlideData *edgeSlideFirstGet(TransInfo *t)
{
  TransDataContainer *tc = edge_slide_container_first_ok(t);
  return static_cast<EdgeSlideData *>(tc->custom.mode.data);
}

static void calcEdgeSlideCustomPoints(TransInfo *t)
{
  EdgeSlideData *sld = edgeSlideFirstGet(t);

  setCustomPoints(t, &t->mouse, sld->mval_end, sld->mval_start);

  /* #setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points. */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

/* Interpolates along a line made up of 2 segments (used for edge slide). */
static void interp_line_v3_v3v3v3(
    float p[3], const float v1[3], const float v2[3], const float v3[3], float t)
{
  float t_mid, t_delta;

  /* Could be pre-calculated. */
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

static void edge_slide_data_init_mval(MouseInput *mi, EdgeSlideData *sld, float *mval_dir)
{
  /* Possible all of the edge loops are pointing directly at the view. */
  if (UNLIKELY(len_squared_v2(mval_dir) < 0.1f)) {
    mval_dir[0] = 0.0f;
    mval_dir[1] = 100.0f;
  }

  float mval_start[2], mval_end[2];

  /* Zero out Start. */
  zero_v2(mval_start);

  /* `mval_dir` holds a vector along edge loop. */
  copy_v2_v2(mval_end, mval_dir);
  mul_v2_fl(mval_end, 0.5f);

  sld->mval_start[0] = mi->imval[0] + mval_start[0];
  sld->mval_start[1] = mi->imval[1] + mval_start[1];

  sld->mval_end[0] = mi->imval[0] + mval_end[0];
  sld->mval_end[1] = mi->imval[1] + mval_end[1];
}

static bool is_vert_slide_visible(TransInfo *t,
                                  SnapObjectContext *sctx,
                                  TransDataEdgeSlideVert *sv,
                                  const float4 &plane_near)
{
  const float3 &v_co_orig = sv->v_co_orig();
  float3 points[3] = {
      v_co_orig,
      v_co_orig + sv->dir_side[0] * 0.9f,
      v_co_orig + sv->dir_side[1] * 0.9f,
  };

  float3 hit_loc;
  for (const float3 &p : points) {
    float3 view_vec;
    float lambda, ray_depth = FLT_MAX;

    transform_view_vector_calc(t, p, view_vec);

    if (dot_v3v3(view_vec, plane_near) > 0.0f) {
      /* Behind the view origin. */
      return false;
    }

    if (!isect_ray_plane_v3(p, view_vec, plane_near, &lambda, false)) {
      return false;
    }

    float3 view_orig = p + view_vec * lambda;

    SnapObjectParams snap_object_params{};
    snap_object_params.snap_target_select = t->tsnap.target_operation;
    snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
    snap_object_params.use_occlusion_test = false;
    snap_object_params.use_backface_culling = (t->tsnap.flag & SCE_SNAP_BACKFACE_CULLING) != 0;

    bool has_hit = ED_transform_snap_object_project_ray_ex(sctx,
                                                           t->depsgraph,
                                                           static_cast<const View3D *>(t->view),
                                                           &snap_object_params,
                                                           view_orig,
                                                           -view_vec,
                                                           &ray_depth,
                                                           hit_loc,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr);

    const bool is_occluded = has_hit && lambda > (ray_depth + 0.0001f);
    if (!is_occluded) {
      return true;
    }
  }
  return false;
}

/**
 * Calculate screen-space `mval_start` / `mval_end`, optionally slide direction.
 */
static void calcEdgeSlide_mval_range(TransInfo *t,
                                     EdgeSlideData *sld,
                                     const int loop_nr,
                                     const float2 &mval,
                                     const bool use_calc_direction)
{
  /* Use for visibility checks. */
  SnapObjectContext *snap_context = nullptr;
  bool use_occlude_geometry = false;
  float4 plane_near;
  if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = static_cast<View3D *>(t->area ? t->area->spacedata.first : nullptr);
    use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE &&
                            !XRAY_ENABLED(v3d));
    planes_from_projmat(t->persmat, nullptr, nullptr, nullptr, nullptr, plane_near, nullptr);
    snap_context = ED_transform_snap_object_context_create(t->scene, 0);
  }

  /* Find mouse vectors, the global one, and one per loop in case we have
   * multiple loops selected, in case they are oriented different. */
  float2 mval_dir = float2(0);
  float dist_best_sq = FLT_MAX;

  /* Only for use_calc_direction. */
  float2 *loop_dir = nullptr;
  float *loop_maxdist = nullptr;

  if (use_calc_direction) {
    loop_dir = static_cast<float2 *>(MEM_callocN(sizeof(float2) * loop_nr, "sv loop_dir"));
    loop_maxdist = static_cast<float *>(MEM_mallocN(sizeof(float) * loop_nr, "sv loop_maxdist"));
    copy_vn_fl(loop_maxdist, loop_nr, FLT_MAX);
  }

  for (int i : sld->sv.index_range()) {
    TransDataEdgeSlideVert *sv = &sld->sv[i];
    bool is_visible = !use_occlude_geometry ||
                      is_vert_slide_visible(t, snap_context, sv, plane_near);

    /* This test is only relevant if object is not wire-drawn! See #32068. */
    if (!is_visible && !use_calc_direction) {
      continue;
    }

    /* Search cross edges for visible edge to the mouse cursor,
     * then use the shared vertex to calculate screen vector. */
    /* Screen-space coords. */
    float2 sco_a, sco_b;
    sld->project(sv, sco_a, sco_b);

    /* Global direction. */
    float dist_sq = dist_squared_to_line_segment_v2(mval, sco_b, sco_a);
    if (is_visible) {
      if (dist_sq < dist_best_sq && (len_squared_v2v2(sco_b, sco_a) > 0.1f)) {
        dist_best_sq = dist_sq;
        mval_dir = sco_b - sco_a;
        sld->curr_sv_index = i;
      }
    }

    if (use_calc_direction) {
      /* Per loop direction. */
      int l_nr = sv->loop_nr;
      if (dist_sq < loop_maxdist[l_nr]) {
        loop_maxdist[l_nr] = dist_sq;
        loop_dir[l_nr] = sco_b - sco_a;
      }
    }
  }

  if (use_calc_direction) {
    for (TransDataEdgeSlideVert &sv : sld->sv) {
      /* Switch a/b if loop direction is different from global direction. */
      int l_nr = sv.loop_nr;
      if (math::dot(loop_dir[l_nr], mval_dir) < 0.0f) {
        swap_v3_v3(sv.dir_side[0], sv.dir_side[1]);
      }
    }

    MEM_freeN(loop_dir);
    MEM_freeN(loop_maxdist);
  }

  edge_slide_data_init_mval(&t->mouse, sld, mval_dir);

  if (snap_context) {
    ED_transform_snap_object_context_destroy(snap_context);
  }
}

static EdgeSlideData *createEdgeSlideVerts(TransInfo *t,
                                           TransDataContainer *tc,
                                           const bool use_double_side)
{
  int group_len;
  EdgeSlideData *sld = MEM_new<EdgeSlideData>("sld");
  sld->sv = transform_mesh_edge_slide_data_create(tc, &group_len);

  if (sld->sv.is_empty()) {
    MEM_delete(sld);
    return nullptr;
  }

  if (!use_double_side) {
    /* Single Side Case.
     * Used by #MESH_OT_offset_edge_loops_slide.
     * It only slides to the side with the longest length. */
    struct TMP {
      float2 accum;
      int count;
    } zero{};

    Array<TMP> array_len(group_len, zero);
    for (TransDataEdgeSlideVert &sv : sld->sv) {
      array_len[sv.loop_nr].accum += float2(math::length(sv.dir_side[0]),
                                            math::length(sv.dir_side[1]));
      array_len[sv.loop_nr].count++;
    }

    for (TMP &accum : array_len) {
      accum.accum /= accum.count;
    }

    for (TransDataEdgeSlideVert &sv : sld->sv) {
      if (array_len[sv.loop_nr].accum[1] > array_len[sv.loop_nr].accum[0]) {
        sv.dir_side[0] = sv.dir_side[1];
      }
      sv.dir_side[1] = float3(0);
      sv.edge_len = math::length(sv.dir_side[0]);
    }
  }

  sld->curr_sv_index = 0;
  sld->update_proj_mat(t, tc);

  calcEdgeSlide_mval_range(t, sld, group_len, t->mval, use_double_side);

  return sld;
}

static void freeEdgeSlideVerts(TransInfo * /*t*/,
                               TransDataContainer * /*tc*/,
                               TransCustomData *custom_data)
{
  EdgeSlideData *sld = static_cast<EdgeSlideData *>(custom_data->data);

  if (sld == nullptr) {
    return;
  }

  MEM_delete(sld);

  custom_data->data = nullptr;
}

static eRedrawFlag handleEventEdgeSlide(TransInfo *t, const wmEvent *event)
{
  EdgeSlideParams *slp = static_cast<EdgeSlideParams *>(t->custom.mode.data);

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
        /* Use like a modifier key. */
        if (event->val == KM_PRESS) {
          t->flag ^= T_ALT_TRANSFORM;
          calcEdgeSlideCustomPoints(t);
          return TREDRAW_HARD;
        }
        break;
      case MOUSEMOVE:
        calcEdgeSlideCustomPoints(t);
        break;
      default:
        break;
    }
  }
  return TREDRAW_NOTHING;
}

static void drawEdgeSlide(TransInfo *t)
{
  EdgeSlideData *sld = edgeSlideFirstGet(t);
  if (sld == nullptr) {
    return;
  }

  const EdgeSlideParams *slp = static_cast<const EdgeSlideParams *>(t->custom.mode.data);
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

  const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;

  GPU_depth_test(GPU_DEPTH_NONE);

  GPU_blend(GPU_BLEND_ALPHA);

  if (t->spacetype == SPACE_VIEW3D) {
    GPU_matrix_push();
    GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->object_to_world().ptr());
  }

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
  const float3 &curr_sv_co_orig = curr_sv->v_co_orig();

  if (slp->use_even == true) {
    /* Even mode. */
    float co_a[3], co_b[3], co_mark[3];
    const float fac = (slp->perc + 1.0f) / 2.0f;
    const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
    const float guide_size = ctrl_size - 0.5f;
    const int alpha_shade = -30;

    add_v3_v3v3(co_a, curr_sv_co_orig, curr_sv->dir_side[0]);
    add_v3_v3v3(co_b, curr_sv_co_orig, curr_sv->dir_side[1]);

    GPU_line_width(line_size);
    immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
    immBeginAtMost(GPU_PRIM_LINES, 4);
    if (!math::is_zero(curr_sv->dir_side[0])) {
      immVertex3fv(pos, co_a);
      immVertex3fv(pos, curr_sv_co_orig);
    }
    if (!math::is_zero(curr_sv->dir_side[1])) {
      immVertex3fv(pos, co_b);
      immVertex3fv(pos, curr_sv_co_orig);
    }
    immEnd();

    {
      float *co_test = nullptr;
      if (slp->flipped) {
        if (!math::is_zero(curr_sv->dir_side[1])) {
          co_test = co_b;
        }
      }
      else {
        if (!math::is_zero(curr_sv->dir_side[0])) {
          co_test = co_a;
        }
      }

      if (co_test != nullptr) {
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
    interp_line_v3_v3v3v3(co_mark, co_b, curr_sv_co_orig, co_a, fac);
    immVertex3fv(pos, co_mark);
    immEnd();
  }
  else if (is_clamp == false) {
    const int side_index = slp->curr_side_unclamp;
    const int alpha_shade = -160;

    GPU_line_width(line_size);
    immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
    immBegin(GPU_PRIM_LINES, sld->sv.size() * 2);

    /* TODO(@ideasman42): Loop over all verts. */
    for (TransDataEdgeSlideVert &sv : sld->sv) {
      float a[3], b[3];

      if (!is_zero_v3(sv.dir_side[side_index])) {
        copy_v3_v3(a, sv.dir_side[side_index]);
      }
      else {
        copy_v3_v3(a, sv.dir_side[!side_index]);
      }

      mul_v3_fl(a, 100.0f);
      negate_v3_v3(b, a);

      const float3 &sv_co_orig = sv.v_co_orig();
      add_v3_v3(a, sv_co_orig);
      add_v3_v3(b, sv_co_orig);

      immVertex3fv(pos, a);
      immVertex3fv(pos, b);
    }
    immEnd();
  }
  else {
    /* Common case. */
    const int alpha_shade = -160;

    float co_dir[3];
    add_v3_v3v3(co_dir, curr_sv_co_orig, curr_sv->dir_side[slp->curr_side_unclamp]);

    GPU_line_width(line_size);
    immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
    immBeginAtMost(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, curr_sv_co_orig);
    immVertex3fv(pos, co_dir);
    immEnd();
  }

  immUnbindProgram();

  if (t->spacetype == SPACE_VIEW3D) {
    GPU_matrix_pop();
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void edge_slide_snap_apply(TransInfo *t, float *value)
{
  TransDataContainer *tc = edge_slide_container_first_ok(t);
  EdgeSlideParams *slp = static_cast<EdgeSlideParams *>(t->custom.mode.data);
  EdgeSlideData *sld_active = static_cast<EdgeSlideData *>(tc->custom.mode.data);
  TransDataEdgeSlideVert *sv = &sld_active->sv[sld_active->curr_sv_index];
  float3 co_orig, co_dest[2], dvec, snap_point;
  co_orig = sv->v_co_orig();
  co_dest[0] = co_orig + sv->dir_side[0];
  co_dest[1] = co_orig + sv->dir_side[1];

  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, co_orig);
    mul_m4_v3(tc->mat, co_dest[0]);
    mul_m4_v3(tc->mat, co_dest[1]);
  }

  getSnapPoint(t, dvec);
  sub_v3_v3(dvec, t->tsnap.snap_source);
  add_v3_v3v3(snap_point, co_orig, dvec);

  float perc = *value;
  int side_index;
  float t_mid;
  if (slp->use_even == false) {
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
    if (is_clamp) {
      side_index = perc < 0.0f;
    }
    else {
      /* Use the side indicated in `EdgeSlideParams::curr_side_unclamp` as long as that side is not
       * zero length. */
      side_index = int(slp->curr_side_unclamp ==
                       !math::is_zero(sv->dir_side[slp->curr_side_unclamp]));
    }
  }
  else {
    /* Could be pre-calculated. */
    t_mid = line_point_factor_v3(
        blender::float3{0.0f, 0.0f, 0.0f}, sv->dir_side[0], sv->dir_side[1]);

    float t_snap = line_point_factor_v3(snap_point, co_dest[0], co_dest[1]);
    side_index = t_snap >= t_mid;
  }

  if (t->tsnap.target_type & (SCE_SNAP_TO_EDGE | SCE_SNAP_TO_FACE)) {
    float co_dir[3];
    sub_v3_v3v3(co_dir, co_dest[side_index], co_orig);
    normalize_v3(co_dir);
    if (t->tsnap.target_type & SCE_SNAP_TO_EDGE) {
      transform_constraint_snap_axis_to_edge(t, co_dir, dvec);
    }
    else {
      transform_constraint_snap_axis_to_face(t, co_dir, dvec);
    }
    add_v3_v3v3(snap_point, co_orig, dvec);
  }

  perc = line_point_factor_v3(snap_point, co_orig, co_dest[side_index]);
  if (slp->use_even == false) {
    if (side_index) {
      perc *= -1;
    }
  }
  else {
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

static void edge_slide_apply_elem(const TransDataEdgeSlideVert &sv,
                                  const float fac,
                                  const float curr_length_fac,
                                  const int curr_side_unclamp,
                                  const bool use_clamp,
                                  const bool use_even,
                                  const bool use_flip,
                                  float r_co[3])
{
  copy_v3_v3(r_co, sv.v_co_orig());

  if (use_even == false) {
    if (use_clamp) {
      const int side_index = (fac < 0.0f);
      const float fac_final = fabsf(fac);
      madd_v3_v3fl(r_co, sv.dir_side[side_index], fac_final);
    }
    else {
      int side_index = curr_side_unclamp;
      if (is_zero_v3(sv.dir_side[side_index])) {
        side_index = int(!side_index);
      }
      const float fac_final = (side_index == (fac < 0.0f) ? fabsf(fac) : -fabsf(fac));
      madd_v3_v3fl(r_co, sv.dir_side[side_index], fac_final);
    }
  }
  else {
    /**
     * NOTE(@ideasman42): Implementation note, even mode ignores the starting positions and uses
     * only the a/b verts, this could be changed/improved so the distance is
     * still met but the verts are moved along their original path (which may not be straight),
     * however how it works now is OK and matches 2.4x.
     *
     * \note `len_v3v3(curr_sv->dir_side[0], curr_sv->dir_side[1])`
     * is the same as the distance between the original vert locations,
     * same goes for the lines below.
     */
    if (sv.edge_len > FLT_EPSILON) {
      float co_a[3], co_b[3];
      const float fac_final = min_ff(sv.edge_len, curr_length_fac) / sv.edge_len;

      add_v3_v3v3(co_a, r_co, sv.dir_side[0]);
      add_v3_v3v3(co_b, r_co, sv.dir_side[1]);

      if (use_flip) {
        interp_line_v3_v3v3v3(r_co, co_b, r_co, co_a, fac_final);
      }
      else {
        interp_line_v3_v3v3v3(r_co, co_a, r_co, co_b, fac_final);
      }
    }
  }
}

static void doEdgeSlide(TransInfo *t, float perc)
{
  EdgeSlideParams *slp = static_cast<EdgeSlideParams *>(t->custom.mode.data);
  EdgeSlideData *sld_active = edgeSlideFirstGet(t);

  slp->perc = perc;

  const bool use_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool use_even = slp->use_even;
  const bool use_flip = slp->flipped;

  const int curr_side_unclamp = slp->curr_side_unclamp;
  float curr_length_fac = 0.0f;
  if (use_even) {
    TransDataEdgeSlideVert *sv_active = &sld_active->sv[sld_active->curr_sv_index];
    curr_length_fac = sv_active->edge_len * (((use_flip ? perc : -perc) + 1.0f) / 2.0f);
  }
  else if (use_clamp) {
    slp->curr_side_unclamp = (perc < 0.0f);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    EdgeSlideData *sld = static_cast<EdgeSlideData *>(tc->custom.mode.data);

    if (sld == nullptr) {
      continue;
    }

    for (TransDataEdgeSlideVert &sv : sld->sv) {
      edge_slide_apply_elem(
          sv, perc, curr_length_fac, curr_side_unclamp, use_clamp, use_even, use_flip, sv.td->loc);
    }
  }
}

static void applyEdgeSlide(TransInfo *t)
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  EdgeSlideParams *slp = static_cast<EdgeSlideParams *>(t->custom.mode.data);
  bool flipped = slp->flipped;
  bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

  final = t->values[0] + t->values_modal_offset[0];

  transform_snap_mixed_apply(t, &final);
  if (!validSnap(t)) {
    transform_snap_increment(t, &final);
  }

  /* Only do this so out of range values are not displayed. */
  if (is_constrained) {
    CLAMP(final, -1.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values_final[0] = final;

  /* Header string. */
  ofs += BLI_strncpy_rlen(str + ofs, RPT_("Edge Slide: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, &t->scene->unit);
    ofs += BLI_strncpy_rlen(str + ofs, &c[0], sizeof(str) - ofs);
  }
  else {
    ofs += BLI_snprintf_rlen(str + ofs, sizeof(str) - ofs, "%.4f ", final);
  }
  ofs += BLI_snprintf_rlen(
      str + ofs, sizeof(str) - ofs, RPT_("(E)ven: %s, "), WM_bool_as_string(use_even));
  if (use_even) {
    ofs += BLI_snprintf_rlen(
        str + ofs, sizeof(str) - ofs, RPT_("(F)lipped: %s, "), WM_bool_as_string(flipped));
  }
  ofs += BLI_snprintf_rlen(
      str + ofs, sizeof(str) - ofs, RPT_("Alt or (C)lamp: %s"), WM_bool_as_string(is_clamp));
  /* Done with header string. */

  /* Do stuff here. */
  doEdgeSlide(t, final);

  recalc_data(t);

  ED_area_status_text(t->area, str);
}

static void edge_slide_transform_matrix_fn(TransInfo *t, float mat_xform[4][4])
{
  float delta[3], orig_co[3], final_co[3];

  EdgeSlideParams *slp = static_cast<EdgeSlideParams *>(t->custom.mode.data);
  TransDataContainer *tc = edge_slide_container_first_ok(t);
  EdgeSlideData *sld_active = static_cast<EdgeSlideData *>(tc->custom.mode.data);
  TransDataEdgeSlideVert &sv_active = sld_active->sv[sld_active->curr_sv_index];

  copy_v3_v3(orig_co, sv_active.v_co_orig());

  const float fac = t->values_final[0];
  float curr_length_fac = 0.0f;
  if (slp->use_even) {
    curr_length_fac = sv_active.edge_len * (((slp->flipped ? fac : -fac) + 1.0f) / 2.0f);
  }

  edge_slide_apply_elem(sv_active,
                        fac,
                        curr_length_fac,
                        slp->curr_side_unclamp,
                        !(t->flag & T_ALT_TRANSFORM),
                        slp->use_even,
                        slp->flipped,
                        final_co);

  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, orig_co);
    mul_m4_v3(tc->mat, final_co);
  }

  sub_v3_v3v3(delta, final_co, orig_co);
  add_v3_v3(mat_xform[3], delta);
}

static void initEdgeSlide_ex(
    TransInfo *t, bool use_double_side, bool use_even, bool flipped, bool use_clamp)
{
  EdgeSlideData *sld;
  bool ok = false;

  t->mode = TFM_EDGE_SLIDE;

  {
    EdgeSlideParams *slp = static_cast<EdgeSlideParams *>(MEM_callocN(sizeof(*slp), __func__));
    slp->use_even = use_even;
    slp->flipped = flipped;
    /* Happens to be best for single-sided. */
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
    sld = createEdgeSlideVerts(t, tc, use_double_side);
    if (sld) {
      tc->custom.mode.data = sld;
      tc->custom.mode.free_cb = freeEdgeSlideVerts;
      ok = true;
    }
  }

  if (!ok) {
    t->state = TRANS_CANCEL;
    return;
  }

  /* Set custom point first if you want value to be initialized by init. */
  calcEdgeSlideCustomPoints(t);
  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO_FLIP);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.1f;
  t->snap[1] = t->snap[0] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
}

static void initEdgeSlide(TransInfo *t, wmOperator *op)
{
  bool use_double_side = true;
  bool use_even = false;
  bool flipped = false;
  bool use_clamp = true;
  if (op) {
    use_double_side = !RNA_boolean_get(op->ptr, "single_side");
    use_even = RNA_boolean_get(op->ptr, "use_even");
    flipped = RNA_boolean_get(op->ptr, "flipped");
    use_clamp = RNA_boolean_get(op->ptr, "use_clamp");
  }
  initEdgeSlide_ex(t, use_double_side, use_even, flipped, use_clamp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Input Utilities
 * \{ */

void transform_mode_edge_slide_reproject_input(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    EdgeSlideData *sld = static_cast<EdgeSlideData *>(tc->custom.mode.data);
    if (sld) {
      sld->update_proj_mat(t, tc);
      TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];

      float2 sco_a, sco_b;
      sld->project(curr_sv, sco_a, sco_b);
      float2 mval_dir = sco_b - sco_a;
      edge_slide_data_init_mval(&t->mouse, sld, mval_dir);
    }
  }

  EdgeSlideData *sld = edgeSlideFirstGet(t);
  setCustomPoints(t, &t->mouse, sld->mval_end, sld->mval_start);
}

/** \} */

TransModeInfo TransMode_edgeslide = {
    /*flags*/ T_NO_CONSTRAINT,
    /*init_fn*/ initEdgeSlide,
    /*transform_fn*/ applyEdgeSlide,
    /*transform_matrix_fn*/ edge_slide_transform_matrix_fn,
    /*handle_event_fn*/ handleEventEdgeSlide,
    /*snap_distance_fn*/ transform_snap_distance_len_squared_fn,
    /*snap_apply_fn*/ edge_slide_snap_apply,
    /*draw_fn*/ drawEdgeSlide,
};
