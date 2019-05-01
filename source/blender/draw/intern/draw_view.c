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
 * Copyright 2016, Blender Foundation.
 */

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
#include "ED_view3d.h"

#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "UI_resources.h"

#include "WM_types.h"

#include "BKE_object.h"
#include "BKE_paint.h"

#include "DRW_render.h"

#include "view3d_intern.h"

#include "draw_view.h"

/* ******************** region info ***************** */

void DRW_draw_region_info(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *ar = draw_ctx->ar;

  DRW_draw_cursor();

  view3d_draw_region_info(draw_ctx->evil_C, ar);
}

/* ************************* Background ************************** */

void DRW_draw_background(bool do_alpha_checker)
{
  /* Just to make sure */
  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilMask(0xFF);

  if (do_alpha_checker) {
    /* Transparent render, do alpha checker. */
    GPU_depth_test(false);

    GPU_matrix_push();
    GPU_matrix_identity_set();
    GPU_matrix_identity_projection_set();

    imm_draw_box_checker_2d(-1.0f, -1.0f, 1.0f, 1.0f);

    GPU_matrix_pop();

    GPU_clear(GPU_DEPTH_BIT | GPU_STENCIL_BIT);

    GPU_depth_test(true);
  }
  else if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
    float m[4][4];
    unit_m4(m);

    /* Gradient background Color */
    GPU_depth_test(false);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint color = GPU_vertformat_attr_add(
        format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    uchar col_hi[3], col_lo[3];

    GPU_matrix_push();
    GPU_matrix_identity_set();
    GPU_matrix_projection_set(m);

    immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR_DITHER);

    UI_GetThemeColor3ubv(TH_BACK_GRAD, col_lo);
    UI_GetThemeColor3ubv(TH_BACK, col_hi);

    immBegin(GPU_PRIM_TRI_FAN, 4);
    immAttr3ubv(color, col_lo);
    immVertex2f(pos, -1.0f, -1.0f);
    immVertex2f(pos, 1.0f, -1.0f);

    immAttr3ubv(color, col_hi);
    immVertex2f(pos, 1.0f, 1.0f);
    immVertex2f(pos, -1.0f, 1.0f);
    immEnd();

    immUnbindProgram();

    GPU_matrix_pop();

    GPU_clear(GPU_DEPTH_BIT | GPU_STENCIL_BIT);

    GPU_depth_test(true);
  }
  else {
    /* Solid background Color */
    UI_ThemeClearColorAlpha(TH_BACK, 1.0f);

    GPU_clear(GPU_COLOR_BIT | GPU_DEPTH_BIT | GPU_STENCIL_BIT);
  }
}

GPUBatch *DRW_draw_background_clipping_batch_from_rv3d(const RegionView3D *rv3d)
{
  const BoundBox *bb = rv3d->clipbb;
  const uint clipping_index[6][4] = {
      {0, 1, 2, 3},
      {0, 4, 5, 1},
      {4, 7, 6, 5},
      {7, 3, 2, 6},
      {1, 5, 6, 2},
      {7, 4, 0, 3},
  };
  GPUVertBuf *vbo;
  GPUIndexBuf *el;
  GPUIndexBufBuilder elb = {0};

  /* Elements */
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, ARRAY_SIZE(clipping_index) * 2, ARRAY_SIZE(bb->vec));
  for (int i = 0; i < ARRAY_SIZE(clipping_index); i++) {
    const uint *idx = clipping_index[i];
    GPU_indexbuf_add_tri_verts(&elb, idx[0], idx[1], idx[2]);
    GPU_indexbuf_add_tri_verts(&elb, idx[0], idx[2], idx[3]);
  }
  el = GPU_indexbuf_build(&elb);

  GPUVertFormat format = {0};
  uint pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, ARRAY_SIZE(bb->vec));
  GPU_vertbuf_attr_fill(vbo, pos_id, bb->vec);

  return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, el, GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

/* **************************** 3D Cursor ******************************** */

static bool is_cursor_visible(const DRWContextState *draw_ctx, Scene *scene, ViewLayer *view_layer)
{
  View3D *v3d = draw_ctx->v3d;
  if ((v3d->flag2 & V3D_HIDE_OVERLAYS) || (v3d->overlay.flag & V3D_OVERLAY_HIDE_CURSOR)) {
    return false;
  }

  /* don't draw cursor in paint modes, but with a few exceptions */
  if (draw_ctx->object_mode & OB_MODE_ALL_PAINT) {
    /* exception: object is in weight paint and has deforming armature in pose mode */
    if (draw_ctx->object_mode & OB_MODE_WEIGHT_PAINT) {
      if (BKE_object_pose_armature_get(draw_ctx->obact) != NULL) {
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
  else if (draw_ctx->object_mode & OB_MODE_WEIGHT_GPENCIL) {
    /* grease pencil hide always in some modes */
    return false;
  }

  return true;
}

void DRW_draw_cursor(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *ar = draw_ctx->ar;
  Scene *scene = draw_ctx->scene;
  ViewLayer *view_layer = draw_ctx->view_layer;

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);

  if (is_cursor_visible(draw_ctx, scene, view_layer)) {
    int co[2];

    /* Get cursor data into quaternion form */
    const View3DCursor *cursor = &scene->cursor;

    if (ED_view3d_project_int_global(
            ar, cursor->location, co, V3D_PROJ_TEST_NOP | V3D_PROJ_TEST_CLIP_NEAR) ==
        V3D_PROJ_RET_OK) {
      RegionView3D *rv3d = ar->regiondata;

      float cursor_quat[4];
      BKE_scene_cursor_rot_to_quat(cursor, cursor_quat);

      /* Draw nice Anti Aliased cursor. */
      GPU_line_width(1.0f);
      GPU_blend(true);
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
      ED_region_pixelspace(ar);
      GPU_matrix_translate_2f(co[0] + 0.5f, co[1] + 0.5f);
      GPU_matrix_scale_2f(U.widget_unit, U.widget_unit);

      GPUBatch *cursor_batch = DRW_cache_cursor_get(is_aligned);
      GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_FLAT_COLOR);
      GPU_batch_program_set(
          cursor_batch, GPU_shader_get_program(shader), GPU_shader_get_interface(shader));

      GPU_batch_draw(cursor_batch);

      GPU_blend(false);
      GPU_line_smooth(false);
      GPU_matrix_pop();
      GPU_matrix_projection_set(original_proj);
    }
  }
}

/* **************************** 3D Gizmo ******************************** */

void DRW_draw_gizmo_3d(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *ar = draw_ctx->ar;

  /* draw depth culled gizmos - gizmos need to be updated *after* view matrix was set up */
  /* TODO depth culling gizmos is not yet supported, just drawing _3D here, should
   * later become _IN_SCENE (and draw _3D separate) */
  WM_gizmomap_draw(ar->gizmo_map, draw_ctx->evil_C, WM_GIZMOMAP_DRAWSTEP_3D);
}

void DRW_draw_gizmo_2d(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *ar = draw_ctx->ar;

  WM_gizmomap_draw(ar->gizmo_map, draw_ctx->evil_C, WM_GIZMOMAP_DRAWSTEP_2D);

  glDepthMask(GL_TRUE);
}
