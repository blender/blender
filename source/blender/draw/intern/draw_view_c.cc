/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Contains dynamic drawing using immediate mode
 */

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_types.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_paint.h"

#include "view3d_intern.h"

#include "draw_manager.h"

/* ******************** region info ***************** */

void DRW_draw_region_info()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  DRW_draw_cursor();

  view3d_draw_region_info(draw_ctx->evil_C, region);
}

/* **************************** 3D Cursor ******************************** */

static bool is_cursor_visible(const DRWContextState *draw_ctx, Scene *scene, ViewLayer *view_layer)
{
  if (G.moving & G_TRANSFORM_CURSOR) {
    return true;
  }

  View3D *v3d = draw_ctx->v3d;
  if ((v3d->flag2 & V3D_HIDE_OVERLAYS) || (v3d->overlay.flag & V3D_OVERLAY_HIDE_CURSOR)) {
    return false;
  }

  /* don't draw cursor in paint modes, but with a few exceptions */
  if ((draw_ctx->object_mode & (OB_MODE_ALL_PAINT | OB_MODE_SCULPT_CURVES)) != 0) {
    /* exception: object is in weight paint and has deforming armature in pose mode */
    if (draw_ctx->object_mode & OB_MODE_WEIGHT_PAINT) {
      if (BKE_object_pose_armature_get(draw_ctx->obact) != nullptr) {
        return true;
      }
    }
    /* exception: object in texture paint mode, clone brush, use_clone_layer disabled */
    else if (draw_ctx->object_mode & OB_MODE_TEXTURE_PAINT) {
      const Paint *p = BKE_paint_get_active(scene, view_layer);

      if (p && p->brush && p->brush->imagepaint_tool == PAINT_TOOL_CLONE) {
        if ((scene->toolsettings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE) == 0) {
          return true;
        }
      }
    }

    /* no exception met? then don't draw cursor! */
    return false;
  }
  if (draw_ctx->object_mode & OB_MODE_WEIGHT_GPENCIL_LEGACY) {
    /* grease pencil hide always in some modes */
    return false;
  }

  return true;
}

void DRW_draw_cursor()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;
  Scene *scene = draw_ctx->scene;
  ViewLayer *view_layer = draw_ctx->view_layer;

  GPU_color_mask(true, true, true, true);
  GPU_depth_mask(false);
  GPU_depth_test(GPU_DEPTH_NONE);

  if (is_cursor_visible(draw_ctx, scene, view_layer)) {
    int co[2];

    /* Get cursor data into quaternion form */
    const View3DCursor *cursor = &scene->cursor;

    if (ED_view3d_project_int_global(
            region, cursor->location, co, V3D_PROJ_TEST_NOP | V3D_PROJ_TEST_CLIP_NEAR) ==
        V3D_PROJ_RET_OK)
    {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

      float cursor_quat[4];
      BKE_scene_cursor_rot_to_quat(cursor, cursor_quat);

      /* Draw nice Anti Aliased cursor. */
      GPU_line_width(1.0f);
      GPU_blend(GPU_BLEND_ALPHA);
      GPU_line_smooth(true);

      float eps = 1e-5f;
      rv3d->viewquat[0] = -rv3d->viewquat[0];
      bool is_aligned = compare_v4v4(cursor_quat, rv3d->viewquat, eps);
      if (is_aligned == false) {
        float tquat[4];
        rotation_between_quats_to_quat(tquat, rv3d->viewquat, cursor_quat);
        is_aligned = tquat[0] - eps < -1.0f;
      }
      rv3d->viewquat[0] = -rv3d->viewquat[0];

      /* Draw lines */
      if (is_aligned == false) {
        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
        immUniformThemeColor3(TH_VIEW_OVERLAY);
        immBegin(GPU_PRIM_LINES, 12);

        const float scale = ED_view3d_pixel_size_no_ui_scale(rv3d, cursor->location) *
                            U.widget_unit;

#define CURSOR_VERT(axis_vec, axis, fac) \
  immVertex3f(pos, \
              cursor->location[0] + axis_vec[0] * (fac), \
              cursor->location[1] + axis_vec[1] * (fac), \
              cursor->location[2] + axis_vec[2] * (fac))

#define CURSOR_EDGE(axis_vec, axis, sign) \
  { \
    CURSOR_VERT(axis_vec, axis, sign 1.0f); \
    CURSOR_VERT(axis_vec, axis, sign 0.25f); \
  } \
  ((void)0)

        for (int axis = 0; axis < 3; axis++) {
          float axis_vec[3] = {0};
          axis_vec[axis] = scale;
          mul_qt_v3(cursor_quat, axis_vec);
          CURSOR_EDGE(axis_vec, axis, +);
          CURSOR_EDGE(axis_vec, axis, -);
        }

#undef CURSOR_VERT
#undef CURSOR_EDGE

        immEnd();
        immUnbindProgram();
      }

      float original_proj[4][4];
      GPU_matrix_projection_get(original_proj);
      GPU_matrix_push();
      ED_region_pixelspace(region);
      GPU_matrix_translate_2f(co[0] + 0.5f, co[1] + 0.5f);
      GPU_matrix_scale_2f(U.widget_unit, U.widget_unit);

      GPUBatch *cursor_batch = DRW_cache_cursor_get(is_aligned);
      GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_FLAT_COLOR);
      GPU_batch_set_shader(cursor_batch, shader);

      GPU_batch_draw(cursor_batch);

      GPU_blend(GPU_BLEND_NONE);
      GPU_line_smooth(false);
      GPU_matrix_pop();
      GPU_matrix_projection_set(original_proj);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name 2D Cursor
 * \{ */

static bool is_cursor_visible_2d(const DRWContextState *draw_ctx)
{
  SpaceInfo *space_data = (SpaceInfo *)draw_ctx->space_data;
  if (space_data == nullptr) {
    return false;
  }
  if (space_data->spacetype != SPACE_IMAGE) {
    return false;
  }
  SpaceImage *sima = (SpaceImage *)space_data;
  switch (sima->mode) {
    case SI_MODE_VIEW:
      return false;
      break;
    case SI_MODE_PAINT:
      return false;
      break;
    case SI_MODE_MASK:
      break;
    case SI_MODE_UV:
      break;
  }
  return (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) != 0;
}

/* -------------------------------------------------------------------- */
/** \name Generic Cursor
 * \{ */

void DRW_draw_cursor_2d_ex(const ARegion *region, const float cursor[2])
{
  int co[2];
  UI_view2d_view_to_region(&region->v2d, cursor[0], cursor[1], &co[0], &co[1]);

  /* Draw nice Anti Aliased cursor. */
  GPU_line_width(1.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  /* Draw lines */
  float original_proj[4][4];
  GPU_matrix_projection_get(original_proj);
  GPU_matrix_push();
  ED_region_pixelspace(region);
  GPU_matrix_translate_2f(co[0] + 0.5f, co[1] + 0.5f);
  GPU_matrix_scale_2f(U.widget_unit, U.widget_unit);

  GPUBatch *cursor_batch = DRW_cache_cursor_get(true);

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_FLAT_COLOR);
  GPU_batch_set_shader(cursor_batch, shader);

  GPU_batch_draw(cursor_batch);

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
  GPU_matrix_pop();
  GPU_matrix_projection_set(original_proj);
}

/** \} */

void DRW_draw_cursor_2d()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  GPU_color_mask(true, true, true, true);
  GPU_depth_mask(false);
  GPU_depth_test(GPU_DEPTH_NONE);

  if (is_cursor_visible_2d(draw_ctx)) {
    const SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;
    DRW_draw_cursor_2d_ex(region, sima->cursor);
  }
}

/** \} */

/* **************************** 3D Gizmo ******************************** */

void DRW_draw_gizmo_3d()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  /* draw depth culled gizmos - gizmos need to be updated *after* view matrix was set up */
  /* TODO: depth culling gizmos is not yet supported, just drawing _3D here, should
   * later become _IN_SCENE (and draw _3D separate) */
  WM_gizmomap_draw(region->gizmo_map, draw_ctx->evil_C, WM_GIZMOMAP_DRAWSTEP_3D);
}

void DRW_draw_gizmo_2d()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  WM_gizmomap_draw(region->gizmo_map, draw_ctx->evil_C, WM_GIZMOMAP_DRAWSTEP_2D);

  GPU_depth_mask(true);
}
