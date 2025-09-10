/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_string_utf8.h"

#include "BKE_unit.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "ED_screen.hh"

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

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Transform (Vert Slide)
 * \{ */

struct VertSlideData {
  Array<TransDataVertSlideVert> sv;
  Vector<float3> targets_buffer;
  int curr_sv_index;

 private:
  float4x4 proj_mat;
  float2 win_half;

 public:
  void update_proj_mat(TransInfo *t, const TransDataContainer *tc)
  {
    ARegion *region = t->region;

    if (UNLIKELY(region == nullptr)) {
      this->win_half = {1.0f, 1.0f};
      this->proj_mat = float4x4::identity();
      return;
    }

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

  float2 project(const float3 &co)
  {
    return math::project_point(this->proj_mat, co).xy() + this->win_half;
  }

  /**
   * Run while moving the mouse to slide along the edge matching the mouse direction.
   */
  void update_active_edges(TransInfo *t, const float2 &mval_fl)
  {
    /* First get the direction of the original mouse position. */
    float2 dir = math::normalize(mval_fl - t->mouse.imval);

    for (TransDataVertSlideVert &sv : this->sv) {
      if (sv.co_link_orig_3d.size() <= 1) {
        continue;
      }

      const float3 v_co_orig = sv.co_orig_3d();
      float2 loc_src_2d = math::project_point(this->proj_mat, v_co_orig).xy();

      float dir_dot_best = -FLT_MAX;
      int co_link_curr_best = -1;

      for (int j : sv.co_link_orig_3d.index_range()) {
        const float3 &loc_dst = sv.co_link_orig_3d[j];
        float2 loc_dst_2d = math::project_point(this->proj_mat, loc_dst).xy();
        float2 tdir = math::normalize(loc_dst_2d - loc_src_2d);

        float dir_dot = math::dot(dir, tdir);
        if (dir_dot > dir_dot_best) {
          dir_dot_best = dir_dot;
          co_link_curr_best = j;
        }
      }

      if (co_link_curr_best != -1) {
        sv.co_link_curr = co_link_curr_best;
      }
    }
  }

  /**
   * Run once when initializing vert slide to find the reference edge.
   */
  void update_active_vert(TransInfo * /*t*/, const float2 &mval_fl)
  {
    /* Set the vertex to use as a reference for the mouse direction `curr_sv_index`. */
    float dist_min_sq = FLT_MAX;

    for (int i : this->sv.index_range()) {
      TransDataVertSlideVert &sv = this->sv[i];
      const float2 co_2d = this->project(sv.co_orig_3d());
      const float dist_sq = len_squared_v2v2(mval_fl, co_2d);
      if (dist_sq < dist_min_sq) {
        dist_min_sq = dist_sq;
        this->curr_sv_index = i;
      }
    }
  }
};

struct VertSlideParams {
  float perc;
  wmOperator *op;
  bool use_even;
  bool flipped;
};

static void vert_slide_update_input(TransInfo *t)
{
  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);
  VertSlideData *sld = static_cast<VertSlideData *>(
      TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data);
  TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];

  const float3 co_orig_3d = sv->co_orig_3d();
  const float3 co_dest_3d = sv->co_dest_3d();

  int mval_ofs[2], mval_start[2], mval_end[2];

  const float2 co_orig_2d = sld->project(co_orig_3d);
  const float2 co_curr_2d = sld->project(co_dest_3d);

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

  /* #setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points. */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

static VertSlideData *createVertSlideVerts(TransInfo *t, TransDataContainer *tc)
{
  VertSlideData *sld = MEM_new<VertSlideData>(__func__);
  if (t->data_type == &TransConvertType_MeshUV) {
    sld->sv = transform_mesh_uv_vert_slide_data_create(t, tc, sld->targets_buffer);
  }
  else {
    sld->sv = transform_mesh_vert_slide_data_create(tc, sld->targets_buffer);
  }

  if (sld->sv.is_empty()) {
    MEM_delete(sld);
    return nullptr;
  }

  sld->curr_sv_index = 0;
  sld->update_proj_mat(t, tc);
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

  MEM_delete(sld);
  custom_data->data = nullptr;
}

static eRedrawFlag handleEventVertSlide(TransInfo *t, const wmEvent *event)
{
  if (t->redraw && event->type != MOUSEMOVE) {
    /* Event already handled. */
    return TREDRAW_NOTHING;
  }

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
        /* Use like a modifier key. */
        if (event->val == KM_PRESS) {
          t->flag ^= T_ALT_TRANSFORM;
          calcVertSlideCustomPoints(t);
          return TREDRAW_HARD;
        }
        break;
      case MOUSEMOVE: {
        /* Don't recalculate the best edge. */
        const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
        if (is_clamp) {
          const TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
          VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
          sld->update_active_edges(t, float2(event->mval));
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
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
  VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
  if (sld) {
    const VertSlideParams *slp = static_cast<const VertSlideParams *>(t->custom.mode.data);
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

    /* Non-Prop mode. */
    {
      TransDataVertSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];

      const float3 co_orig_3d_act = curr_sv->co_orig_3d();
      const float3 co_dest_3d_act = curr_sv->co_dest_3d();

      const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
      const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;
      const int alpha_shade = -160;

      GPU_depth_test(GPU_DEPTH_NONE);

      GPU_blend(GPU_BLEND_ALPHA);

      GPU_matrix_push();
      if (t->spacetype == SPACE_VIEW3D) {
        GPU_matrix_mul(tc->obedit->object_to_world().ptr());
      }
      else {
        GPU_matrix_scale_2f(1 / t->aspect[0], 1 / t->aspect[1]);
      }

      GPU_line_width(line_size);

      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);

      immBegin(GPU_PRIM_LINES, sld->sv.size() * 2);
      if (is_clamp) {
        for (TransDataVertSlideVert &sv : sld->sv) {
          immVertex3fv(shdr_pos, sv.co_orig_3d());
          immVertex3fv(shdr_pos, sv.co_dest_3d());
        }
      }
      else {
        for (TransDataVertSlideVert &sv : sld->sv) {
          const float3 co_orig_3d = sv.co_orig_3d();
          const float3 co_dest_3d = sv.co_dest_3d();
          float a[3], b[3];
          sub_v3_v3v3(a, co_dest_3d, co_orig_3d);
          mul_v3_fl(a, 100.0f);
          negate_v3_v3(b, a);
          add_v3_v3(a, co_orig_3d);
          add_v3_v3(b, co_orig_3d);

          immVertex3fv(shdr_pos, a);
          immVertex3fv(shdr_pos, b);
        }
      }
      immEnd();

      immUnbindProgram();

      immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);

      GPU_point_size(ctrl_size);
      immUniformThemeColorShadeAlpha(TH_VERTEX_ACTIVE, 80, alpha_shade);

      immBegin(GPU_PRIM_POINTS, 1);
      immVertex3fv(shdr_pos, (slp->flipped && slp->use_even) ? co_dest_3d_act : co_orig_3d_act);
      immEnd();

      immUnbindProgram();

      GPU_matrix_pop();

      /* Direction from active vertex! */
      if (!compare_v2v2(t->mval, t->mouse.imval, FLT_EPSILON)) {
        /* 2D Pixel Space. */
        GPU_matrix_push_projection();
        GPU_matrix_push();
        GPU_matrix_identity_set();
        wmOrtho2_region_pixelspace(t->region);

        float3 co_orig_3d_cpy = co_orig_3d_act;
        if (t->spacetype != SPACE_VIEW3D) {
          co_orig_3d_cpy[0] /= t->aspect[0];
          co_orig_3d_cpy[1] /= t->aspect[1];
        }

        float2 loc_src_act_2d = sld->project(co_orig_3d_cpy);
        float2 loc_mval_dir = loc_src_act_2d + (t->mval - t->mouse.imval);

        GPU_line_width(1.0f);

        const uint shdr_pos_2d = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

        immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

        float viewport_size[4];
        GPU_viewport_size_get_f(viewport_size);
        immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

        immUniform1i("colors_len", 0); /* "simple" mode. */
        immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        immUniform1f("dash_width", 6.0f);
        immUniform1f("udash_factor", 0.5f);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex2fv(shdr_pos_2d, loc_src_act_2d);
        immVertex2fv(shdr_pos_2d, loc_mval_dir);
        immEnd();

        immUnbindProgram();

        GPU_matrix_pop();
        GPU_matrix_pop_projection();
      }
    }
  }
}

static void vert_slide_apply_elem(const TransDataVertSlideVert &sv,
                                  const float perc,
                                  const bool use_even,
                                  const bool use_flip,
                                  float r_co[3])
{
  const float3 co_orig_3d = sv.co_orig_3d();
  const float3 co_dest_3d = sv.co_dest_3d();
  if (use_even == false) {
    interp_v3_v3v3(r_co, co_orig_3d, co_dest_3d, perc);
  }
  else {
    float dir[3];
    sub_v3_v3v3(dir, co_dest_3d, co_orig_3d);
    float edge_len = normalize_v3(dir);
    if (edge_len > FLT_EPSILON) {
      if (use_flip) {
        madd_v3_v3v3fl(r_co, co_dest_3d, dir, -perc);
      }
      else {
        madd_v3_v3v3fl(r_co, co_orig_3d, dir, perc);
      }
    }
    else {
      copy_v3_v3(r_co, co_orig_3d);
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
      const float edge_len_curr = len_v3v3(sv_curr->co_orig_3d(), sv_curr->co_dest_3d());
      tperc *= edge_len_curr;
    }

    for (TransDataVertSlideVert &sv : sld->sv) {
      vert_slide_apply_elem(sv, tperc, use_even, slp->flipped, sv.td->loc);
    }
  }
}

static void vert_slide_snap_apply(TransInfo *t, float *value)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
  VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
  TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];
  float3 co_orig_3d = sv->co_orig_3d();
  float3 co_curr_3d = sv->co_dest_3d();

  float snap_point[3], dvec[3];
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

static void applyVertSlide(TransInfo *t)
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);
  const bool flipped = slp->flipped;
  const bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));
  const bool is_precision = t->modifiers & MOD_PRECISION;
  const bool is_snap = t->modifiers & MOD_SNAP;
  const bool is_snap_invert = t->modifiers & MOD_SNAP_INVERT;

  final = t->values[0] + t->values_modal_offset[0];

  transform_snap_mixed_apply(t, &final);
  if (!validSnap(t)) {
    transform_snap_increment(t, &final);
  }

  /* Only do this so out of range values are not displayed. */
  if (is_constrained) {
    CLAMP(final, 0.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values_final[0] = final;

  /* Header string. */
  ofs += BLI_strncpy_utf8_rlen(str + ofs, IFACE_("Vertex Slide: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, t->scene->unit);
    ofs += BLI_strncpy_utf8_rlen(str + ofs, &c[0], sizeof(str) - ofs);
  }
  else {
    ofs += BLI_snprintf_utf8_rlen(str + ofs, sizeof(str) - ofs, "%.4f ", final);
  }
  /* Done with header string. */

  /* Do stuff here. */
  doVertSlide(t, final);

  recalc_data(t);

  ED_area_status_text(t->area, str);

  wmOperator *op = slp->op;
  if (!op) {
    return;
  }

  WorkspaceStatus status(t->context);
  status.opmodal(IFACE_("Confirm"), op->type, TFM_MODAL_CONFIRM);
  status.opmodal(IFACE_("Cancel"), op->type, TFM_MODAL_CONFIRM);
  status.opmodal(IFACE_("Snap"), op->type, TFM_MODAL_SNAP_TOGGLE, is_snap);
  status.opmodal(IFACE_("Snap Invert"), op->type, TFM_MODAL_SNAP_INV_ON, is_snap_invert);
  status.opmodal(IFACE_("Set Snap Base"), op->type, TFM_MODAL_EDIT_SNAP_SOURCE_ON);
  status.opmodal(IFACE_("Move"), op->type, TFM_MODAL_TRANSLATE);
  status.opmodal(IFACE_("Rotate"), op->type, TFM_MODAL_ROTATE);
  status.opmodal(IFACE_("Resize"), op->type, TFM_MODAL_RESIZE);
  status.opmodal(IFACE_("Precision Mode"), op->type, TFM_MODAL_PRECISION, is_precision);
  status.item_bool(IFACE_("Clamp"), is_clamp, ICON_EVENT_C, ICON_EVENT_ALT);
  status.item_bool(IFACE_("Even"), use_even, ICON_EVENT_E);
  if (use_even) {
    status.item_bool(IFACE_("Flipped"), flipped, ICON_EVENT_F);
  }
}

static void vert_slide_transform_matrix_fn(TransInfo *t, float mat_xform[4][4])
{
  float delta[3];

  VertSlideParams *slp = static_cast<VertSlideParams *>(t->custom.mode.data);
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
  VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
  TransDataVertSlideVert &sv_active = sld->sv[sld->curr_sv_index];
  float3 orig_co = sv_active.co_orig_3d();
  const float3 &loc_dst_act = sv_active.co_dest_3d();

  float tperc = t->values_final[0];
  if (slp->use_even) {
    const float edge_len_curr = len_v3v3(orig_co, loc_dst_act);
    tperc *= edge_len_curr;
  }

  float3 final_co;
  vert_slide_apply_elem(sv_active, tperc, slp->use_even, slp->flipped, final_co);

  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, orig_co);
    mul_m4_v3(tc->mat, final_co);
  }

  sub_v3_v3v3(delta, final_co, orig_co);
  add_v3_v3(mat_xform[3], delta);
}

static void initVertSlide_ex(
    TransInfo *t, wmOperator *op, bool use_even, bool flipped, bool use_clamp)
{

  t->mode = TFM_VERT_SLIDE;

  {
    VertSlideParams *slp = MEM_callocN<VertSlideParams>(__func__);
    slp->use_even = use_even;
    slp->flipped = flipped;
    slp->perc = 0.0f;
    slp->op = op;

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
      sld->update_active_vert(t, t->mval);
      sld->update_active_edges(t, t->mval);

      tc->custom.mode.data = sld;
      tc->custom.mode.free_cb = freeVertSlideVerts;
      ok = true;
    }
  }

  if (ok == false) {
    t->state = TRANS_CANCEL;
    return;
  }

  /* Set custom point first if you want value to be initialized by init. */
  calcVertSlideCustomPoints(t);
  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->increment[0] = 0.1f;
  t->increment_precision = 0.1f;

  copy_v3_fl(t->num.val_inc, t->increment[0]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
}

static void initVertSlide(TransInfo *t, wmOperator *op)
{
  bool use_even = false;
  bool flipped = false;
  bool use_clamp = true;
  if (op) {
    PropertyRNA *prop;
    prop = RNA_struct_find_property(op->ptr, "use_even");
    use_even = (prop) ? RNA_property_boolean_get(op->ptr, prop) : false;
    prop = RNA_struct_find_property(op->ptr, "flipped");
    flipped = (prop) ? RNA_property_boolean_get(op->ptr, prop) : false;
    prop = RNA_struct_find_property(op->ptr, "use_clamp");
    use_clamp = (prop) ? RNA_property_boolean_get(op->ptr, prop) : true;
  }
  initVertSlide_ex(t, op, use_even, flipped, use_clamp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Input Utilities
 * \{ */

void transform_mode_vert_slide_reproject_input(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    VertSlideData *sld = static_cast<VertSlideData *>(tc->custom.mode.data);
    if (sld) {
      sld->update_proj_mat(t, tc);
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

}  // namespace blender::ed::transform
