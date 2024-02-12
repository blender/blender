/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cfloat>

#include "DNA_windowmanager_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_node.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_uvedit.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "SEQ_sequencer.hh"

#include "MEM_guardedalloc.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"
#include "transform_snap.hh"

using namespace blender;

/* use half of flt-max so we can scale up without an exception */

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static void setSnappingCallback(TransInfo *t);

static void snap_target_view3d_fn(TransInfo *t, float *vec);
static void snap_target_uv_fn(TransInfo *t, float *vec);
static void snap_target_node_fn(TransInfo *t, float *vec);
static void snap_target_sequencer_fn(TransInfo *t, float *vec);
static void snap_target_nla_fn(TransInfo *t, float *vec);

static void snap_source_median_fn(TransInfo *t);
static void snap_source_center_fn(TransInfo *t);
static void snap_source_closest_fn(TransInfo *t);
static void snap_source_active_fn(TransInfo *t);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implementations
 * \{ */

static bool snapNodeTest(View2D *v2d, bNode *node, eSnapTargetOP snap_target_select);
static NodeBorder snapNodeBorder(eSnapMode snap_node_mode);

#if 0
int BIF_snappingSupported(Object *obedit)
{
  int status = 0;

  /* only support object mesh, armature, curves */
  if (obedit == nullptr ||
      ELEM(obedit->type, OB_MESH, OB_ARMATURE, OB_CURVES_LEGACY, OB_LATTICE, OB_MBALL))
  {
    status = 1;
  }

  return status;
}
#endif

static bool snap_use_backface_culling(const TransInfo *t)
{
  BLI_assert(t->spacetype == SPACE_VIEW3D);
  View3D *v3d = static_cast<View3D *>(t->view);
  if ((v3d->shading.type == OB_SOLID) && (v3d->shading.flag & V3D_SHADING_BACKFACE_CULLING)) {
    return true;
  }
  if (v3d->shading.type == OB_RENDER &&
      (t->scene->display.shading.flag & V3D_SHADING_BACKFACE_CULLING) &&
      BKE_scene_uses_blender_workbench(t->scene))
  {
    return true;
  }
  if (t->settings->snap_flag & SCE_SNAP_BACKFACE_CULLING) {
    return true;
  }
  return false;
}

bool validSnap(const TransInfo *t)
{
  return (t->tsnap.status & (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND)) ==
             (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND) ||
         (t->tsnap.status & (SNAP_MULTI_POINTS | SNAP_SOURCE_FOUND)) ==
             (SNAP_MULTI_POINTS | SNAP_SOURCE_FOUND);
}

void transform_snap_flag_from_modifiers_set(TransInfo *t)
{
  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA)) {
    /* Those space-types define their own invert behavior instead of toggling it on/off. */
    return;
  }
  if (t->spacetype == SPACE_GRAPH) {
    /* This is to stay consistent with the behavior from 3.6. */
    if (t->modifiers & MOD_SNAP_INVERT) {
      t->tsnap.mode |= SCE_SNAP_TO_INCREMENT;
    }
    else {
      t->tsnap.mode &= ~SCE_SNAP_TO_INCREMENT;
    }
    /* In 3.6 when snapping was disabled, pressing the invert button would turn on snapping.
     * But it wouldn't turn it off when it was enabled. */
    if ((t->modifiers & MOD_SNAP) || (t->modifiers & MOD_SNAP_INVERT)) {
      t->tsnap.flag |= SCE_SNAP;
    }
    else {
      t->tsnap.flag &= ~SCE_SNAP;
    }
    return;
  }
  SET_FLAG_FROM_TEST(t->tsnap.flag,
                     (((t->modifiers & (MOD_SNAP | MOD_SNAP_INVERT)) == MOD_SNAP) ||
                      ((t->modifiers & (MOD_SNAP | MOD_SNAP_INVERT)) == MOD_SNAP_INVERT)),
                     SCE_SNAP);
}

bool transform_snap_is_active(const TransInfo *t)
{
  return (t->tsnap.flag & SCE_SNAP) != 0;
}

bool transformModeUseSnap(const TransInfo *t)
{
  /* The animation editors should not depend on the snapping options of the 3D viewport. */
  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA)) {
    return true;
  }
  ToolSettings *ts = t->settings;
  if (t->mode == TFM_TRANSLATION) {
    return (ts->snap_transform_mode_flag & SCE_SNAP_TRANSFORM_MODE_TRANSLATE) != 0;
  }
  if (t->mode == TFM_ROTATION) {
    return (ts->snap_transform_mode_flag & SCE_SNAP_TRANSFORM_MODE_ROTATE) != 0;
  }
  if (t->mode == TFM_RESIZE) {
    return (ts->snap_transform_mode_flag & SCE_SNAP_TRANSFORM_MODE_SCALE) != 0;
  }
  if (ELEM(t->mode,
           TFM_VERT_SLIDE,
           TFM_EDGE_SLIDE,
           TFM_SEQ_SLIDE,
           TFM_TIME_TRANSLATE,
           TFM_TIME_EXTEND))
  {
    return true;
  }

  return false;
}

static bool doForceIncrementSnap(const TransInfo *t)
{
  if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_ACTION, SPACE_NLA)) {
    /* These spaces don't support increment snapping. */
    return false;
  }
  if (t->modifiers & MOD_SNAP_FORCED) {
    return false;
  }

  return !transformModeUseSnap(t);
}

void drawSnapping(TransInfo *t)
{
  uchar col[4], selectedCol[4], activeCol[4];
  if (!(transform_snap_is_active(t) || t->modifiers & MOD_EDIT_SNAP_SOURCE)) {
    return;
  }

  const bool draw_source = (t->flag & T_DRAW_SNAP_SOURCE) &&
                           (t->tsnap.status & (SNAP_SOURCE_FOUND | SNAP_MULTI_POINTS));
  const bool draw_target = (t->tsnap.status & (SNAP_TARGET_FOUND | SNAP_MULTI_POINTS));

  if (!(draw_source || draw_target)) {
    return;
  }

  if (t->spacetype == SPACE_SEQ) {
    UI_GetThemeColor3ubv(TH_SEQ_ACTIVE, col);
    col[3] = 128;
  }
  else if (t->spacetype != SPACE_IMAGE) {
    UI_GetThemeColor3ubv(TH_TRANSFORM, col);
    col[3] = 128;

    UI_GetThemeColor3ubv(TH_SELECT, selectedCol);
    selectedCol[3] = 128;

    UI_GetThemeColor3ubv(TH_ACTIVE, activeCol);
    activeCol[3] = 192;
  }

  if (t->spacetype == SPACE_VIEW3D) {
    const float *source_loc = nullptr;
    const float *target_loc = nullptr;

    GPU_depth_test(GPU_DEPTH_NONE);

    RegionView3D *rv3d = (RegionView3D *)t->region->regiondata;
    if (!BLI_listbase_is_empty(&t->tsnap.points)) {
      /* Draw snap points. */

      float size = 2.0f * UI_GetThemeValuef(TH_VERTEX_SIZE);
      float view_inv[4][4];
      copy_m4_m4(view_inv, rv3d->viewinv);

      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      if (!BLI_listbase_is_empty(&t->tsnap.points)) {
        LISTBASE_FOREACH (TransSnapPoint *, p, &t->tsnap.points) {
          if (p == t->tsnap.selectedPoint) {
            immUniformColor4ubv(selectedCol);
          }
          else {
            immUniformColor4ubv(col);
          }
          imm_drawcircball(p->co, ED_view3d_pixel_size(rv3d, p->co) * size, view_inv, pos);
        }
      }

      immUnbindProgram();
    }

    if (draw_source) {
      source_loc = t->tsnap.snap_source;
    }

    if (t->tsnap.status & SNAP_TARGET_FOUND) {
      target_loc = t->tsnap.snap_target;
    }

    ED_view3d_cursor_snap_draw_util(
        rv3d, source_loc, target_loc, t->tsnap.source_type, t->tsnap.target_type, col, activeCol);

    /* Draw normal if needed. */
    if (target_loc && usingSnappingNormal(t) && validSnappingNormal(t)) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor4ubv(activeCol);
      immBegin(GPU_PRIM_LINES, 2);
      immVertex3fv(pos, target_loc);
      immVertex3f(pos,
                  target_loc[0] + t->tsnap.snapNormal[0],
                  target_loc[1] + t->tsnap.snapNormal[1],
                  target_loc[2] + t->tsnap.snapNormal[2]);
      immEnd();
      immUnbindProgram();
    }

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    float x, y;
    const float snap_point[2] = {
        t->tsnap.snap_target[0] / t->aspect[0],
        t->tsnap.snap_target[1] / t->aspect[1],
    };
    UI_view2d_view_to_region_fl(&t->region->v2d, UNPACK2(snap_point), &x, &y);
    float radius = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE) * U.pixelsize;

    GPU_matrix_push_projection();
    wmOrtho2_region_pixelspace(t->region);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3ub(255, 255, 255);
    imm_draw_circle_wire_2d(pos, x, y, radius, 8);
    immUnbindProgram();

    GPU_matrix_pop_projection();
  }
  else if (t->spacetype == SPACE_NODE) {
    ARegion *region = t->region;
    float size;

    size = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);

    GPU_blend(GPU_BLEND_ALPHA);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    LISTBASE_FOREACH (TransSnapPoint *, p, &t->tsnap.points) {
      if (p == t->tsnap.selectedPoint) {
        immUniformColor4ubv(selectedCol);
      }
      else {
        immUniformColor4ubv(col);
      }

      ED_node_draw_snap(&region->v2d, p->co, size, NodeBorder(0), pos);
    }

    if (t->tsnap.status & SNAP_TARGET_FOUND) {
      immUniformColor4ubv(activeCol);

      ED_node_draw_snap(
          &region->v2d, t->tsnap.snap_target, size, NodeBorder(t->tsnap.snapNodeBorder), pos);
    }

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
  }
  else if (t->spacetype == SPACE_SEQ) {
    const ARegion *region = t->region;
    GPU_blend(GPU_BLEND_ALPHA);
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4ubv(col);
    float pixelx = BLI_rctf_size_x(&region->v2d.cur) / BLI_rcti_size_x(&region->v2d.mask);
    immRectf(pos,
             t->tsnap.snap_target[0] - pixelx,
             region->v2d.cur.ymax,
             t->tsnap.snap_target[0] + pixelx,
             region->v2d.cur.ymin);
    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
  }
}

eRedrawFlag handleSnapping(TransInfo *t, const wmEvent *event)
{
  eRedrawFlag status = TREDRAW_NOTHING;

#if 0 /* XXX need a proper selector for all snap mode */
  if (BIF_snappingSupported(t->obedit) && (event->type == EVT_TABKEY) &&
      (event->modifier & KM_SHIFT))
  {
    /* Toggle snap and reinitialize. */
    t->settings->snap_flag ^= SCE_SNAP;
    initSnapping(t, nullptr);
    status = TREDRAW_HARD;
  }
#endif
  if (event->type == MOUSEMOVE) {
    status |= updateSelectedSnapPoint(t);
  }

  return status;
}

static bool applyFaceProject(TransInfo *t, TransDataContainer *tc, TransData *td)
{
  float iloc[3], loc[3], no[3];
  float mval_fl[2];

  copy_v3_v3(iloc, td->loc);
  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, iloc);
  }
  else if (t->options & CTX_OBJECT) {
    BKE_object_eval_transform_all(t->depsgraph, t->scene, td->ob);
    copy_v3_v3(iloc, td->ob->object_to_world[3]);
  }

  if (ED_view3d_project_float_global(t->region, iloc, mval_fl, V3D_PROJ_TEST_NOP) !=
      V3D_PROJ_RET_OK)
  {
    return false;
  }

  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
  snap_object_params.use_occlusion_test = false;
  snap_object_params.use_backface_culling = (t->tsnap.flag & SCE_SNAP_BACKFACE_CULLING) != 0;

  eSnapMode hit = ED_transform_snap_object_project_view3d(t->tsnap.object_context,
                                                          t->depsgraph,
                                                          t->region,
                                                          static_cast<const View3D *>(t->view),
                                                          SCE_SNAP_TO_FACE,
                                                          &snap_object_params,
                                                          nullptr,
                                                          mval_fl,
                                                          nullptr,
                                                          nullptr,
                                                          loc,
                                                          no);
  if (hit != SCE_SNAP_TO_FACE) {
    return false;
  }

  float tvec[3];
  sub_v3_v3v3(tvec, loc, iloc);

  mul_m3_v3(td->smtx, tvec);

  add_v3_v3(td->loc, tvec);

  if ((t->tsnap.flag & SCE_SNAP_ROTATE) && (t->options & CTX_OBJECT)) {
    /* handle alignment as well */
    const float *original_normal;
    float mat[3][3];

    /* In pose mode, we want to align normals with Y axis of bones. */
    original_normal = td->axismtx[2];

    rotation_between_vecs_to_mat3(mat, original_normal, no);

    transform_data_ext_rotate(td, mat, true);

    /* TODO: support constraints for rotation too? see #ElementRotation. */
  }
  return true;
}

static void applyFaceNearest(TransInfo *t, TransDataContainer *tc, TransData *td)
{
  float init_loc[3];
  float prev_loc[3];
  float snap_loc[3], snap_no[3];

  copy_v3_v3(init_loc, td->iloc);
  copy_v3_v3(prev_loc, td->loc);
  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, init_loc);
    mul_m4_v3(tc->mat, prev_loc);
  }
  else if (t->options & CTX_OBJECT) {
    BKE_object_eval_transform_all(t->depsgraph, t->scene, td->ob);
    copy_v3_v3(init_loc, td->ob->object_to_world[3]);
  }

  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
  snap_object_params.use_occlusion_test = false;
  snap_object_params.use_backface_culling = false;
  snap_object_params.face_nearest_steps = t->tsnap.face_nearest_steps;
  snap_object_params.keep_on_same_target = t->tsnap.flag & SCE_SNAP_KEEP_ON_SAME_OBJECT;

  eSnapMode hit = ED_transform_snap_object_project_view3d(t->tsnap.object_context,
                                                          t->depsgraph,
                                                          t->region,
                                                          static_cast<const View3D *>(t->view),
                                                          SCE_SNAP_INDIVIDUAL_NEAREST,
                                                          &snap_object_params,
                                                          init_loc,
                                                          nullptr,
                                                          prev_loc,
                                                          nullptr,
                                                          snap_loc,
                                                          snap_no);

  if (hit != SCE_SNAP_INDIVIDUAL_NEAREST) {
    return;
  }

  float tvec[3];
  sub_v3_v3v3(tvec, snap_loc, prev_loc);
  mul_m3_v3(td->smtx, tvec);
  add_v3_v3(td->loc, tvec);

  /* TODO: support snap alignment similar to #SCE_SNAP_INDIVIDUAL_PROJECT? */
}

bool transform_snap_project_individual_is_active(const TransInfo *t)
{
  if (!transform_snap_is_active(t)) {
    return false;
  }

  return (t->tsnap.mode & (SCE_SNAP_INDIVIDUAL_PROJECT | SCE_SNAP_INDIVIDUAL_NEAREST)) != 0;
}

void transform_snap_project_individual_apply(TransInfo *t)
{
  if (!transform_snap_project_individual_is_active(t)) {
    return;
  }

  /* XXX FLICKER IN OBJECT MODE */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if ((t->flag & T_PROP_EDIT) && (td->factor == 0.0f)) {
        continue;
      }

      /* If both face ray-cast and face nearest methods are enabled, start with face ray-cast and
       * fallback to face nearest ray-cast does not hit. */
      bool hit = false;
      if (t->tsnap.mode & SCE_SNAP_INDIVIDUAL_PROJECT) {
        hit = applyFaceProject(t, tc, td);
      }

      if (!hit && t->tsnap.mode & SCE_SNAP_INDIVIDUAL_NEAREST) {
        applyFaceNearest(t, tc, td);
      }
#if 0 /* TODO: support this? */
      constraintTransLim(t, td);
#endif
    }
  }
}

static bool transform_snap_mixed_is_active(const TransInfo *t)
{
  if (!transform_snap_is_active(t)) {
    return false;
  }

  return (t->tsnap.mode &
          (SCE_SNAP_TO_VERTEX | SCE_SNAP_TO_EDGE | SCE_SNAP_TO_FACE | SCE_SNAP_TO_VOLUME |
           SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR)) != 0;
}

void transform_snap_mixed_apply(TransInfo *t, float *vec)
{
  if (!transform_snap_mixed_is_active(t)) {
    return;
  }

  if (t->tsnap.mode & ~(SCE_SNAP_TO_INCREMENT | SCE_SNAP_TO_GRID)) {
    double current = BLI_check_seconds_timer();

    /* Time base quirky code to go around find-nearest slowness. */
    /* TODO: add exception for object mode, no need to slow it down then. */
    if (current - t->tsnap.last >= 0.01) {
      if (t->tsnap.snap_target_fn) {
        t->tsnap.snap_target_fn(t, vec);
      }
      if (t->tsnap.snap_source_fn) {
        t->tsnap.snap_source_fn(t);
      }

      t->tsnap.last = current;
    }

    if (validSnap(t)) {
      t->mode_info->snap_apply_fn(t, vec);
    }
  }
}

void resetSnapping(TransInfo *t)
{
  t->tsnap.status = SNAP_RESETTED;
  t->tsnap.source_type = SCE_SNAP_TO_NONE;
  t->tsnap.target_type = SCE_SNAP_TO_NONE;
  t->tsnap.mode = SCE_SNAP_TO_NONE;
  t->tsnap.target_operation = SCE_SNAP_TARGET_ALL;
  t->tsnap.source_operation = SCE_SNAP_SOURCE_CLOSEST;
  t->tsnap.last = 0;

  t->tsnap.snapNormal[0] = 0;
  t->tsnap.snapNormal[1] = 0;
  t->tsnap.snapNormal[2] = 0;

  t->tsnap.snapNodeBorder = 0;
}

bool usingSnappingNormal(const TransInfo *t)
{
  return (t->tsnap.flag & SCE_SNAP_ROTATE) != 0;
}

bool validSnappingNormal(const TransInfo *t)
{
  if (validSnap(t)) {
    if (!is_zero_v3(t->tsnap.snapNormal)) {
      return true;
    }
  }

  return false;
}

static bool bm_edge_is_snap_target(BMEdge *e, void * /*user_data*/)
{
  if (BM_elem_flag_test(e, BM_ELEM_SELECT | BM_ELEM_HIDDEN) ||
      BM_elem_flag_test(e->v1, BM_ELEM_SELECT) || BM_elem_flag_test(e->v2, BM_ELEM_SELECT))
  {
    return false;
  }

  return true;
}

static bool bm_face_is_snap_target(BMFace *f, void * /*user_data*/)
{
  if (BM_elem_flag_test(f, BM_ELEM_SELECT | BM_ELEM_HIDDEN)) {
    return false;
  }

  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
      return false;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return true;
}

static eSnapFlag snap_flag_from_spacetype(TransInfo *t)
{
  ToolSettings *ts = t->settings;
  switch (t->spacetype) {
    case SPACE_VIEW3D:
      return eSnapFlag(ts->snap_flag);
    case SPACE_NODE:
      return eSnapFlag(ts->snap_flag_node);
    case SPACE_IMAGE:
      return eSnapFlag(ts->snap_uv_flag);
    case SPACE_SEQ:
      return eSnapFlag(ts->snap_flag_seq);
    case SPACE_GRAPH:
    case SPACE_ACTION:
    case SPACE_NLA:
      return eSnapFlag(ts->snap_flag_anim);
  }
  /* #SPACE_EMPTY.
   * It can happen when the operator is called via a handle in `bpy.app.handlers`. */
  return eSnapFlag(0);
}

static eSnapMode snap_mode_from_spacetype(TransInfo *t)
{
  ToolSettings *ts = t->settings;

  if (t->spacetype == SPACE_NODE) {
    return eSnapMode(ts->snap_node_mode);
  }

  if (t->spacetype == SPACE_IMAGE) {
    eSnapMode snap_mode = eSnapMode(ts->snap_uv_mode);
    if ((snap_mode & SCE_SNAP_TO_INCREMENT) && (ts->snap_uv_flag & SCE_SNAP_ABS_GRID) &&
        (t->mode == TFM_TRANSLATION))
    {
      snap_mode &= ~SCE_SNAP_TO_INCREMENT;
      snap_mode |= SCE_SNAP_TO_GRID;
    }
    return snap_mode;
  }

  if (t->spacetype == SPACE_SEQ) {
    return eSnapMode(SEQ_tool_settings_snap_mode_get(t->scene));
  }

  if (t->spacetype == SPACE_VIEW3D) {
    if (t->options & (CTX_CAMERA | CTX_EDGE_DATA | CTX_PAINT_CURVE)) {
      return SCE_SNAP_TO_INCREMENT;
    }

    eSnapMode snap_mode = eSnapMode(ts->snap_mode);
    if ((snap_mode & SCE_SNAP_TO_INCREMENT) && (ts->snap_flag & SCE_SNAP_ABS_GRID) &&
        (t->mode == TFM_TRANSLATION))
    {
      /* Special case in which snap to increments is transformed to snap to grid. */
      snap_mode &= ~SCE_SNAP_TO_INCREMENT;
      snap_mode |= SCE_SNAP_TO_GRID;
    }
    return snap_mode;
  }

  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_GRAPH)) {
    return eSnapMode(ts->snap_anim_mode);
  }

  return SCE_SNAP_TO_INCREMENT;
}

static eSnapTargetOP snap_target_select_from_spacetype(TransInfo *t)
{
  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Base *base_act = BKE_view_layer_active_base_get(t->view_layer);

  eSnapTargetOP ret = SCE_SNAP_TARGET_ALL;

  /* `t->tsnap.target_operation` not initialized yet. */
  BLI_assert(t->tsnap.target_operation == SCE_SNAP_TARGET_ALL);

  if (ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE) && !(t->options & CTX_CAMERA)) {
    if (base_act && (base_act->object->mode & OB_MODE_PARTICLE_EDIT)) {
      /* Particles edit mode. */
      return ret;
    }

    if (t->options & (CTX_GPENCIL_STROKES | CTX_CURSOR | CTX_OBMODE_XFORM_OBDATA)) {
      /* In "Edit Strokes" mode,
       * snap tool can perform snap to selected or active objects (see #49632)
       * TODO: perform self snap in gpencil_strokes.
       *
       * When we're moving the origins, allow snapping onto our own geometry (see #69132). */
      return ret;
    }

    const int obedit_type = t->obedit_type;
    if (obedit_type != -1) {
      /* Edit mode */
      if (obedit_type == OB_MESH) {
        /* Editing a mesh */
        if ((t->flag & T_PROP_EDIT) != 0) {
          /* Exclude editmesh when using proportional edit */
          ret |= SCE_SNAP_TARGET_NOT_EDITED;
        }
        /* UV editing must never snap to the selection as this is what is transformed. */
        if (t->spacetype == SPACE_IMAGE) {
          ret |= SCE_SNAP_TARGET_NOT_SELECTED;
        }
      }
      else if (ELEM(obedit_type, OB_ARMATURE, OB_CURVES_LEGACY, OB_SURF, OB_LATTICE, OB_MBALL)) {
        /* Temporary limited to edit mode armature, curves, surfaces, lattices, and metaballs. */
        ret |= SCE_SNAP_TARGET_NOT_SELECTED;
      }
    }
    else {
      /* Object or pose mode. */
      ret |= SCE_SNAP_TARGET_NOT_SELECTED | SCE_SNAP_TARGET_NOT_ACTIVE;
    }
  }
  else if (ELEM(t->spacetype, SPACE_NODE, SPACE_SEQ)) {
    ret |= SCE_SNAP_TARGET_NOT_SELECTED;
  }

  return ret;
}

static void snap_object_context_init(TransInfo *t)
{
  if (t->data_type == &TransConvertType_Mesh) {
    /* Ignore elements being transformed. */
    ED_transform_snap_object_context_set_editmesh_callbacks(
        t->tsnap.object_context,
        (bool (*)(BMVert *, void *))BM_elem_cb_check_hflag_disabled,
        bm_edge_is_snap_target,
        bm_face_is_snap_target,
        POINTER_FROM_UINT(BM_ELEM_SELECT | BM_ELEM_HIDDEN));
  }
  else {
    /* Ignore hidden geometry in the general case. */
    ED_transform_snap_object_context_set_editmesh_callbacks(
        t->tsnap.object_context,
        (bool (*)(BMVert *, void *))BM_elem_cb_check_hflag_disabled,
        (bool (*)(BMEdge *, void *))BM_elem_cb_check_hflag_disabled,
        (bool (*)(BMFace *, void *))BM_elem_cb_check_hflag_disabled,
        POINTER_FROM_UINT(BM_ELEM_HIDDEN));
  }
}

static void initSnappingMode(TransInfo *t)
{
  if (!transformModeUseSnap(t)) {
    /* In this case, snapping is always disabled by default. */
    t->modifiers &= ~MOD_SNAP;
  }

  if (doForceIncrementSnap(t)) {
    t->tsnap.mode = SCE_SNAP_TO_INCREMENT;
  }

  if ((t->spacetype != SPACE_VIEW3D) || (t->flag & T_NO_PROJECT)) {
    /* Force project off when not supported. */
    t->tsnap.mode &= ~(SCE_SNAP_INDIVIDUAL_PROJECT | SCE_SNAP_INDIVIDUAL_NEAREST);
  }

  if (t->tsnap.mode & SCE_SNAP_TO_EDGE_PERPENDICULAR) {
    t->flag |= T_DRAW_SNAP_SOURCE;
  }

  setSnappingCallback(t);

  if (t->spacetype == SPACE_VIEW3D) {
    if (t->tsnap.object_context == nullptr) {
      SET_FLAG_FROM_TEST(t->tsnap.flag, snap_use_backface_culling(t), SCE_SNAP_BACKFACE_CULLING);
      t->tsnap.object_context = ED_transform_snap_object_context_create(t->scene, 0);
      snap_object_context_init(t);
    }
  }
  else if (t->spacetype == SPACE_SEQ) {
    if (t->tsnap.seq_context == nullptr) {
      t->tsnap.seq_context = transform_snap_sequencer_data_alloc(t);
    }
  }
}

void initSnapping(TransInfo *t, wmOperator *op)
{
  ToolSettings *ts = t->settings;
  eSnapSourceOP snap_source = eSnapSourceOP(ts->snap_target);

  resetSnapping(t);
  t->tsnap.mode = snap_mode_from_spacetype(t);
  t->tsnap.flag = snap_flag_from_spacetype(t);
  t->tsnap.target_operation = snap_target_select_from_spacetype(t);
  t->tsnap.face_nearest_steps = max_ii(ts->snap_face_nearest_steps, 1);

  /* if snap property exists */
  PropertyRNA *prop;
  if (op && (prop = RNA_struct_find_property(op->ptr, "snap")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->modifiers |= MOD_SNAP;

      if ((prop = RNA_struct_find_property(op->ptr, "snap_elements")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        t->tsnap.mode = eSnapMode(RNA_property_enum_get(op->ptr, prop));
      }

      /* TODO(@gfxcoder): Rename `snap_target` to `snap_source` to avoid previous ambiguity of
       * "target" (now, "source" is geometry to be moved and "target" is geometry to which moved
       * geometry is snapped). */
      if ((prop = RNA_struct_find_property(op->ptr, "snap_target")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        snap_source = eSnapSourceOP(RNA_property_enum_get(op->ptr, prop));
      }

      if ((prop = RNA_struct_find_property(op->ptr, "snap_point")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        RNA_property_float_get_array(op->ptr, prop, t->tsnap.snap_target);
        t->modifiers |= MOD_SNAP_FORCED;
        t->tsnap.status |= SNAP_TARGET_FOUND;
      }

      /* snap align only defined in specific cases */
      if ((prop = RNA_struct_find_property(op->ptr, "snap_align")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        SET_FLAG_FROM_TEST(
            t->tsnap.flag, RNA_property_boolean_get(op->ptr, prop), SCE_SNAP_ROTATE);

        RNA_float_get_array(op->ptr, "snap_normal", t->tsnap.snapNormal);
        normalize_v3(t->tsnap.snapNormal);
      }

      if ((prop = RNA_struct_find_property(op->ptr, "use_snap_project")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        SET_FLAG_FROM_TEST(
            t->tsnap.mode, RNA_property_boolean_get(op->ptr, prop), SCE_SNAP_INDIVIDUAL_PROJECT);
      }

      /* use_snap_self is misnamed and should be use_snap_active */
      if ((prop = RNA_struct_find_property(op->ptr, "use_snap_self")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                           !RNA_property_boolean_get(op->ptr, prop),
                           SCE_SNAP_TARGET_NOT_ACTIVE);
      }

      if ((prop = RNA_struct_find_property(op->ptr, "use_snap_edit")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                           !RNA_property_boolean_get(op->ptr, prop),
                           SCE_SNAP_TARGET_NOT_EDITED);
      }

      if ((prop = RNA_struct_find_property(op->ptr, "use_snap_nonedit")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                           !RNA_property_boolean_get(op->ptr, prop),
                           SCE_SNAP_TARGET_NOT_NONEDITED);
      }

      if ((prop = RNA_struct_find_property(op->ptr, "use_snap_selectable")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                           RNA_property_boolean_get(op->ptr, prop),
                           SCE_SNAP_TARGET_ONLY_SELECTABLE);
      }
    }
  }
  /* use scene defaults only when transform is modal */
  else if (t->flag & T_MODAL) {
    if (t->tsnap.flag & SCE_SNAP) {
      t->modifiers |= MOD_SNAP;
    }

    SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                       (ts->snap_flag & SCE_SNAP_NOT_TO_ACTIVE),
                       SCE_SNAP_TARGET_NOT_ACTIVE);
    SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                       !(ts->snap_flag & SCE_SNAP_TO_INCLUDE_EDITED),
                       SCE_SNAP_TARGET_NOT_EDITED);
    SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                       !(ts->snap_flag & SCE_SNAP_TO_INCLUDE_NONEDITED),
                       SCE_SNAP_TARGET_NOT_NONEDITED);
    SET_FLAG_FROM_TEST(t->tsnap.target_operation,
                       (ts->snap_flag & SCE_SNAP_TO_ONLY_SELECTABLE),
                       SCE_SNAP_TARGET_ONLY_SELECTABLE);
  }

  t->tsnap.source_operation = snap_source;

  initSnappingMode(t);

  transform_snap_flag_from_modifiers_set(t);
}

void freeSnapping(TransInfo *t)
{
  if ((t->spacetype == SPACE_SEQ) && t->tsnap.seq_context) {
    transform_snap_sequencer_data_free(t->tsnap.seq_context);
    t->tsnap.seq_context = nullptr;
  }
  else if (t->tsnap.object_context) {
    ED_transform_snap_object_context_destroy(t->tsnap.object_context);
    t->tsnap.object_context = nullptr;

    ED_transform_snap_object_time_average_print();
  }
}

static void setSnappingCallback(TransInfo *t)
{
  if (t->spacetype == SPACE_VIEW3D) {
    if (t->options & CTX_CAMERA) {
      /* Not with camera selected in camera view. */
      return;
    }
    t->tsnap.snap_target_fn = snap_target_view3d_fn;
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Object *obact = BKE_view_layer_active_object_get(t->view_layer);

    const bool is_uv_editor = sima->mode == SI_MODE_UV;
    const bool has_edit_object = obact && BKE_object_is_in_editmode(obact);
    if (is_uv_editor && has_edit_object) {
      t->tsnap.snap_target_fn = snap_target_uv_fn;
    }
  }
  else if (t->spacetype == SPACE_NODE) {
    t->tsnap.snap_target_fn = snap_target_node_fn;
  }
  else if (t->spacetype == SPACE_SEQ) {
    t->tsnap.snap_target_fn = snap_target_sequencer_fn;
    /* The target is calculated along with the snap point. */
    return;
  }
  else if (t->spacetype == SPACE_NLA) {
    t->tsnap.snap_target_fn = snap_target_nla_fn;
    /* The target is calculated along with the snap point. */
    return;
  }
  else {
    return;
  }

  switch (t->tsnap.source_operation) {
    case SCE_SNAP_SOURCE_CLOSEST:
      t->tsnap.snap_source_fn = snap_source_closest_fn;
      break;
    case SCE_SNAP_SOURCE_CENTER:
      if (!ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
        t->tsnap.snap_source_fn = snap_source_center_fn;
        break;
      }
      /* Can't do TARGET_CENTER with these modes,
       * use TARGET_MEDIAN instead. */
      ATTR_FALLTHROUGH;
    case SCE_SNAP_SOURCE_MEDIAN:
      t->tsnap.snap_source_fn = snap_source_median_fn;
      break;
    case SCE_SNAP_SOURCE_ACTIVE:
      t->tsnap.snap_source_fn = snap_source_active_fn;

      /* XXX, workaround: active needs to be calculated before transforming, otherwise
       * `t->tsnap.snap_source` will be calculated with the transformed data since we're not
       * reading from 'td->center' in this case. (See: #40241 and #40348). */
      snap_source_active_fn(t);
      break;
  }
}

void addSnapPoint(TransInfo *t)
{
  /* Currently only 3D viewport works for snapping points. */
  if (t->tsnap.status & SNAP_TARGET_FOUND && t->spacetype == SPACE_VIEW3D) {
    TransSnapPoint *p = MEM_cnew<TransSnapPoint>("SnapPoint");

    t->tsnap.selectedPoint = p;

    copy_v3_v3(p->co, t->tsnap.snap_target);

    BLI_addtail(&t->tsnap.points, p);

    t->tsnap.status |= SNAP_MULTI_POINTS;
  }
}

eRedrawFlag updateSelectedSnapPoint(TransInfo *t)
{
  eRedrawFlag status = TREDRAW_NOTHING;

  if (t->tsnap.status & SNAP_MULTI_POINTS) {
    TransSnapPoint *closest_p = nullptr;
    float dist_min_sq = TRANSFORM_SNAP_MAX_PX;
    float screen_loc[2];

    LISTBASE_FOREACH (TransSnapPoint *, p, &t->tsnap.points) {
      float dist_sq;

      if (ED_view3d_project_float_global(t->region, p->co, screen_loc, V3D_PROJ_TEST_NOP) !=
          V3D_PROJ_RET_OK)
      {
        continue;
      }

      dist_sq = len_squared_v2v2(t->mval, screen_loc);

      if (dist_sq < dist_min_sq) {
        closest_p = p;
        dist_min_sq = dist_sq;
      }
    }

    if (closest_p) {
      if (t->tsnap.selectedPoint != closest_p) {
        status = TREDRAW_HARD;
      }

      t->tsnap.selectedPoint = closest_p;
    }
  }

  return status;
}

void removeSnapPoint(TransInfo *t)
{
  if (t->tsnap.status & SNAP_MULTI_POINTS) {
    updateSelectedSnapPoint(t);

    if (t->tsnap.selectedPoint) {
      BLI_freelinkN(&t->tsnap.points, t->tsnap.selectedPoint);

      if (BLI_listbase_is_empty(&t->tsnap.points)) {
        t->tsnap.status &= ~SNAP_MULTI_POINTS;
      }

      t->tsnap.selectedPoint = nullptr;
    }
  }
}

void getSnapPoint(const TransInfo *t, float vec[3])
{
  if (t->tsnap.points.first) {
    TransSnapPoint *p;
    int total = 0;

    vec[0] = vec[1] = vec[2] = 0;

    for (p = static_cast<TransSnapPoint *>(t->tsnap.points.first); p; p = p->next, total++) {
      add_v3_v3(vec, p->co);
    }

    if (t->tsnap.status & SNAP_TARGET_FOUND) {
      add_v3_v3(vec, t->tsnap.snap_target);
      total++;
    }

    mul_v3_fl(vec, 1.0f / total);
  }
  else {
    copy_v3_v3(vec, t->tsnap.snap_target);
  }
}

static void snap_multipoints_free(TransInfo *t)
{
  if (t->tsnap.status & SNAP_MULTI_POINTS) {
    BLI_freelistN(&t->tsnap.points);
    t->tsnap.status &= ~SNAP_MULTI_POINTS;
    t->tsnap.selectedPoint = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calc Snap
 * \{ */

static void snap_target_view3d_fn(TransInfo *t, float * /*vec*/)
{
  BLI_assert(t->spacetype == SPACE_VIEW3D);
  float loc[3];
  float no[3];
  bool found = false;
  eSnapMode snap_elem = SCE_SNAP_TO_NONE;
  float dist_px = SNAP_MIN_DISTANCE; /* Use a user defined value here. */

  if (t->tsnap.mode & SCE_SNAP_TO_GEOM) {
    zero_v3(no); /* objects won't set this */
    snap_elem = snapObjectsTransform(t, t->mval, &dist_px, loc, no);
    found = (snap_elem != SCE_SNAP_TO_NONE);
  }
  if ((found == false) && (t->tsnap.mode & SCE_SNAP_TO_VOLUME)) {
    bool use_peel = (t->settings->snap_flag & SCE_SNAP_PEEL_OBJECT) != 0;
    found = peelObjectsTransform(t, t->mval, use_peel, loc, no, nullptr);

    if (found) {
      snap_elem = SCE_SNAP_TO_VOLUME;
    }
  }

  if (found == true) {
    copy_v3_v3(t->tsnap.snap_target, loc);
    copy_v3_v3(t->tsnap.snapNormal, no);

    t->tsnap.status |= SNAP_TARGET_FOUND;
  }
  else {
    t->tsnap.status &= ~SNAP_TARGET_FOUND;
  }

  t->tsnap.target_type = snap_elem;
}

static void snap_target_uv_fn(TransInfo *t, float * /*vec*/)
{
  BLI_assert(t->spacetype == SPACE_IMAGE);
  if (t->tsnap.mode & SCE_SNAP_TO_VERTEX) {
    const Vector<Object *> objects =
        BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
            t->scene, t->view_layer, nullptr);

    float dist_sq = square_f(float(SNAP_MIN_DISTANCE));
    if (ED_uvedit_nearest_uv_multi(&t->region->v2d,
                                   t->scene,
                                   objects,
                                   t->mval,
                                   t->tsnap.target_operation & SCE_SNAP_TARGET_NOT_SELECTED,
                                   &dist_sq,
                                   t->tsnap.snap_target))
    {
      t->tsnap.snap_target[0] *= t->aspect[0];
      t->tsnap.snap_target[1] *= t->aspect[1];

      t->tsnap.status |= SNAP_TARGET_FOUND;
    }
    else {
      t->tsnap.status &= ~SNAP_TARGET_FOUND;
    }
  }
}

static void snap_target_node_fn(TransInfo *t, float * /*vec*/)
{
  BLI_assert(t->spacetype == SPACE_NODE);
  if (t->tsnap.mode & (SCE_SNAP_TO_NODE_X | SCE_SNAP_TO_NODE_Y)) {
    float loc[2];
    float dist_px = SNAP_MIN_DISTANCE; /* Use a user defined value here. */
    char node_border;

    if (snapNodesTransform(t, t->mval, loc, &dist_px, &node_border)) {
      copy_v2_v2(t->tsnap.snap_target, loc);
      t->tsnap.snapNodeBorder = node_border;

      t->tsnap.status |= SNAP_TARGET_FOUND;
    }
    else {
      t->tsnap.status &= ~SNAP_TARGET_FOUND;
    }
  }
}

static void snap_target_sequencer_fn(TransInfo *t, float * /*vec*/)
{
  BLI_assert(t->spacetype == SPACE_SEQ);
  if (transform_snap_sequencer_calc(t)) {
    t->tsnap.status |= (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }
  else {
    t->tsnap.status &= ~(SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }
}

static void snap_target_nla_fn(TransInfo *t, float *vec)
{
  BLI_assert(t->spacetype == SPACE_NLA);
  if (transform_snap_nla_calc(t, vec)) {
    t->tsnap.status |= (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }
  else {
    t->tsnap.status &= ~(SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Target
 * \{ */

void tranform_snap_target_median_calc(const TransInfo *t, float r_median[3])
{
  int i_accum = 0;

  zero_v3(r_median);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    int i;
    float v[3];
    zero_v3(v);

    for (i = 0; i < tc->data_len && td->flag & TD_SELECTED; i++, td++) {
      add_v3_v3(v, td->center);
    }

    if (i == 0) {
      /* Is this possible? */
      continue;
    }

    mul_v3_fl(v, 1.0 / i);

    if (tc->use_local_mat) {
      mul_m4_v3(tc->mat, v);
    }

    add_v3_v3(r_median, v);
    i_accum++;
  }

  mul_v3_fl(r_median, 1.0 / i_accum);

  // TargetSnapOffset(t, nullptr);
}

static void TargetSnapOffset(TransInfo *t, TransData *td)
{
  if (t->spacetype == SPACE_NODE && td != nullptr) {
    bNode *node = static_cast<bNode *>(td->extra);
    char border = t->tsnap.snapNodeBorder;

    if (border & NODE_LEFT) {
      t->tsnap.snap_source[0] -= 0.0f;
    }
    if (border & NODE_RIGHT) {
      t->tsnap.snap_source[0] += BLI_rctf_size_x(&node->runtime->totr);
    }
    if (border & NODE_BOTTOM) {
      t->tsnap.snap_source[1] -= BLI_rctf_size_y(&node->runtime->totr);
    }
    if (border & NODE_TOP) {
      t->tsnap.snap_source[1] += 0.0f;
    }
  }
}

static void snap_source_center_fn(TransInfo *t)
{
  /* Only need to calculate once */
  if ((t->tsnap.status & SNAP_SOURCE_FOUND) == 0) {
    copy_v3_v3(t->tsnap.snap_source, t->center_global);
    TargetSnapOffset(t, nullptr);

    t->tsnap.status |= SNAP_SOURCE_FOUND;
    t->tsnap.source_type = SCE_SNAP_TO_NONE;
  }
}

static void snap_source_active_fn(TransInfo *t)
{
  /* Only need to calculate once */
  if ((t->tsnap.status & SNAP_SOURCE_FOUND) == 0) {
    if (calculateCenterActive(t, true, t->tsnap.snap_source)) {
      TargetSnapOffset(t, nullptr);
      t->tsnap.status |= SNAP_SOURCE_FOUND;
      t->tsnap.source_type = SCE_SNAP_TO_NONE;
    }
    else {
      /* No active, default to median, */
      t->tsnap.source_operation = SCE_SNAP_SOURCE_MEDIAN;
      t->tsnap.snap_source_fn = snap_source_median_fn;
      snap_source_median_fn(t);
    }
  }
}

static void snap_source_median_fn(TransInfo *t)
{
  /* Only need to calculate once. */
  if ((t->tsnap.status & SNAP_SOURCE_FOUND) == 0) {
    tranform_snap_target_median_calc(t, t->tsnap.snap_source);
    t->tsnap.status |= SNAP_SOURCE_FOUND;
    t->tsnap.source_type = SCE_SNAP_TO_NONE;
  }
}

static void snap_source_closest_fn(TransInfo *t)
{
  /* Only valid if a snap point has been selected. */
  if (t->tsnap.status & SNAP_TARGET_FOUND) {
    float dist_closest = 0.0f;
    TransData *closest = nullptr;

    /* Object mode */
    if (t->options & CTX_OBJECT) {
      int i;
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        TransData *td;
        for (td = tc->data, i = 0; i < tc->data_len && td->flag & TD_SELECTED; i++, td++) {
          std::optional<blender::Bounds<blender::float3>> bounds;

          if ((t->options & CTX_OBMODE_XFORM_OBDATA) == 0) {
            bounds = BKE_object_boundbox_get(td->ob);
          }

          /* use boundbox if possible */
          if (bounds) {
            BoundBox bb;
            BKE_boundbox_init_from_minmax(&bb, bounds->min, bounds->max);
            int j;

            for (j = 0; j < 8; j++) {
              float loc[3];
              float dist;

              copy_v3_v3(loc, bb.vec[j]);
              mul_m4_v3(td->ext->obmat, loc);

              dist = t->mode_info->snap_distance_fn(t, loc, t->tsnap.snap_target);

              if ((dist != TRANSFORM_DIST_INVALID) &&
                  (closest == nullptr || fabsf(dist) < fabsf(dist_closest)))
              {
                copy_v3_v3(t->tsnap.snap_source, loc);
                closest = td;
                dist_closest = dist;
              }
            }
          }
          /* use element center otherwise */
          else {
            float loc[3];
            float dist;

            copy_v3_v3(loc, td->center);

            dist = t->mode_info->snap_distance_fn(t, loc, t->tsnap.snap_target);

            if ((dist != TRANSFORM_DIST_INVALID) &&
                (closest == nullptr || fabsf(dist) < fabsf(dist_closest)))
            {
              copy_v3_v3(t->tsnap.snap_source, loc);
              closest = td;
            }
          }
        }
      }
    }
    else {
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        TransData *td = tc->data;
        int i;
        for (i = 0; i < tc->data_len && td->flag & TD_SELECTED; i++, td++) {
          float loc[3];
          float dist;

          copy_v3_v3(loc, td->center);

          if (tc->use_local_mat) {
            mul_m4_v3(tc->mat, loc);
          }

          dist = t->mode_info->snap_distance_fn(t, loc, t->tsnap.snap_target);

          if ((dist != TRANSFORM_DIST_INVALID) &&
              (closest == nullptr || fabsf(dist) < fabsf(dist_closest)))
          {
            copy_v3_v3(t->tsnap.snap_source, loc);
            closest = td;
            dist_closest = dist;
          }
        }
      }
    }

    TargetSnapOffset(t, closest);

    t->tsnap.status |= SNAP_SOURCE_FOUND;
    t->tsnap.source_type = SCE_SNAP_TO_NONE;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Objects
 * \{ */

eSnapMode snapObjectsTransform(
    TransInfo *t, const float mval[2], float *dist_px, float r_loc[3], float r_no[3])
{
  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
  snap_object_params.use_occlusion_test = true;
  snap_object_params.use_backface_culling = (t->tsnap.flag & SCE_SNAP_BACKFACE_CULLING) != 0;

  float *prev_co = (t->tsnap.status & SNAP_SOURCE_FOUND) ? t->tsnap.snap_source : t->center_global;

  return ED_transform_snap_object_project_view3d(t->tsnap.object_context,
                                                 t->depsgraph,
                                                 t->region,
                                                 static_cast<const View3D *>(t->view),
                                                 t->tsnap.mode,
                                                 &snap_object_params,
                                                 nullptr,
                                                 mval,
                                                 prev_co,
                                                 dist_px,
                                                 r_loc,
                                                 r_no);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Peeling
 * \{ */

bool peelObjectsTransform(TransInfo *t,
                          const float mval[2],
                          const bool use_peel_object,
                          /* return args */
                          float r_loc[3],
                          float r_no[3],
                          float *r_thickness)
{
  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;

  ListBase depths_peel = {nullptr};
  ED_transform_snap_object_project_all_view3d_ex(t->tsnap.object_context,
                                                 t->depsgraph,
                                                 t->region,
                                                 static_cast<const View3D *>(t->view),
                                                 &snap_object_params,
                                                 mval,
                                                 -1.0f,
                                                 false,
                                                 &depths_peel);

  if (!BLI_listbase_is_empty(&depths_peel)) {
    /* At the moment we only use the hits of the first object */
    SnapObjectHitDepth *hit_min = static_cast<SnapObjectHitDepth *>(depths_peel.first);
    for (SnapObjectHitDepth *iter = hit_min->next; iter; iter = iter->next) {
      if (iter->depth < hit_min->depth) {
        hit_min = iter;
      }
    }
    SnapObjectHitDepth *hit_max = nullptr;

    if (use_peel_object) {
      /* if peeling objects, take the first and last from each object */
      hit_max = hit_min;
      LISTBASE_FOREACH (SnapObjectHitDepth *, iter, &depths_peel) {
        if ((iter->depth > hit_max->depth) && (iter->ob_uuid == hit_min->ob_uuid)) {
          hit_max = iter;
        }
      }
    }
    else {
      /* otherwise, pair first with second and so on */
      LISTBASE_FOREACH (SnapObjectHitDepth *, iter, &depths_peel) {
        if ((iter != hit_min) && (iter->ob_uuid == hit_min->ob_uuid)) {
          if (hit_max == nullptr) {
            hit_max = iter;
          }
          else if (iter->depth < hit_max->depth) {
            hit_max = iter;
          }
        }
      }
      /* In this case has only one hit. treat as ray-cast. */
      if (hit_max == nullptr) {
        hit_max = hit_min;
      }
    }

    mid_v3_v3v3(r_loc, hit_min->co, hit_max->co);

    if (r_thickness) {
      *r_thickness = hit_max->depth - hit_min->depth;
    }

    /* XXX, is there a correct normal in this case ???, for now just z up */
    r_no[0] = 0.0;
    r_no[1] = 0.0;
    r_no[2] = 1.0;

    BLI_freelistN(&depths_peel);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name snap Nodes
 * \{ */

static bool snapNodeTest(View2D *v2d, bNode *node, eSnapTargetOP snap_target_select)
{
  /* node is use for snapping only if a) snap mode matches and b) node is inside the view */
  return (((snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED) && !(node->flag & NODE_SELECT)) ||
          (snap_target_select == SCE_SNAP_TARGET_ALL && !(node->flag & NODE_ACTIVE))) &&
         (node->runtime->totr.xmin < v2d->cur.xmax && node->runtime->totr.xmax > v2d->cur.xmin &&
          node->runtime->totr.ymin < v2d->cur.ymax && node->runtime->totr.ymax > v2d->cur.ymin);
}

static NodeBorder snapNodeBorder(eSnapMode snap_node_mode)
{
  NodeBorder flag = NodeBorder(0);
  if (snap_node_mode & SCE_SNAP_TO_NODE_X) {
    flag |= NODE_LEFT | NODE_RIGHT;
  }
  if (snap_node_mode & SCE_SNAP_TO_NODE_Y) {
    flag |= NODE_TOP | NODE_BOTTOM;
  }
  return flag;
}

static bool snapNode(ToolSettings *ts,
                     SpaceNode * /*snode*/,
                     ARegion *region,
                     bNode *node,
                     const float2 &mval,
                     float r_loc[2],
                     float *r_dist_px,
                     char *r_node_border)
{
  View2D *v2d = &region->v2d;
  NodeBorder border = snapNodeBorder(eSnapMode(ts->snap_node_mode));
  bool retval = false;
  rcti totr;
  int new_dist;

  UI_view2d_view_to_region_rcti(v2d, &node->runtime->totr, &totr);

  if (border & NODE_LEFT) {
    new_dist = abs(totr.xmin - mval[0]);
    if (new_dist < *r_dist_px) {
      UI_view2d_region_to_view(v2d, totr.xmin, mval[1], &r_loc[0], &r_loc[1]);
      *r_dist_px = new_dist;
      *r_node_border = NODE_LEFT;
      retval = true;
    }
  }

  if (border & NODE_RIGHT) {
    new_dist = abs(totr.xmax - mval[0]);
    if (new_dist < *r_dist_px) {
      UI_view2d_region_to_view(v2d, totr.xmax, mval[1], &r_loc[0], &r_loc[1]);
      *r_dist_px = new_dist;
      *r_node_border = NODE_RIGHT;
      retval = true;
    }
  }

  if (border & NODE_BOTTOM) {
    new_dist = abs(totr.ymin - mval[1]);
    if (new_dist < *r_dist_px) {
      UI_view2d_region_to_view(v2d, mval[0], totr.ymin, &r_loc[0], &r_loc[1]);
      *r_dist_px = new_dist;
      *r_node_border = NODE_BOTTOM;
      retval = true;
    }
  }

  if (border & NODE_TOP) {
    new_dist = abs(totr.ymax - mval[1]);
    if (new_dist < *r_dist_px) {
      UI_view2d_region_to_view(v2d, mval[0], totr.ymax, &r_loc[0], &r_loc[1]);
      *r_dist_px = new_dist;
      *r_node_border = NODE_TOP;
      retval = true;
    }
  }

  return retval;
}

static bool snapNodes(ToolSettings *ts,
                      SpaceNode *snode,
                      ARegion *region,
                      const float2 &mval,
                      eSnapTargetOP snap_target_select,
                      float r_loc[2],
                      float *r_dist_px,
                      char *r_node_border)
{
  bNodeTree *ntree = snode->edittree;
  bool retval = false;

  *r_node_border = 0;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (snapNodeTest(&region->v2d, node, snap_target_select)) {
      retval |= snapNode(ts, snode, region, node, mval, r_loc, r_dist_px, r_node_border);
    }
  }

  return retval;
}

bool snapNodesTransform(
    TransInfo *t, const float2 &mval, float r_loc[2], float *r_dist_px, char *r_node_border)
{
  return snapNodes(t->settings,
                   static_cast<SpaceNode *>(t->area->spacedata.first),
                   t->region,
                   mval,
                   t->tsnap.target_operation,
                   r_loc,
                   r_dist_px,
                   r_node_border);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name snap Grid
 * \{ */

static void snap_increment_apply_ex(const TransInfo * /*t*/,
                                    const int max_index,
                                    const float increment_val,
                                    const float aspect[3],
                                    const float loc[3],
                                    float r_out[3])
{
  /* relative snapping in fixed increments */
  for (int i = 0; i <= max_index; i++) {
    const float iter_fac = increment_val * aspect[i];
    r_out[i] = iter_fac * roundf(loc[i] / iter_fac);
  }
}

static void snap_increment_apply(const TransInfo *t,
                                 const int max_index,
                                 const float increment_dist,
                                 float *r_val)
{
  BLI_assert(t->tsnap.mode & SCE_SNAP_TO_INCREMENT);
  BLI_assert(max_index <= 2);

  /* Early bailing out if no need to snap */
  if (increment_dist == 0.0f) {
    return;
  }

  float asp_local[3] = {1, 1, 1};
  const bool use_aspect = ELEM(t->mode, TFM_TRANSLATION);
  const float *asp = use_aspect ? t->aspect : asp_local;

  if (use_aspect) {
    /* custom aspect for fcurve */
    if (t->spacetype == SPACE_GRAPH) {
      View2D *v2d = &t->region->v2d;
      Scene *scene = t->scene;
      SpaceGraph *sipo = static_cast<SpaceGraph *>(t->area->spacedata.first);
      asp_local[0] = UI_view2d_grid_resolution_x__frames_or_seconds(
          v2d, scene, sipo->flag & SIPO_DRAWTIME);
      asp_local[1] = UI_view2d_grid_resolution_y__values(v2d);
      asp = asp_local;
    }
  }

  snap_increment_apply_ex(t, max_index, increment_dist, asp, r_val, r_val);
}

bool transform_snap_increment_ex(const TransInfo *t, bool use_local_space, float *r_val)
{
  if (!transform_snap_is_active(t)) {
    return false;
  }

  if (!(t->tsnap.mode & SCE_SNAP_TO_INCREMENT)) {
    return false;
  }

  if (t->spacetype != SPACE_VIEW3D && validSnap(t)) {
    /* Only do something if using absolute or incremental grid snapping
     * and there is no valid snap point. */
    return false;
  }

  if (use_local_space) {
    BLI_assert(t->idx_max == 2);
    mul_m3_v3(t->spacemtx_inv, r_val);
  }

  float increment_dist = (t->modifiers & MOD_PRECISION) ? t->snap[1] : t->snap[0];
  snap_increment_apply(t, t->idx_max, increment_dist, r_val);

  if (use_local_space) {
    mul_m3_v3(t->spacemtx, r_val);
  }

  return true;
}

bool transform_snap_increment(const TransInfo *t, float *r_val)
{
  return transform_snap_increment_ex(t, false, r_val);
}

float transform_snap_increment_get(const TransInfo *t)
{
  if (transform_snap_is_active(t) && (t->tsnap.mode & (SCE_SNAP_TO_INCREMENT | SCE_SNAP_TO_GRID)))
  {
    return (t->modifiers & MOD_PRECISION) ? t->snap[1] : t->snap[0];
  }

  return 0.0f;
}

void tranform_snap_source_restore_context(TransInfo *t)
{
  snap_object_context_init(t);
  snap_multipoints_free(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic callbacks
 * \{ */

float transform_snap_distance_len_squared_fn(TransInfo * /*t*/,
                                             const float p1[3],
                                             const float p2[3])
{
  return len_squared_v3v3(p1, p2);
}

/** \} */
