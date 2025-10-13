/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_time.h"

#include "DNA_userdef_types.h"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "ED_image.hh"
#include "ED_node.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_uvedit.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "SEQ_sequencer.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"
#include "transform_snap.hh"

namespace blender::ed::transform {

/* Use half of flt-max so we can scale up without an exception. */

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static void setSnappingCallback(TransInfo *t);

static void snap_target_view3d_fn(TransInfo *t, float *vec);
static void snap_target_uv_fn(TransInfo *t, float *vec);
static void snap_target_sequencer_fn(TransInfo *t, float *vec);
static void snap_target_nla_fn(TransInfo *t, float *vec);

static void snap_source_median_fn(TransInfo *t);
static void snap_source_center_fn(TransInfo *t);
static void snap_source_closest_fn(TransInfo *t);
static void snap_source_active_fn(TransInfo *t);

static eSnapMode snapObjectsTransform(
    TransInfo *t, const float mval[2], float *dist_px, float r_loc[3], float r_no[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implementations
 * \{ */

#if 0
int BIF_snappingSupported(Object *obedit)
{
  int status = 0;

  /* Only support object mesh, armature, curves. */
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
  /* The VSE and animation editors should not depend on the snapping options of the 3D viewport. */
  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA, SPACE_SEQ)) {
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

  if (t->spacetype == SPACE_SEQ && ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
    return true;
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
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

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
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

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
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

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
  else if (t->spacetype == SPACE_SEQ) {
    const ARegion *region = t->region;
    GPU_blend(GPU_BLEND_ALPHA);
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4ubv(col);
    float pixelx = BLI_rctf_size_x(&region->v2d.cur) / BLI_rcti_size_x(&region->v2d.mask);

    if (region->regiontype == RGN_TYPE_PREVIEW) {
      if (t->tsnap.direction & DIR_GLOBAL_X) {
        immRectf(pos,
                 t->tsnap.snap_target[0] - pixelx,
                 region->v2d.cur.ymax,
                 t->tsnap.snap_target[0] + pixelx,
                 region->v2d.cur.ymin);
      }
      if (t->tsnap.direction & DIR_GLOBAL_Y) {
        immRectf(pos,
                 region->v2d.cur.xmin,
                 t->tsnap.snap_target[1] - pixelx,
                 region->v2d.cur.xmax,
                 t->tsnap.snap_target[1] + pixelx);
      }
    }
    else {
      immRectf(pos,
               t->tsnap.snap_target[0] - pixelx,
               region->v2d.cur.ymax,
               t->tsnap.snap_target[0] + pixelx,
               region->v2d.cur.ymin);
    }

    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
  }
}

eRedrawFlag handleSnapping(TransInfo *t, const wmEvent *event)
{
  eRedrawFlag status = TREDRAW_NOTHING;

#if 0 /* XXX: need a proper selector for all snap mode. */
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

static bool applyFaceProject(TransInfo *t,
                             TransDataContainer *tc,
                             TransData *td,
                             TransDataExtension *td_ext)
{
  float iloc[3], loc[3], no[3];
  float mval_fl[2];

  copy_v3_v3(iloc, td->loc);
  if (tc->use_local_mat) {
    mul_m4_v3(tc->mat, iloc);
  }
  else if (t->options & CTX_OBJECT) {
    Object *ob = static_cast<Object *>(td->extra);
    BKE_object_eval_transform_all(t->depsgraph, t->scene, ob);
    copy_v3_v3(iloc, ob->object_to_world().location());
  }

  if (ED_view3d_project_float_global(t->region, iloc, mval_fl, V3D_PROJ_TEST_NOP) !=
      V3D_PROJ_RET_OK)
  {
    return false;
  }

  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
  snap_object_params.occlusion_test = SNAP_OCCLUSION_ALWAYS;
  snap_object_params.use_backface_culling = (t->tsnap.flag & SCE_SNAP_BACKFACE_CULLING) != 0;

  eSnapMode hit = blender::ed::transform::snap_object_project_view3d(
      t->tsnap.object_context,
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
    /* Handle alignment as well. */
    const float *original_normal;
    float mat[3][3];

    /* In pose mode, we want to align normals with Y axis of bones. */
    original_normal = td->axismtx[2];

    rotation_between_vecs_to_mat3(mat, original_normal, no);

    transform_data_ext_rotate(td, td_ext, mat, true);

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
    Object *ob = static_cast<Object *>(td->extra);
    BKE_object_eval_transform_all(t->depsgraph, t->scene, ob);
    copy_v3_v3(init_loc, ob->object_to_world().location());
  }

  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
  snap_object_params.occlusion_test = SNAP_OCCLUSION_ALWAYS;
  snap_object_params.use_backface_culling = false;
  snap_object_params.face_nearest_steps = t->tsnap.face_nearest_steps;
  snap_object_params.keep_on_same_target = t->tsnap.flag & SCE_SNAP_KEEP_ON_SAME_OBJECT;

  eSnapMode hit = blender::ed::transform::snap_object_project_view3d(
      t->tsnap.object_context,
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

  /* XXX: flickers in object mode. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    TransDataExtension *td_ext = tc->data_ext;
    for (int i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      if ((t->flag & T_PROP_EDIT) && (td->factor == 0.0f)) {
        continue;
      }

      /* If both face ray-cast and face nearest methods are enabled, start with face ray-cast and
       * fall back to face nearest ray-cast does not hit. */
      bool hit = false;
      if (t->tsnap.mode & SCE_SNAP_INDIVIDUAL_PROJECT) {
        hit = applyFaceProject(t, tc, td, td_ext);
        if (td_ext) {
          td_ext++;
        }
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
           SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR | SCE_SNAP_TO_GRID)) != 0;
}

void transform_snap_mixed_apply(TransInfo *t, float *vec)
{
  if (!transform_snap_mixed_is_active(t)) {
    return;
  }

  if (t->tsnap.mode != SCE_SNAP_TO_INCREMENT) {
    double current = BLI_time_now_seconds();

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

short *transform_snap_flag_from_spacetype_ptr(TransInfo *t, const PropertyRNA **r_prop = nullptr)
{
  ToolSettings *ts = t->settings;
  switch (t->spacetype) {
    case SPACE_VIEW3D:
      if (r_prop) {
        *r_prop = &rna_ToolSettings_use_snap;
      }
      return &ts->snap_flag;
    case SPACE_NODE:
      if (r_prop) {
        *r_prop = &rna_ToolSettings_use_snap_node;
      }
      return &ts->snap_flag_node;
    case SPACE_IMAGE:
      if (r_prop) {
        *r_prop = &rna_ToolSettings_use_snap_uv;
      }
      return &ts->snap_uv_flag;
    case SPACE_SEQ:
      if (r_prop) {
        *r_prop = &rna_ToolSettings_use_snap_sequencer;
      }
      return &ts->snap_flag_seq;
    case SPACE_ACTION:
    case SPACE_NLA:
      if (r_prop) {
        *r_prop = &rna_ToolSettings_use_snap_anim;
      }
      return &ts->snap_flag_anim;
    case SPACE_GRAPH: {
      SpaceGraph *graph_editor = static_cast<SpaceGraph *>(t->area->spacedata.first);
      switch (graph_editor->mode) {
        case SIPO_MODE_DRIVERS:
          /* The driver editor has a separate snapping flag so it can be kept disabled while
           * keeping it enabled in the Graph Editor. */
          return &ts->snap_flag_driver;

        case SIPO_MODE_ANIMATION: {
          if (r_prop) {
            *r_prop = &rna_ToolSettings_use_snap_anim;
          }
          return &ts->snap_flag_anim;
        }
        default:
          BLI_assert_unreachable();
          break;
      }
    }
  }
  /* #SPACE_EMPTY.
   * It can happen when the operator is called via a handle in `bpy.app.handlers`. */
  return nullptr;
}

static eSnapFlag snap_flag_from_spacetype(TransInfo *t)
{
  if (short *snap_flag = transform_snap_flag_from_spacetype_ptr(t)) {
    return eSnapFlag(*snap_flag);
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
    return eSnapMode(ts->snap_uv_mode);
  }

  if (t->spacetype == SPACE_SEQ) {
    return eSnapMode(seq::tool_settings_snap_mode_get(t->scene));
  }

  if (t->spacetype == SPACE_VIEW3D) {
    if (t->options & (CTX_CAMERA | CTX_EDGE_DATA | CTX_PAINT_CURVE)) {
      return SCE_SNAP_TO_INCREMENT;
    }
    eSnapMode snap_mode = eSnapMode(ts->snap_mode);
    if (t->mode == TFM_TRANSLATION) {
      /* Use grid-snap for absolute snap while translating, see: #147246. */
      if ((snap_mode & SCE_SNAP_TO_INCREMENT) && (ts->snap_flag & SCE_SNAP_ABS_GRID)) {
        snap_mode &= ~SCE_SNAP_TO_INCREMENT;
        snap_mode |= SCE_SNAP_TO_GRID;
      }
    }
    return snap_mode;
  }

  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA)) {
    return eSnapMode(ts->snap_anim_mode);
  }

  if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *graph_editor = static_cast<SpaceGraph *>(t->area->spacedata.first);
    switch (graph_editor->mode) {
      case SIPO_MODE_DRIVERS:
        /* Snapping to full values is the only mode that currently makes
         * sense for the driver editor. */
        return SCE_SNAP_TO_FRAME;

      case SIPO_MODE_ANIMATION:
        return eSnapMode(ts->snap_anim_mode);

      default:
        BLI_assert_unreachable();
        break;
    }
  }

  return SCE_SNAP_TO_INCREMENT;
}

static eSnapTargetOP snap_target_select_from_spacetype_and_tool_settings(TransInfo *t)
{
  /* `t->tsnap.target_operation` not initialized yet. */
  BLI_assert(t->tsnap.target_operation == SCE_SNAP_TARGET_ALL);

  eSnapTargetOP target_operation = SCE_SNAP_TARGET_ALL;

  if (ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE) && !(t->options & CTX_CAMERA)) {
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Base *base_act = BKE_view_layer_active_base_get(t->view_layer);
    const int obedit_type = t->obedit_type;
    if (base_act && (base_act->object->mode & OB_MODE_PARTICLE_EDIT)) {
      /* Particles edit mode. */
    }
    else if (t->options & (CTX_GPENCIL_STROKES | CTX_CURSOR | CTX_OBMODE_XFORM_OBDATA)) {
      /* In "Edit Strokes" mode,
       * snap tool can perform snap to selected or active objects (see #49632)
       * TODO: perform self snap in gpencil_strokes.
       *
       * When we're moving the origins, allow snapping onto our own geometry (see #69132). */
    }
    else if (obedit_type != -1) {
      /* Edit mode. */
      if (obedit_type == OB_MESH) {
        /* Editing a mesh. */
        if ((t->flag & T_PROP_EDIT) != 0) {
          /* Exclude editmesh when using proportional edit. */
          target_operation |= SCE_SNAP_TARGET_NOT_EDITED;
        }
        /* UV editing must never snap to the selection as this is what is transformed. */
        if (t->spacetype == SPACE_IMAGE) {
          target_operation |= SCE_SNAP_TARGET_NOT_SELECTED;
        }
      }
      else if (ELEM(obedit_type, OB_ARMATURE, OB_CURVES_LEGACY, OB_SURF, OB_LATTICE, OB_MBALL)) {
        /* Temporary limited to edit mode armature, curves, surfaces, lattices, and meta-balls.
         */
        target_operation |= SCE_SNAP_TARGET_NOT_SELECTED;
      }
    }
    else {
      /* Object or pose mode. */
      target_operation |= SCE_SNAP_TARGET_NOT_SELECTED | SCE_SNAP_TARGET_NOT_ACTIVE;
    }
  }
  else if (ELEM(t->spacetype, SPACE_NODE, SPACE_SEQ)) {
    target_operation |= SCE_SNAP_TARGET_NOT_SELECTED;
  }

  /* Use scene defaults only when transform is modal. */
  if (t->flag & T_MODAL) {
    ToolSettings *ts = t->settings;
    SET_FLAG_FROM_TEST(
        target_operation, (ts->snap_flag & SCE_SNAP_NOT_TO_ACTIVE), SCE_SNAP_TARGET_NOT_ACTIVE);
    SET_FLAG_FROM_TEST(target_operation,
                       !(ts->snap_flag & SCE_SNAP_TO_INCLUDE_EDITED),
                       SCE_SNAP_TARGET_NOT_EDITED);
    SET_FLAG_FROM_TEST(target_operation,
                       !(ts->snap_flag & SCE_SNAP_TO_INCLUDE_NONEDITED),
                       SCE_SNAP_TARGET_NOT_NONEDITED);
    SET_FLAG_FROM_TEST(target_operation,
                       (ts->snap_flag & SCE_SNAP_TO_ONLY_SELECTABLE),
                       SCE_SNAP_TARGET_ONLY_SELECTABLE);
  }

  return target_operation;
}

static void snap_object_context_init(TransInfo *t)
{
  if (t->data_type == &TransConvertType_Mesh) {
    /* Ignore elements being transformed. */
    blender::ed::transform::snap_object_context_set_editmesh_callbacks(
        t->tsnap.object_context,
        (bool (*)(BMVert *, void *))BM_elem_cb_check_hflag_disabled,
        bm_edge_is_snap_target,
        bm_face_is_snap_target,
        POINTER_FROM_UINT(BM_ELEM_SELECT | BM_ELEM_HIDDEN));
  }
  else {
    /* Ignore hidden geometry in the general case. */
    blender::ed::transform::snap_object_context_set_editmesh_callbacks(
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
  else if (t->flag & T_MODAL) {
    /* Use scene defaults only when transform is modal. */
    if (t->tsnap.flag & SCE_SNAP) {
      t->modifiers |= MOD_SNAP;
    }
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
}

void transform_snap_grid_init(const TransInfo *t, float r_snap[3], float *r_snap_precision)
{
  /* Default values. */
  r_snap[0] = r_snap[1] = 1.0f;
  r_snap[2] = 0.0f;
  *r_snap_precision = 0.1f;

  if (t->spacetype == SPACE_VIEW3D) {
    /* Used by incremental snap. */
    if (t->region->regiontype == RGN_TYPE_WINDOW) {
      View3D *v3d = static_cast<View3D *>(t->area->spacedata.first);
      r_snap[0] = r_snap[1] = r_snap[2] = ED_view3d_grid_view_scale(
          t->scene, v3d, t->region, nullptr);
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
    const View2D *v2d = &t->region->v2d;
    int grid_size = SI_GRID_STEPS_LEN;
    float zoom_factor = ED_space_image_zoom_level(v2d, grid_size);
    float grid_steps_x[SI_GRID_STEPS_LEN];
    float grid_steps_y[SI_GRID_STEPS_LEN];

    ED_space_image_grid_steps(sima, grid_steps_x, grid_steps_y, grid_size);
    /* Snapping value based on what type of grid is used (adaptive-subdividing or custom-grid). */
    r_snap[0] = ED_space_image_increment_snap_value(grid_size, grid_steps_x, zoom_factor);
    r_snap[1] = ED_space_image_increment_snap_value(grid_size, grid_steps_y, zoom_factor);
    *r_snap_precision = 0.5f;
  }
  else if (t->spacetype == SPACE_CLIP) {
    r_snap[0] = r_snap[1] = 0.125f;
    *r_snap_precision = 0.5f;
  }
  else if (t->spacetype == SPACE_NODE) {
    r_snap[0] = r_snap[1] = space_node::grid_size_get();
  }
}

void transform_snap_reset_from_mode(TransInfo *t, wmOperator *op)
{
  ToolSettings *ts = t->settings;
  eSnapSourceOP snap_source = eSnapSourceOP(ts->snap_target);

  resetSnapping(t);

  t->tsnap.mode = snap_mode_from_spacetype(t);
  t->tsnap.flag = snap_flag_from_spacetype(t);
  t->tsnap.target_operation = snap_target_select_from_spacetype_and_tool_settings(t);
  t->tsnap.face_nearest_steps = max_ii(ts->snap_face_nearest_steps, 1);

  initSnappingMode(t);

  /* Overwrite defaults with values ​​in properties. */
  PropertyRNA *prop;
  if (op && (prop = RNA_struct_find_property(op->ptr, "snap"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      SET_FLAG_FROM_TEST(t->modifiers, RNA_property_boolean_get(op->ptr, prop), MOD_SNAP);
    }

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

    /* Snap align only defined in specific cases. */
    if ((prop = RNA_struct_find_property(op->ptr, "snap_align")) &&
        RNA_property_is_set(op->ptr, prop))
    {
      SET_FLAG_FROM_TEST(t->tsnap.flag, RNA_property_boolean_get(op->ptr, prop), SCE_SNAP_ROTATE);

      RNA_float_get_array(op->ptr, "snap_normal", t->tsnap.snapNormal);
      normalize_v3(t->tsnap.snapNormal);
    }

    if ((prop = RNA_struct_find_property(op->ptr, "use_snap_project")) &&
        RNA_property_is_set(op->ptr, prop))
    {
      SET_FLAG_FROM_TEST(
          t->tsnap.mode, RNA_property_boolean_get(op->ptr, prop), SCE_SNAP_INDIVIDUAL_PROJECT);
    }

    /* Use_snap_self is misnamed and should be use_snap_active. */
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

  t->tsnap.source_operation = snap_source;
  transform_snap_flag_from_modifiers_set(t);
}

void initSnapping(TransInfo *t, wmOperator *op)
{
  transform_snap_reset_from_mode(t, op);
  transform_snap_grid_init(t, t->snap_spatial, &t->snap_spatial_precision);
  setSnappingCallback(t);

  if (t->spacetype == SPACE_VIEW3D) {
    if (t->tsnap.object_context == nullptr) {
      SET_FLAG_FROM_TEST(t->tsnap.flag, snap_use_backface_culling(t), SCE_SNAP_BACKFACE_CULLING);
      t->tsnap.object_context = snap_object_context_create(t->scene, 0);
      snap_object_context_init(t);
    }
  }
  else if (t->spacetype == SPACE_SEQ) {
    if (t->tsnap.seq_context == nullptr) {
      t->tsnap.seq_context = snap_sequencer_data_alloc(t);
    }
  }

  /* Default increment values. */
  t->increment = float3(1.0f);
  t->increment_precision = 0.1f;
}

void freeSnapping(TransInfo *t)
{
  if ((t->spacetype == SPACE_SEQ) && t->tsnap.seq_context) {
    snap_sequencer_data_free(t->tsnap.seq_context);
    t->tsnap.seq_context = nullptr;
  }
  else if (t->tsnap.object_context) {
    blender::ed::transform::snap_object_context_destroy(t->tsnap.object_context);
    t->tsnap.object_context = nullptr;

    ED_transform_snap_object_time_average_print();
  }
}

void initSnapAngleIncrements(TransInfo *t)
{
  /* The final value of increment with precision is `t->increment[0] * t->increment_precision`.
   * Therefore, we divide `snap_angle_increment_*_precision` by `snap_angle_increment_*`
   * to compute `increment_precision`. */
  float increment;
  float increment_precision;
  if (t->spacetype == SPACE_VIEW3D) {
    increment = t->settings->snap_angle_increment_3d;
    increment_precision = t->settings->snap_angle_increment_3d_precision;
  }
  else {
    increment = t->settings->snap_angle_increment_2d;
    increment_precision = t->settings->snap_angle_increment_2d_precision;
  }

  t->increment[0] = increment;
  if (increment != 0.0f) {
    t->increment_precision = float(double(increment_precision) / double(increment));
  }
  else {
    t->increment_precision = 1.0f;
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
    /* Pass. */
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
    TransSnapPoint *p = MEM_callocN<TransSnapPoint>("SnapPoint");

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

static void snap_grid_uv_apply(TransInfo *t, const float grid_dist[2], float r_out[2])
{
  float3 in;
  convertViewVec(t, in, t->mval[0] - t->center2d[0], t->mval[1] - t->center2d[1]);

  if (t->con.mode & CON_APPLY) {
    /* We need to clear the previous Snap to Grid result,
     * otherwise #t->con.applyVec will have no effect. */
    t->tsnap.target_type = SCE_SNAP_TO_NONE;
    t->tsnap.status &= ~SNAP_TARGET_FOUND;
    transform_constraint_get_nearest(t, in, in);
  }

  const float *center_global = t->center_global;
  for (int i = 0; i < 2; i++) {
    const float iter_fac = grid_dist[i];
    r_out[i] = iter_fac * roundf((in[i] + center_global[i]) / iter_fac);
  }
}

static bool snap_grid_uv(TransInfo *t, float r_val[2])
{
  float grid_dist[2];
  mul_v2_v2v2(grid_dist, t->snap_spatial, t->aspect);
  if (t->modifiers & MOD_PRECISION) {
    mul_v2_fl(grid_dist, t->snap_spatial_precision);
  }

  /* Early bailing out if no need to snap */
  if (is_zero_v2(grid_dist)) {
    return false;
  }

  snap_grid_uv_apply(t, grid_dist, r_val);
  t->tsnap.target_type = SCE_SNAP_TO_GRID;
  return true;
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

  if (t->tsnap.mode & (SCE_SNAP_TO_GEOM | SCE_SNAP_TO_GRID)) {
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

    if (snap_elem == SCE_SNAP_TO_GRID && t->mode_info != &TransMode_translate) {
      /* Change it to #SCE_SNAP_TO_POINT so we can see the symbol for other modes. */
      snap_elem = SCE_SNAP_TO_POINT;
    }
  }
  else {
    t->tsnap.status &= ~SNAP_TARGET_FOUND;
  }

  t->tsnap.target_type = snap_elem;
}

static void snap_target_uv_fn(TransInfo *t, float * /*vec*/)
{
  BLI_assert(t->spacetype == SPACE_IMAGE);
  bool found = false;
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
      t->tsnap.target_type = SCE_SNAP_TO_EDGE_ENDPOINT;
      found = true;
    }
  }

  if (!found && (t->tsnap.mode & SCE_SNAP_TO_GRID)) {
    found = snap_grid_uv(t, t->tsnap.snap_target);
  }

  SET_FLAG_FROM_TEST(t->tsnap.status, found, SNAP_TARGET_FOUND);
}

static void snap_target_sequencer_fn(TransInfo *t, float * /*vec*/)
{
  BLI_assert(t->spacetype == SPACE_SEQ);
  if (snap_sequencer_calc(t)) {
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
    float v[3];
    zero_v3(v);

    int num_selected = 0;
    tc->foreach_index_selected([&](const int i) {
      add_v3_v3(v, tc->data[i].center);
      num_selected++;
    });

    if (num_selected == 0) {
      /* Is this possible? */
      continue;
    }

    mul_v3_fl(v, 1.0 / num_selected);

    if (tc->use_local_mat) {
      mul_m4_v3(tc->mat, v);
    }

    add_v3_v3(r_median, v);
    i_accum++;
  }

  mul_v3_fl(r_median, 1.0 / i_accum);
}

static void snap_source_center_fn(TransInfo *t)
{
  /* Only need to calculate once. */
  if ((t->tsnap.status & SNAP_SOURCE_FOUND) == 0) {
    copy_v3_v3(t->tsnap.snap_source, t->center_global);

    t->tsnap.status |= SNAP_SOURCE_FOUND;
    t->tsnap.source_type = SCE_SNAP_TO_NONE;
  }
}

static void snap_source_active_fn(TransInfo *t)
{
  /* Only need to calculate once. */
  if ((t->tsnap.status & SNAP_SOURCE_FOUND) == 0) {
    if (calculateCenterActive(t, true, t->tsnap.snap_source)) {
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
  if (!(t->tsnap.status & SNAP_TARGET_FOUND)) {
    return;
  }

  if (t->tsnap.target_type == SCE_SNAP_TO_GRID) {
    /* Previously Snap to Grid had its own snap source which was always the result of
     * #snap_source_median_fn. Now this mode shares the same code, so to not change the behavior
     * too much when using Closest, use the transform pivot as the snap source in this case. */
    if (t->tsnap.source_type != SCE_SNAP_TO_POINT) {
      tranform_snap_target_median_calc(t, t->tsnap.snap_source);
      /* Use #SCE_SNAP_TO_POINT to differentiate from 'Closest' bounds and thus avoid recalculating
       * the median center. */
      t->tsnap.source_type = SCE_SNAP_TO_POINT;
    }
  }
  else {
    float dist_closest = 0.0f;
    TransData *closest = nullptr;

    /* Object mode. */
    if (t->options & CTX_OBJECT) {
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        tc->foreach_index_selected([&](const int i) {
          TransData *td = &tc->data[i];

          std::optional<Bounds<float3>> bounds;

          if ((t->options & CTX_OBMODE_XFORM_OBDATA) == 0) {
            Object *ob = static_cast<Object *>(td->extra);
            bounds = BKE_object_boundbox_eval_cached_get(ob);
          }

          /* Use bound-box if possible. */
          if (bounds) {
            TransDataExtension *td_ext = &tc->data_ext[i];
            const std::array<float3, 8> bounds_corners = bounds::corners(*bounds);
            int j;

            for (j = 0; j < 8; j++) {
              float loc[3];
              float dist;

              copy_v3_v3(loc, bounds_corners[j]);
              mul_m4_v3(td_ext->obmat, loc);

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
          /* Use element center otherwise. */
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
        });
      }
    }
    else {
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        tc->foreach_index_selected([&](const int i) {
          TransData *td = &tc->data[i];

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
        });
      }
    }
  }

  t->tsnap.status |= SNAP_SOURCE_FOUND;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Objects
 * \{ */

static eSnapMode snapObjectsTransform(
    TransInfo *t, const float mval[2], float *dist_px, float r_loc[3], float r_no[3])
{
  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.grid_size = (t->modifiers & MOD_PRECISION) ?
                                     t->snap_spatial[0] * t->snap_spatial_precision :
                                     t->snap_spatial[0];
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;
  snap_object_params.occlusion_test = SNAP_OCCLUSION_AS_SEEM;
  snap_object_params.use_backface_culling = (t->tsnap.flag & SCE_SNAP_BACKFACE_CULLING) != 0;

  float *prev_co = (t->tsnap.status & SNAP_SOURCE_FOUND) ? t->tsnap.snap_source : t->center_global;
  float *grid_co = nullptr, grid_co_stack[3];
  if ((t->tsnap.mode & SCE_SNAP_TO_GRID) && (t->con.mode & CON_APPLY) && t->mode != TFM_ROTATION) {
    /* Without this position adjustment, the snap may be far from the expected constraint point. */
    grid_co = grid_co_stack;
    convertViewVec(t, grid_co, mval[0] - t->center2d[0], mval[1] - t->center2d[1]);
    t->tsnap.status &= ~SNAP_TARGET_FOUND;
    transform_constraint_get_nearest(t, grid_co, grid_co);
    add_v3_v3(grid_co, t->center_global);
  }

  return blender::ed::transform::snap_object_project_view3d(t->tsnap.object_context,
                                                            t->depsgraph,
                                                            t->region,
                                                            static_cast<const View3D *>(t->view),
                                                            t->tsnap.mode,
                                                            &snap_object_params,
                                                            grid_co,
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
                          /* Return args. */
                          float r_loc[3],
                          float r_no[3],
                          float *r_thickness)
{
  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = t->tsnap.target_operation;
  snap_object_params.edit_mode_type = (t->flag & T_EDIT) != 0 ? SNAP_GEOM_EDIT : SNAP_GEOM_FINAL;

  ListBase depths_peel = {nullptr};
  blender::ed::transform::object_project_all_view3d_ex(t->tsnap.object_context,
                                                       t->depsgraph,
                                                       t->region,
                                                       static_cast<const View3D *>(t->view),
                                                       &snap_object_params,
                                                       mval,
                                                       -1.0f,
                                                       false,
                                                       &depths_peel);

  if (!BLI_listbase_is_empty(&depths_peel)) {
    /* At the moment we only use the hits of the first object. */
    SnapObjectHitDepth *hit_min = static_cast<SnapObjectHitDepth *>(depths_peel.first);
    for (SnapObjectHitDepth *iter = hit_min->next; iter; iter = iter->next) {
      if (iter->depth < hit_min->depth) {
        hit_min = iter;
      }
    }
    SnapObjectHitDepth *hit_max = nullptr;

    if (use_peel_object) {
      /* If peeling objects, take the first and last from each object. */
      hit_max = hit_min;
      LISTBASE_FOREACH (SnapObjectHitDepth *, iter, &depths_peel) {
        if ((iter->depth > hit_max->depth) && (iter->ob_uuid == hit_min->ob_uuid)) {
          hit_max = iter;
        }
      }
    }
    else {
      /* Otherwise, pair first with second and so on. */
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

    /* XXX, is there a correct normal in this case ???, for now just z up. */
    r_no[0] = 0.0;
    r_no[1] = 0.0;
    r_no[2] = 1.0;

    LISTBASE_FOREACH_MUTABLE (SnapObjectHitDepth *, link, &depths_peel) {
      MEM_delete(link);
    }
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name snap Grid
 * \{ */

static void snap_increment_apply(const TransInfo *t, const float loc[3], float r_out[3])
{
  bool use_precision = (t->modifiers & MOD_PRECISION) != 0;

  /* Relative snapping in fixed increments. */
  for (int i = 0; i <= t->idx_max; i++) {
    const float iter_fac = use_precision ? t->increment[i] * t->increment_precision :
                                           t->increment[i];
    if (iter_fac != 0.0f) {
      r_out[i] = iter_fac * roundf(loc[i] / iter_fac);
    }
  }
}

bool transform_snap_increment_ex(const TransInfo *t, bool use_local_space, float *r_val)
{
  if (!transform_snap_is_active(t)) {
    return false;
  }

  if (t->spacetype == SPACE_SEQ) {
    /* Sequencer has its own dedicated enum for snap_mode with increment snap bit overridden. */
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
    mul_m3_v3(t->spacemtx_inv, r_val);
  }

  snap_increment_apply(t, r_val, r_val);

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
    return (t->modifiers & MOD_PRECISION) ? t->increment[0] * t->increment_precision :
                                            t->increment[0];
  }

  return 0.0f;
}

void tranform_snap_source_restore_context(TransInfo *t)
{
  if (t->spacetype == SPACE_VIEW3D) {
    snap_object_context_init(t);
  }
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

}  // namespace blender::ed::transform
