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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spview3d
 */

#include <math.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_jitter_2d.h"

#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_unit.h"

#include "BLF_api.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "DRW_engine.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_draw.h"
#include "GPU_matrix.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_material.h"
#include "GPU_viewport.h"
#include "GPU_state.h"
#include "GPU_framebuffer.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "view3d_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name General Functions
 * \{ */

/**
 * \note keep this synced with #ED_view3d_mats_rv3d_backup/#ED_view3d_mats_rv3d_restore
 */
void ED_view3d_update_viewmat(Depsgraph *depsgraph,
                              Scene *scene,
                              View3D *v3d,
                              ARegion *ar,
                              float viewmat[4][4],
                              float winmat[4][4],
                              const rcti *rect)
{
  RegionView3D *rv3d = ar->regiondata;

  /* setup window matrices */
  if (winmat) {
    copy_m4_m4(rv3d->winmat, winmat);
  }
  else {
    view3d_winmatrix_set(depsgraph, ar, v3d, rect);
  }

  /* setup view matrix */
  if (viewmat) {
    copy_m4_m4(rv3d->viewmat, viewmat);
  }
  else {
    float rect_scale[2];
    if (rect) {
      rect_scale[0] = (float)BLI_rcti_size_x(rect) / (float)ar->winx;
      rect_scale[1] = (float)BLI_rcti_size_y(rect) / (float)ar->winy;
    }
    /* note: calls BKE_object_where_is_calc for camera... */
    view3d_viewmatrix_set(depsgraph, scene, v3d, rv3d, rect ? rect_scale : NULL);
  }
  /* update utility matrices */
  mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
  invert_m4_m4(rv3d->persinv, rv3d->persmat);
  invert_m4_m4(rv3d->viewinv, rv3d->viewmat);

  /* calculate GLSL view dependent values */

  /* store window coordinates scaling/offset */
  if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
    rctf cameraborder;
    ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &cameraborder, false);
    rv3d->viewcamtexcofac[0] = (float)ar->winx / BLI_rctf_size_x(&cameraborder);
    rv3d->viewcamtexcofac[1] = (float)ar->winy / BLI_rctf_size_y(&cameraborder);

    rv3d->viewcamtexcofac[2] = -rv3d->viewcamtexcofac[0] * cameraborder.xmin / (float)ar->winx;
    rv3d->viewcamtexcofac[3] = -rv3d->viewcamtexcofac[1] * cameraborder.ymin / (float)ar->winy;
  }
  else {
    rv3d->viewcamtexcofac[0] = rv3d->viewcamtexcofac[1] = 1.0f;
    rv3d->viewcamtexcofac[2] = rv3d->viewcamtexcofac[3] = 0.0f;
  }

  /* calculate pixelsize factor once, is used for lights and obcenters */
  {
    /* note:  '1.0f / len_v3(v1)'  replaced  'len_v3(rv3d->viewmat[0])'
     * because of float point precision problems at large values [#23908] */
    float v1[3], v2[3];
    float len_px, len_sc;

    v1[0] = rv3d->persmat[0][0];
    v1[1] = rv3d->persmat[1][0];
    v1[2] = rv3d->persmat[2][0];

    v2[0] = rv3d->persmat[0][1];
    v2[1] = rv3d->persmat[1][1];
    v2[2] = rv3d->persmat[2][1];

    len_px = 2.0f / sqrtf(min_ff(len_squared_v3(v1), len_squared_v3(v2)));
    len_sc = (float)MAX2(ar->winx, ar->winy);

    rv3d->pixsize = len_px / len_sc;
  }
}

static void view3d_main_region_setup_view(Depsgraph *depsgraph,
                                          Scene *scene,
                                          View3D *v3d,
                                          ARegion *ar,
                                          float viewmat[4][4],
                                          float winmat[4][4],
                                          const rcti *rect)
{
  RegionView3D *rv3d = ar->regiondata;

  ED_view3d_update_viewmat(depsgraph, scene, v3d, ar, viewmat, winmat, rect);

  /* set for opengl */
  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);
}

static bool view3d_stereo3d_active(wmWindow *win, Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if ((v3d->camera == NULL) || (v3d->camera->type != OB_CAMERA) || rv3d->persp != RV3D_CAMOB) {
    return false;
  }

  switch (v3d->stereo3d_camera) {
    case STEREO_MONO_ID:
      return false;
      break;
    case STEREO_3D_ID:
      /* win will be NULL when calling this from the selection or draw loop. */
      if ((win == NULL) || (WM_stereo3d_enabled(win, true) == false)) {
        return false;
      }
      if (((scene->r.views_format & SCE_VIEWS_FORMAT_MULTIVIEW) != 0) &&
          !BKE_scene_multiview_is_stereo3d(&scene->r)) {
        return false;
      }
      break;
    /* We always need the stereo calculation for left and right cameras. */
    case STEREO_LEFT_ID:
    case STEREO_RIGHT_ID:
    default:
      break;
  }
  return true;
}

/* setup the view and win matrices for the multiview cameras
 *
 * unlike view3d_stereo3d_setup_offscreen, when view3d_stereo3d_setup is called
 * we have no winmatrix (i.e., projection matrix) defined at that time.
 * Since the camera and the camera shift are needed for the winmat calculation
 * we do a small hack to replace it temporarily so we don't need to change the
 * view3d)main_region_setup_view() code to account for that.
 */
static void view3d_stereo3d_setup(
    Depsgraph *depsgraph, Scene *scene, View3D *v3d, ARegion *ar, const rcti *rect)
{
  bool is_left;
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
  const char *viewname;

  /* show only left or right camera */
  if (v3d->stereo3d_camera != STEREO_3D_ID) {
    v3d->multiview_eye = v3d->stereo3d_camera;
  }

  is_left = v3d->multiview_eye == STEREO_LEFT_ID;
  viewname = names[is_left ? STEREO_LEFT_ID : STEREO_RIGHT_ID];

  /* update the viewport matrices with the new camera */
  if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
    Camera *data, *data_eval;
    float viewmat[4][4];
    float shiftx;

    data = (Camera *)v3d->camera->data;
    data_eval = (Camera *)DEG_get_evaluated_id(depsgraph, &data->id);

    shiftx = data_eval->shiftx;

    BLI_thread_lock(LOCK_VIEW3D);
    data_eval->shiftx = BKE_camera_multiview_shift_x(&scene->r, v3d->camera, viewname);

    BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
    view3d_main_region_setup_view(depsgraph, scene, v3d, ar, viewmat, NULL, rect);

    data_eval->shiftx = shiftx;
    BLI_thread_unlock(LOCK_VIEW3D);
  }
  else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
    float viewmat[4][4];
    Object *view_ob = v3d->camera;
    Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

    BLI_thread_lock(LOCK_VIEW3D);
    v3d->camera = camera;

    BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
    view3d_main_region_setup_view(depsgraph, scene, v3d, ar, viewmat, NULL, rect);

    v3d->camera = view_ob;
    BLI_thread_unlock(LOCK_VIEW3D);
  }
}

/**
 * Set the correct matrices
 */
void ED_view3d_draw_setup_view(wmWindow *win,
                               Depsgraph *depsgraph,
                               Scene *scene,
                               ARegion *ar,
                               View3D *v3d,
                               float viewmat[4][4],
                               float winmat[4][4],
                               const rcti *rect)
{
  RegionView3D *rv3d = ar->regiondata;

  /* Setup the view matrix. */
  if (view3d_stereo3d_active(win, scene, v3d, rv3d)) {
    view3d_stereo3d_setup(depsgraph, scene, v3d, ar, rect);
  }
  else {
    view3d_main_region_setup_view(depsgraph, scene, v3d, ar, viewmat, winmat, rect);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View Border
 * \{ */

static void view3d_camera_border(const Scene *scene,
                                 struct Depsgraph *depsgraph,
                                 const ARegion *ar,
                                 const View3D *v3d,
                                 const RegionView3D *rv3d,
                                 rctf *r_viewborder,
                                 const bool no_shift,
                                 const bool no_zoom)
{
  CameraParams params;
  rctf rect_view, rect_camera;
  Object *camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);

  /* get viewport viewplane */
  BKE_camera_params_init(&params);
  BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);
  if (no_zoom) {
    params.zoom = 1.0f;
  }
  BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
  rect_view = params.viewplane;

  /* get camera viewplane */
  BKE_camera_params_init(&params);
  /* fallback for non camera objects */
  params.clip_start = v3d->clip_start;
  params.clip_end = v3d->clip_end;
  BKE_camera_params_from_object(&params, camera_eval);
  if (no_shift) {
    params.shiftx = 0.0f;
    params.shifty = 0.0f;
  }
  BKE_camera_params_compute_viewplane(
      &params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
  rect_camera = params.viewplane;

  /* get camera border within viewport */
  r_viewborder->xmin = ((rect_camera.xmin - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) *
                       ar->winx;
  r_viewborder->xmax = ((rect_camera.xmax - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) *
                       ar->winx;
  r_viewborder->ymin = ((rect_camera.ymin - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) *
                       ar->winy;
  r_viewborder->ymax = ((rect_camera.ymax - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) *
                       ar->winy;
}

void ED_view3d_calc_camera_border_size(const Scene *scene,
                                       Depsgraph *depsgraph,
                                       const ARegion *ar,
                                       const View3D *v3d,
                                       const RegionView3D *rv3d,
                                       float r_size[2])
{
  rctf viewborder;

  view3d_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewborder, true, true);
  r_size[0] = BLI_rctf_size_x(&viewborder);
  r_size[1] = BLI_rctf_size_y(&viewborder);
}

void ED_view3d_calc_camera_border(const Scene *scene,
                                  Depsgraph *depsgraph,
                                  const ARegion *ar,
                                  const View3D *v3d,
                                  const RegionView3D *rv3d,
                                  rctf *r_viewborder,
                                  const bool no_shift)
{
  view3d_camera_border(scene, depsgraph, ar, v3d, rv3d, r_viewborder, no_shift, false);
}

static void drawviewborder_grid3(uint shdr_pos, float x1, float x2, float y1, float y2, float fac)
{
  float x3, y3, x4, y4;

  x3 = x1 + fac * (x2 - x1);
  y3 = y1 + fac * (y2 - y1);
  x4 = x1 + (1.0f - fac) * (x2 - x1);
  y4 = y1 + (1.0f - fac) * (y2 - y1);

  immBegin(GPU_PRIM_LINES, 8);

  immVertex2f(shdr_pos, x1, y3);
  immVertex2f(shdr_pos, x2, y3);

  immVertex2f(shdr_pos, x1, y4);
  immVertex2f(shdr_pos, x2, y4);

  immVertex2f(shdr_pos, x3, y1);
  immVertex2f(shdr_pos, x3, y2);

  immVertex2f(shdr_pos, x4, y1);
  immVertex2f(shdr_pos, x4, y2);

  immEnd();
}

/* harmonious triangle */
static void drawviewborder_triangle(
    uint shdr_pos, float x1, float x2, float y1, float y2, const char golden, const char dir)
{
  float ofs;
  float w = x2 - x1;
  float h = y2 - y1;

  immBegin(GPU_PRIM_LINES, 6);

  if (w > h) {
    if (golden) {
      ofs = w * (1.0f - (1.0f / 1.61803399f));
    }
    else {
      ofs = h * (h / w);
    }
    if (dir == 'B') {
      SWAP(float, y1, y2);
    }

    immVertex2f(shdr_pos, x1, y1);
    immVertex2f(shdr_pos, x2, y2);

    immVertex2f(shdr_pos, x2, y1);
    immVertex2f(shdr_pos, x1 + (w - ofs), y2);

    immVertex2f(shdr_pos, x1, y2);
    immVertex2f(shdr_pos, x1 + ofs, y1);
  }
  else {
    if (golden) {
      ofs = h * (1.0f - (1.0f / 1.61803399f));
    }
    else {
      ofs = w * (w / h);
    }
    if (dir == 'B') {
      SWAP(float, x1, x2);
    }

    immVertex2f(shdr_pos, x1, y1);
    immVertex2f(shdr_pos, x2, y2);

    immVertex2f(shdr_pos, x2, y1);
    immVertex2f(shdr_pos, x1, y1 + ofs);

    immVertex2f(shdr_pos, x1, y2);
    immVertex2f(shdr_pos, x2, y1 + (h - ofs));
  }

  immEnd();
}

static void drawviewborder(Scene *scene, Depsgraph *depsgraph, ARegion *ar, View3D *v3d)
{
  float x1, x2, y1, y2;
  float x1i, x2i, y1i, y2i;

  rctf viewborder;
  Camera *ca = NULL;
  RegionView3D *rv3d = ar->regiondata;

  if (v3d->camera == NULL) {
    return;
  }
  if (v3d->camera->type == OB_CAMERA) {
    ca = v3d->camera->data;
  }

  ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewborder, false);
  /* the offsets */
  x1 = viewborder.xmin;
  y1 = viewborder.ymin;
  x2 = viewborder.xmax;
  y2 = viewborder.ymax;

  GPU_line_width(1.0f);

  /* apply offsets so the real 3D camera shows through */

  /* note: quite un-scientific but without this bit extra
   * 0.0001 on the lower left the 2D border sometimes
   * obscures the 3D camera border */
  /* note: with VIEW3D_CAMERA_BORDER_HACK defined this error isn't noticeable
   * but keep it here in case we need to remove the workaround */
  x1i = (int)(x1 - 1.0001f);
  y1i = (int)(y1 - 1.0001f);
  x2i = (int)(x2 + (1.0f - 0.0001f));
  y2i = (int)(y2 + (1.0f - 0.0001f));

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* First, solid lines. */
  {
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    /* passepartout, specified in camera edit buttons */
    if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001f) {
      const float winx = (ar->winx + 1);
      const float winy = (ar->winy + 1);

      float alpha = 1.0f;

      if (ca->passepartalpha != 1.0f) {
        GPU_blend_set_func_separate(
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
        GPU_blend(true);
        alpha = ca->passepartalpha;
      }

      immUniformColor4f(0.0f, 0.0f, 0.0f, alpha);

      if (x1i > 0.0f) {
        immRectf(shdr_pos, 0.0f, winy, x1i, 0.0f);
      }
      if (x2i < winx) {
        immRectf(shdr_pos, x2i, winy, winx, 0.0f);
      }
      if (y2i < winy) {
        immRectf(shdr_pos, x1i, winy, x2i, y2i);
      }
      if (y2i > 0.0f) {
        immRectf(shdr_pos, x1i, y1i, x2i, 0.0f);
      }

      GPU_blend(false);
    }

    immUniformThemeColor(TH_BACK);
    imm_draw_box_wire_2d(shdr_pos, x1i, y1i, x2i, y2i);

#ifdef VIEW3D_CAMERA_BORDER_HACK
    if (view3d_camera_border_hack_test == true) {
      immUniformColor3ubv(view3d_camera_border_hack_col);
      imm_draw_box_wire_2d(shdr_pos, x1i + 1, y1i + 1, x2i - 1, y2i - 1);
      view3d_camera_border_hack_test = false;
    }
#endif

    immUnbindProgram();
  }

  /* When overlays are disabled, only show camera outline & passepartout. */
  if (v3d->flag2 & V3D_HIDE_OVERLAYS) {
    return;
  }

  /* And now, the dashed lines! */
  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  {
    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniform1f("dash_width", 6.0f);
    immUniform1f("dash_factor", 0.5f);

    /* outer line not to confuse with object selection */
    if (v3d->flag2 & V3D_LOCK_CAMERA) {
      immUniformThemeColor(TH_REDALERT);
      imm_draw_box_wire_2d(shdr_pos, x1i - 1, y1i - 1, x2i + 1, y2i + 1);
    }

    immUniformThemeColor(TH_VIEW_OVERLAY);
    imm_draw_box_wire_2d(shdr_pos, x1i, y1i, x2i, y2i);
  }

  /* Render Border. */
  if (scene->r.mode & R_BORDER) {
    float x3, y3, x4, y4;

    x3 = floorf(x1 + (scene->r.border.xmin * (x2 - x1))) - 1;
    y3 = floorf(y1 + (scene->r.border.ymin * (y2 - y1))) - 1;
    x4 = floorf(x1 + (scene->r.border.xmax * (x2 - x1))) + (U.pixelsize - 1);
    y4 = floorf(y1 + (scene->r.border.ymax * (y2 - y1))) + (U.pixelsize - 1);

    immUniformColor3f(1.0f, 0.25f, 0.25f);
    imm_draw_box_wire_2d(shdr_pos, x3, y3, x4, y4);
  }

  /* safety border */
  if (ca) {
    immUniformThemeColorBlend(TH_VIEW_OVERLAY, TH_BACK, 0.25f);

    if (ca->dtx & CAM_DTX_CENTER) {
      float x3, y3;

      x3 = x1 + 0.5f * (x2 - x1);
      y3 = y1 + 0.5f * (y2 - y1);

      immBegin(GPU_PRIM_LINES, 4);

      immVertex2f(shdr_pos, x1, y3);
      immVertex2f(shdr_pos, x2, y3);

      immVertex2f(shdr_pos, x3, y1);
      immVertex2f(shdr_pos, x3, y2);

      immEnd();
    }

    if (ca->dtx & CAM_DTX_CENTER_DIAG) {
      immBegin(GPU_PRIM_LINES, 4);

      immVertex2f(shdr_pos, x1, y1);
      immVertex2f(shdr_pos, x2, y2);

      immVertex2f(shdr_pos, x1, y2);
      immVertex2f(shdr_pos, x2, y1);

      immEnd();
    }

    if (ca->dtx & CAM_DTX_THIRDS) {
      drawviewborder_grid3(shdr_pos, x1, x2, y1, y2, 1.0f / 3.0f);
    }

    if (ca->dtx & CAM_DTX_GOLDEN) {
      drawviewborder_grid3(shdr_pos, x1, x2, y1, y2, 1.0f - (1.0f / 1.61803399f));
    }

    if (ca->dtx & CAM_DTX_GOLDEN_TRI_A) {
      drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 0, 'A');
    }

    if (ca->dtx & CAM_DTX_GOLDEN_TRI_B) {
      drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 0, 'B');
    }

    if (ca->dtx & CAM_DTX_HARMONY_TRI_A) {
      drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 1, 'A');
    }

    if (ca->dtx & CAM_DTX_HARMONY_TRI_B) {
      drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 1, 'B');
    }

    if (ca->flag & CAM_SHOW_SAFE_MARGINS) {
      UI_draw_safe_areas(
          shdr_pos, x1, x2, y1, y2, scene->safe_areas.title, scene->safe_areas.action);

      if (ca->flag & CAM_SHOW_SAFE_CENTER) {
        UI_draw_safe_areas(shdr_pos,
                           x1,
                           x2,
                           y1,
                           y2,
                           scene->safe_areas.title_center,
                           scene->safe_areas.action_center);
      }
    }

    if (ca->flag & CAM_SHOWSENSOR) {
      /* determine sensor fit, and get sensor x/y, for auto fit we
       * assume and square sensor and only use sensor_x */
      float sizex = scene->r.xsch * scene->r.xasp;
      float sizey = scene->r.ysch * scene->r.yasp;
      int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, sizex, sizey);
      float sensor_x = ca->sensor_x;
      float sensor_y = (ca->sensor_fit == CAMERA_SENSOR_FIT_AUTO) ? ca->sensor_x : ca->sensor_y;

      /* determine sensor plane */
      rctf rect;

      if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
        float sensor_scale = (x2i - x1i) / sensor_x;
        float sensor_height = sensor_scale * sensor_y;

        rect.xmin = x1i;
        rect.xmax = x2i;
        rect.ymin = (y1i + y2i) * 0.5f - sensor_height * 0.5f;
        rect.ymax = rect.ymin + sensor_height;
      }
      else {
        float sensor_scale = (y2i - y1i) / sensor_y;
        float sensor_width = sensor_scale * sensor_x;

        rect.xmin = (x1i + x2i) * 0.5f - sensor_width * 0.5f;
        rect.xmax = rect.xmin + sensor_width;
        rect.ymin = y1i;
        rect.ymax = y2i;
      }

      /* draw */
      immUniformThemeColorShade(TH_VIEW_OVERLAY, 100);

      /* TODO Was using:
       * UI_draw_roundbox_4fv(false, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 2.0f, color);
       * We'll probably need a new imm_draw_line_roundbox_dashed dor that - though in practice the
       * 2.0f round corner effect was nearly not visible anyway... */
      imm_draw_box_wire_2d(shdr_pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    }
  }

  immUnbindProgram();
  /* end dashed lines */

  /* camera name - draw in highlighted text color */
  if (ca && ((v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) == 0) && (ca->flag & CAM_SHOWNAME)) {
    UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
    BLF_draw_default(x1i,
                     y1i - (0.7f * U.widget_unit),
                     0.0f,
                     v3d->camera->id.name + 2,
                     sizeof(v3d->camera->id.name) - 2);
  }
}

static void drawrenderborder(ARegion *ar, View3D *v3d)
{
  /* use the same program for everything */
  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform4f("color", 1.0f, 0.25f, 0.25f, 1.0f);
  immUniform1f("dash_width", 6.0f);
  immUniform1f("dash_factor", 0.5f);

  imm_draw_box_wire_2d(shdr_pos,
                       v3d->render_border.xmin * ar->winx,
                       v3d->render_border.ymin * ar->winy,
                       v3d->render_border.xmax * ar->winx,
                       v3d->render_border.ymax * ar->winy);

  immUnbindProgram();
}

void ED_view3d_draw_depth(Depsgraph *depsgraph, ARegion *ar, View3D *v3d, bool alphaoverride)
{
  struct bThemeState theme_state;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RegionView3D *rv3d = ar->regiondata;

  short flag = v3d->flag;
  float glalphaclip = U.glalphaclip;
  /* temp set drawtype to solid */
  /* Setting these temporarily is not nice */
  v3d->flag &= ~V3D_SELECT_OUTLINE;

  /* not that nice but means we wont zoom into billboards */
  U.glalphaclip = alphaoverride ? 0.5f : glalphaclip;

  /* Tools may request depth outside of regular drawing code. */
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  ED_view3d_draw_setup_view(NULL, depsgraph, scene, ar, v3d, NULL, NULL, NULL);

  GPU_clear(GPU_DEPTH_BIT);

  if (rv3d->rflag & RV3D_CLIPPING) {
    ED_view3d_clipping_set(rv3d);
  }
  /* get surface depth without bias */
  rv3d->rflag |= RV3D_ZOFFSET_DISABLED;

  GPU_depth_test(true);

  GPUViewport *viewport = WM_draw_region_get_viewport(ar, 0);
  DRW_draw_depth_loop(depsgraph, ar, v3d, viewport);

  if (rv3d->rflag & RV3D_CLIPPING) {
    ED_view3d_clipping_disable();
  }
  rv3d->rflag &= ~RV3D_ZOFFSET_DISABLED;

  /* Reset default for UI */
  GPU_depth_test(false);

  U.glalphaclip = glalphaclip;
  v3d->flag = flag;

  UI_Theme_Restore(&theme_state);
}

/* ******************** other elements ***************** */

/** could move this elsewhere, but tied into #ED_view3d_grid_scale */
float ED_scene_grid_scale(Scene *scene, const char **grid_unit)
{
  /* apply units */
  if (scene->unit.system) {
    const void *usys;
    int len;

    bUnit_GetSystem(scene->unit.system, B_UNIT_LENGTH, &usys, &len);

    if (usys) {
      int i = bUnit_GetBaseUnit(usys);
      if (grid_unit) {
        *grid_unit = bUnit_GetNameDisplay(usys, i);
      }
      return (float)bUnit_GetScaler(usys, i) / scene->unit.scale_length;
    }
  }

  return 1.0f;
}

float ED_view3d_grid_scale(Scene *scene, View3D *v3d, const char **grid_unit)
{
  return v3d->grid * ED_scene_grid_scale(scene, grid_unit);
}

/* Simulates the grid scale that is actually viewed.
 * The actual code is seen in `object_grid_frag.glsl` (see `grid_res`).
 * Currently the simulation is only done when RV3D_VIEW_IS_AXIS. */
float ED_view3d_grid_view_scale(Scene *scene,
                                View3D *v3d,
                                RegionView3D *rv3d,
                                const char **grid_unit)
{
  float grid_scale = ED_view3d_grid_scale(scene, v3d, grid_unit);
  if (!rv3d->is_persp && RV3D_VIEW_IS_AXIS(rv3d->view)) {
    /* Decrease the distance between grid snap points depending on zoom. */
    float grid_subdiv = v3d->gridsubdiv;
    if (grid_subdiv > 1) {
      /* Allow 3 more subdivisions (see OBJECT_engine_init). */
      grid_scale /= powf(grid_subdiv, 3);

      /* `3.0` was a value obtained by trial and error in order to get
       * a nice snap distance.*/
      float grid_res = 3.0 * (rv3d->dist / v3d->lens);
      float lvl = (logf(grid_res / grid_scale) / logf(grid_subdiv));

      CLAMP_MIN(lvl, 0.0f);

      grid_scale *= pow(grid_subdiv, (int)lvl);
    }
  }

  return grid_scale;
}

static void draw_view_axis(RegionView3D *rv3d, const rcti *rect)
{
  const float k = U.rvisize * U.pixelsize; /* axis size */
  /* axis alpha offset (rvibright has range 0-10) */
  const int bright = -20 * (10 - U.rvibright);

  /* Axis center in screen coordinates.
   *
   * - Unit size offset so small text doesn't draw outside the screen
   * - Extra X offset because of the panel expander.
   */
  const float startx = rect->xmax - (k + UI_UNIT_X * 1.5);
  const float starty = rect->ymax - (k + UI_UNIT_Y);

  float axis_pos[3][2];
  uchar axis_col[3][4];

  int axis_order[3] = {0, 1, 2};
  axis_sort_v3(rv3d->viewinv[2], axis_order);

  for (int axis_i = 0; axis_i < 3; axis_i++) {
    int i = axis_order[axis_i];

    /* get position of each axis tip on screen */
    float vec[3] = {0.0f};
    vec[i] = 1.0f;
    mul_qt_v3(rv3d->viewquat, vec);
    axis_pos[i][0] = startx + vec[0] * k;
    axis_pos[i][1] = starty + vec[1] * k;

    /* get color of each axis */
    UI_GetThemeColorShade3ubv(TH_AXIS_X + i, bright, axis_col[i]); /* rgb */
    axis_col[i][3] = 255 * hypotf(vec[0], vec[1]);                 /* alpha */
  }

  /* draw axis lines */
  GPU_line_width(2.0f);
  GPU_line_smooth(true);
  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

  immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
  immBegin(GPU_PRIM_LINES, 6);

  for (int axis_i = 0; axis_i < 3; axis_i++) {
    int i = axis_order[axis_i];

    immAttr4ubv(col, axis_col[i]);
    immVertex2f(pos, startx, starty);
    immAttr4ubv(col, axis_col[i]);
    immVertex2fv(pos, axis_pos[i]);
  }

  immEnd();
  immUnbindProgram();
  GPU_line_smooth(false);

  /* draw axis names */
  for (int axis_i = 0; axis_i < 3; axis_i++) {
    int i = axis_order[axis_i];

    const char axis_text[2] = {'x' + i, '\0'};
    BLF_color4ubv(BLF_default(), axis_col[i]);
    BLF_draw_default_ascii(axis_pos[i][0] + 2, axis_pos[i][1] + 2, 0.0f, axis_text, 1);
  }
}

#ifdef WITH_INPUT_NDOF
/* draw center and axis of rotation for ongoing 3D mouse navigation */
static void draw_rotation_guide(const RegionView3D *rv3d)
{
  float o[3];   /* center of rotation */
  float end[3]; /* endpoints for drawing */

  GLubyte color[4] = {0, 108, 255, 255}; /* bright blue so it matches device LEDs */

  negate_v3_v3(o, rv3d->ofs);

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE); /* don't overwrite zbuf */

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  if (rv3d->rot_angle != 0.0f) {
    /* -- draw rotation axis -- */
    float scaled_axis[3];
    const float scale = rv3d->dist;
    mul_v3_v3fl(scaled_axis, rv3d->rot_axis, scale);

    immBegin(GPU_PRIM_LINE_STRIP, 3);
    color[3] = 0; /* more transparent toward the ends */
    immAttr4ubv(col, color);
    add_v3_v3v3(end, o, scaled_axis);
    immVertex3fv(pos, end);

#  if 0
    color[3] = 0.2f + fabsf(rv3d->rot_angle); /* modulate opacity with angle */
    /* ^^ neat idea, but angle is frame-rate dependent, so it's usually close to 0.2 */
#  endif

    color[3] = 127; /* more opaque toward the center */
    immAttr4ubv(col, color);
    immVertex3fv(pos, o);

    color[3] = 0;
    immAttr4ubv(col, color);
    sub_v3_v3v3(end, o, scaled_axis);
    immVertex3fv(pos, end);
    immEnd();

    /* -- draw ring around rotation center -- */
    {
#  define ROT_AXIS_DETAIL 13

      const float s = 0.05f * scale;
      const float step = 2.0f * (float)(M_PI / ROT_AXIS_DETAIL);

      float q[4]; /* rotate ring so it's perpendicular to axis */
      const int upright = fabsf(rv3d->rot_axis[2]) >= 0.95f;
      if (!upright) {
        const float up[3] = {0.0f, 0.0f, 1.0f};
        float vis_angle, vis_axis[3];

        cross_v3_v3v3(vis_axis, up, rv3d->rot_axis);
        vis_angle = acosf(dot_v3v3(up, rv3d->rot_axis));
        axis_angle_to_quat(q, vis_axis, vis_angle);
      }

      immBegin(GPU_PRIM_LINE_LOOP, ROT_AXIS_DETAIL);
      color[3] = 63; /* somewhat faint */
      immAttr4ubv(col, color);
      float angle = 0.0f;
      for (int i = 0; i < ROT_AXIS_DETAIL; ++i, angle += step) {
        float p[3] = {s * cosf(angle), s * sinf(angle), 0.0f};

        if (!upright) {
          mul_qt_v3(q, p);
        }

        add_v3_v3(p, o);
        immVertex3fv(pos, p);
      }
      immEnd();

#  undef ROT_AXIS_DETAIL
    }

    color[3] = 255; /* solid dot */
  }
  else {
    color[3] = 127; /* see-through dot */
  }

  immUnbindProgram();

  /* -- draw rotation center -- */
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR);
  GPU_point_size(5.0f);
  immBegin(GPU_PRIM_POINTS, 1);
  immAttr4ubv(col, color);
  immVertex3fv(pos, o);
  immEnd();
  immUnbindProgram();

#  if 0
  /* find screen coordinates for rotation center, then draw pretty icon */
  mul_m4_v3(rv3d->persinv, rot_center);
  UI_icon_draw(rot_center[0], rot_center[1], ICON_NDOF_TURN);
  /* ^^ just playing around, does not work */
#  endif

  GPU_blend(false);
  glDepthMask(GL_TRUE);
}
#endif /* WITH_INPUT_NDOF */

/**
 * Render and camera border
 */
static void view3d_draw_border(const bContext *C, ARegion *ar)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  RegionView3D *rv3d = ar->regiondata;
  View3D *v3d = CTX_wm_view3d(C);

  if (rv3d->persp == RV3D_CAMOB) {
    drawviewborder(scene, depsgraph, ar, v3d);
  }
  else if (v3d->flag2 & V3D_RENDER_BORDER) {
    drawrenderborder(ar, v3d);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Text & Info
 * \{ */

/**
 * Draw Info
 */
static void view3d_draw_grease_pencil(const bContext *UNUSED(C))
{
  /* TODO viewport */
}

/**
 * Viewport Name
 */
static const char *view3d_get_name(View3D *v3d, RegionView3D *rv3d)
{
  const char *name = NULL;

  switch (rv3d->view) {
    case RV3D_VIEW_FRONT:
      if (rv3d->persp == RV3D_ORTHO) {
        name = IFACE_("Front Orthographic");
      }
      else {
        name = IFACE_("Front Perspective");
      }
      break;
    case RV3D_VIEW_BACK:
      if (rv3d->persp == RV3D_ORTHO) {
        name = IFACE_("Back Orthographic");
      }
      else {
        name = IFACE_("Back Perspective");
      }
      break;
    case RV3D_VIEW_TOP:
      if (rv3d->persp == RV3D_ORTHO) {
        name = IFACE_("Top Orthographic");
      }
      else {
        name = IFACE_("Top Perspective");
      }
      break;
    case RV3D_VIEW_BOTTOM:
      if (rv3d->persp == RV3D_ORTHO) {
        name = IFACE_("Bottom Orthographic");
      }
      else {
        name = IFACE_("Bottom Perspective");
      }
      break;
    case RV3D_VIEW_RIGHT:
      if (rv3d->persp == RV3D_ORTHO) {
        name = IFACE_("Right Orthographic");
      }
      else {
        name = IFACE_("Right Perspective");
      }
      break;
    case RV3D_VIEW_LEFT:
      if (rv3d->persp == RV3D_ORTHO) {
        name = IFACE_("Left Orthographic");
      }
      else {
        name = IFACE_("Left Perspective");
      }
      break;

    default:
      if (rv3d->persp == RV3D_CAMOB) {
        if ((v3d->camera) && (v3d->camera->type == OB_CAMERA)) {
          Camera *cam;
          cam = v3d->camera->data;
          if (cam->type == CAM_PERSP) {
            name = IFACE_("Camera Perspective");
          }
          else if (cam->type == CAM_ORTHO) {
            name = IFACE_("Camera Orthographic");
          }
          else {
            BLI_assert(cam->type == CAM_PANO);
            name = IFACE_("Camera Panoramic");
          }
        }
        else {
          name = IFACE_("Object as Camera");
        }
      }
      else {
        name = (rv3d->persp == RV3D_ORTHO) ? IFACE_("User Orthographic") :
                                             IFACE_("User Perspective");
      }
  }

  return name;
}

static void draw_viewport_name(ARegion *ar, View3D *v3d, int xoffset, int *yoffset)
{
  RegionView3D *rv3d = ar->regiondata;
  const char *name = view3d_get_name(v3d, rv3d);
  const int font_id = BLF_default();

  /* increase size for unicode languages (Chinese in utf-8...) */
#ifdef WITH_INTERNATIONAL
  char tmpstr[96];
#else
  char tmpstr[32];
#endif

  BLF_enable(font_id, BLF_SHADOW);
  BLF_shadow(font_id, 5, (const float[4]){0.0f, 0.0f, 0.0f, 1.0f});
  BLF_shadow_offset(font_id, 1, -1);

  if (v3d->localvd) {
    BLI_snprintf(tmpstr, sizeof(tmpstr), IFACE_("%s (Local)"), name);
    name = tmpstr;
  }

  UI_FontThemeColor(BLF_default(), TH_TEXT_HI);

  *yoffset -= U.widget_unit;

#ifdef WITH_INTERNATIONAL
  BLF_draw_default(xoffset, *yoffset, 0.0f, name, sizeof(tmpstr));
#else
  BLF_draw_default_ascii(xoffset, *yoffset, 0.0f, name, sizeof(tmpstr));
#endif

  BLF_disable(font_id, BLF_SHADOW);
}

/**
 * draw info beside axes in bottom left-corner:
 * framenum, collection, object name, bone name (if available), marker name (if available)
 */

static void draw_selected_name(
    Scene *scene, ViewLayer *view_layer, Object *ob, int xoffset, int *yoffset)
{
  const int cfra = CFRA;
  const char *msg_pin = " (Pinned)";
  const char *msg_sep = " : ";

  const int font_id = BLF_default();

  char info[300];
  char *s = info;

  s += sprintf(s, "(%d)", cfra);

  if ((ob == NULL) || (ob->mode == OB_MODE_OBJECT)) {
    LayerCollection *layer_collection = view_layer->active_collection;
    s += sprintf(s,
                 " %s%s",
                 BKE_collection_ui_name_get(layer_collection->collection),
                 (ob == NULL) ? "" : " |");
  }

  /*
   * info can contain:
   * - a frame (7 + 2)
   * - a collection name (MAX_NAME + 3)
   * - 3 object names (MAX_NAME)
   * - 2 BREAD_CRUMB_SEPARATORs (6)
   * - a SHAPE_KEY_PINNED marker and a trailing '\0' (9+1) - translated, so give some room!
   * - a marker name (MAX_NAME + 3)
   */

  /* get name of marker on current frame (if available) */
  const char *markern = BKE_scene_find_marker_name(scene, cfra);

  /* check if there is an object */
  if (ob) {
    *s++ = ' ';
    s += BLI_strcpy_rlen(s, ob->id.name + 2);

    /* name(s) to display depends on type of object */
    if (ob->type == OB_ARMATURE) {
      bArmature *arm = ob->data;

      /* show name of active bone too (if possible) */
      if (arm->edbo) {
        if (arm->act_edbone) {
          s += BLI_strcpy_rlen(s, msg_sep);
          s += BLI_strcpy_rlen(s, arm->act_edbone->name);
        }
      }
      else if (ob->mode & OB_MODE_POSE) {
        if (arm->act_bone) {

          if (arm->act_bone->layer & arm->layer) {
            s += BLI_strcpy_rlen(s, msg_sep);
            s += BLI_strcpy_rlen(s, arm->act_bone->name);
          }
        }
      }
    }
    else if (ELEM(ob->type, OB_MESH, OB_LATTICE, OB_CURVE)) {
      /* try to display active bone and active shapekey too (if they exist) */

      if (ob->type == OB_MESH && ob->mode & OB_MODE_WEIGHT_PAINT) {
        Object *armobj = BKE_object_pose_armature_get(ob);
        if (armobj && armobj->mode & OB_MODE_POSE) {
          bArmature *arm = armobj->data;
          if (arm->act_bone) {
            if (arm->act_bone->layer & arm->layer) {
              s += BLI_strcpy_rlen(s, msg_sep);
              s += BLI_strcpy_rlen(s, arm->act_bone->name);
            }
          }
        }
      }

      Key *key = BKE_key_from_object(ob);
      if (key) {
        KeyBlock *kb = BLI_findlink(&key->block, ob->shapenr - 1);
        if (kb) {
          s += BLI_strcpy_rlen(s, msg_sep);
          s += BLI_strcpy_rlen(s, kb->name);
          if (ob->shapeflag & OB_SHAPE_LOCK) {
            s += BLI_strcpy_rlen(s, IFACE_(msg_pin));
          }
        }
      }
    }

    /* color depends on whether there is a keyframe */
    if (id_frame_has_keyframe(
            (ID *)ob, /* BKE_scene_frame_get(scene) */ (float)cfra, ANIMFILTER_KEYS_LOCAL)) {
      UI_FontThemeColor(font_id, TH_TIME_KEYFRAME);
    }
    else if (ED_gpencil_has_keyframe_v3d(scene, ob, cfra)) {
      UI_FontThemeColor(font_id, TH_TIME_GP_KEYFRAME);
    }
    else {
      UI_FontThemeColor(font_id, TH_TEXT_HI);
    }
  }
  else {
    /* no object */
    if (ED_gpencil_has_keyframe_v3d(scene, NULL, cfra)) {
      UI_FontThemeColor(font_id, TH_TIME_GP_KEYFRAME);
    }
    else {
      UI_FontThemeColor(font_id, TH_TEXT_HI);
    }
  }

  if (markern) {
    s += sprintf(s, " <%s>", markern);
  }

  BLF_enable(font_id, BLF_SHADOW);
  BLF_shadow(font_id, 5, (const float[4]){0.0f, 0.0f, 0.0f, 1.0f});
  BLF_shadow_offset(font_id, 1, -1);

  *yoffset -= U.widget_unit;
  BLF_draw_default(xoffset, *yoffset, 0.0f, info, sizeof(info));

  BLF_disable(font_id, BLF_SHADOW);
}

/**
 * Information drawn on top of the solid plates and composed data
 */
void view3d_draw_region_info(const bContext *C, ARegion *ar)
{
  RegionView3D *rv3d = ar->regiondata;
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);

#ifdef WITH_INPUT_NDOF
  if ((U.ndof_flag & NDOF_SHOW_GUIDE) && ((rv3d->viewlock & RV3D_LOCKED) == 0) &&
      (rv3d->persp != RV3D_CAMOB)) {
    /* TODO: draw something else (but not this) during fly mode */
    draw_rotation_guide(rv3d);
  }
#endif

  /* correct projection matrix */
  ED_region_pixelspace(ar);

  /* local coordinate visible rect inside region, to accommodate overlapping ui */
  rcti rect;
  ED_region_visible_rect(ar, &rect);

  view3d_draw_border(C, ar);
  view3d_draw_grease_pencil(C);

  BLF_batch_draw_begin();

  if ((U.uiflag & USER_SHOW_GIZMO_AXIS) ||
      /* No need to display gizmo and this info. */
      (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_NAVIGATE))) {
    /* pass */
  }
  else {
    draw_view_axis(rv3d, &rect);
  }

  int xoffset = rect.xmin + U.widget_unit;
  int yoffset = rect.ymax;

  if ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0 && (v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) == 0) {
    if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(wm)) {
      ED_scene_draw_fps(scene, xoffset, &yoffset);
    }
    else if (U.uiflag & USER_SHOW_VIEWPORTNAME) {
      draw_viewport_name(ar, v3d, xoffset, &yoffset);
    }

    if (U.uiflag & USER_DRAWVIEWINFO) {
      ViewLayer *view_layer = CTX_data_view_layer(C);
      Object *ob = OBACT(view_layer);
      draw_selected_name(scene, view_layer, ob, xoffset, &yoffset);
    }

#if 0 /* TODO */
    if (grid_unit) { /* draw below the viewport name */
      char numstr[32] = "";

      UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
      if (v3d->grid != 1.0f) {
        BLI_snprintf(numstr, sizeof(numstr), "%s x %.4g", grid_unit, v3d->grid);
      }

      *yoffset -= U.widget_unit;
      BLF_draw_default_ascii(xoffset, *yoffset, numstr[0] ? numstr : grid_unit, sizeof(numstr));
    }
#endif
  }

  if ((v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) == 0) {
    DRW_draw_region_engine_info(xoffset, yoffset);
  }

  BLF_batch_draw_end();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Viewport Contents
 * \{ */

static void view3d_draw_view(const bContext *C, ARegion *ar)
{
  ED_view3d_draw_setup_view(CTX_wm_window(C),
                            CTX_data_depsgraph(C),
                            CTX_data_scene(C),
                            ar,
                            CTX_wm_view3d(C),
                            NULL,
                            NULL,
                            NULL);

  /* Only 100% compliant on new spec goes below */
  DRW_draw_view(C);
}

RenderEngineType *ED_view3d_engine_type(Scene *scene, int drawtype)
{
  /*
   * Temporary viewport draw modes until we have a proper system.
   * all modes are done in the draw manager, except
   * cycles material as it is an external render engine.
   */
  if (strcmp(scene->r.engine, RE_engine_id_CYCLES) == 0 && drawtype == OB_MATERIAL) {
    return RE_engines_find(RE_engine_id_BLENDER_EEVEE);
  }
  return RE_engines_find(scene->r.engine);
}

void view3d_main_region_draw(const bContext *C, ARegion *ar)
{
  Main *bmain = CTX_data_main(C);
  View3D *v3d = CTX_wm_view3d(C);

  view3d_draw_view(C, ar);

  DRW_cache_free_old_batches(bmain);
  GPU_free_images_old(bmain);
  GPU_pass_cache_garbage_collect();

  /* XXX This is in order to draw UI batches with the DRW
   * olg context since we now use it for drawing the entire area */
  gpu_batch_presets_reset();

  /* No depth test for drawing action zones afterwards. */
  GPU_depth_test(false);

  v3d->flag |= V3D_INVALID_BACKBUF;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Offscreen Drawing
 * \{ */

static void view3d_stereo3d_setup_offscreen(Depsgraph *depsgraph,
                                            Scene *scene,
                                            View3D *v3d,
                                            ARegion *ar,
                                            float winmat[4][4],
                                            const char *viewname)
{
  /* update the viewport matrices with the new camera */
  if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
    float viewmat[4][4];
    const bool is_left = STREQ(viewname, STEREO_LEFT_NAME);

    BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
    view3d_main_region_setup_view(depsgraph, scene, v3d, ar, viewmat, winmat, NULL);
  }
  else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
    float viewmat[4][4];
    Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

    BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
    view3d_main_region_setup_view(depsgraph, scene, v3d, ar, viewmat, winmat, NULL);
  }
}

void ED_view3d_draw_offscreen(Depsgraph *depsgraph,
                              Scene *scene,
                              int drawtype,
                              View3D *v3d,
                              ARegion *ar,
                              int winx,
                              int winy,
                              float viewmat[4][4],
                              float winmat[4][4],
                              bool do_sky,
                              bool UNUSED(is_persp),
                              const char *viewname,
                              GPUFXSettings *UNUSED(fx_settings),
                              const bool do_color_management,
                              GPUOffScreen *ofs,
                              GPUViewport *viewport)
{
  RegionView3D *rv3d = ar->regiondata;
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, drawtype);

  /* set temporary new size */
  int bwinx = ar->winx;
  int bwiny = ar->winy;
  rcti brect = ar->winrct;

  ar->winx = winx;
  ar->winy = winy;
  ar->winrct.xmin = 0;
  ar->winrct.ymin = 0;
  ar->winrct.xmax = winx;
  ar->winrct.ymax = winy;

  struct bThemeState theme_state;
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* set flags */
  G.f |= G_FLAG_RENDER_VIEWPORT;

  {
    /* free images which can have changed on frame-change
     * warning! can be slow so only free animated images - campbell */
    GPU_free_images_anim(G.main); /* XXX :((( */
  }

  GPU_matrix_push_projection();
  GPU_matrix_identity_set();
  GPU_matrix_push();
  GPU_matrix_identity_set();

  if ((viewname != NULL && viewname[0] != '\0') && (viewmat == NULL) &&
      rv3d->persp == RV3D_CAMOB && v3d->camera) {
    view3d_stereo3d_setup_offscreen(depsgraph, scene, v3d, ar, winmat, viewname);
  }
  else {
    view3d_main_region_setup_view(depsgraph, scene, v3d, ar, viewmat, winmat, NULL);
  }

  /* main drawing call */
  DRW_draw_render_loop_offscreen(
      depsgraph, engine_type, ar, v3d, do_sky, do_color_management, ofs, viewport);

  /* restore size */
  ar->winx = bwinx;
  ar->winy = bwiny;
  ar->winrct = brect;

  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  UI_Theme_Restore(&theme_state);

  G.f &= ~G_FLAG_RENDER_VIEWPORT;
}

/**
 * Utility func for ED_view3d_draw_offscreen
 *
 * \param ofs: Optional off-screen buffer, can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 */
ImBuf *ED_view3d_draw_offscreen_imbuf(Depsgraph *depsgraph,
                                      Scene *scene,
                                      int drawtype,
                                      View3D *v3d,
                                      ARegion *ar,
                                      int sizex,
                                      int sizey,
                                      uint flag,
                                      uint draw_flags,
                                      int alpha_mode,
                                      int samples,
                                      const char *viewname,
                                      /* output vars */
                                      GPUOffScreen *ofs,
                                      char err_out[256])
{
  RegionView3D *rv3d = ar->regiondata;
  const bool draw_sky = (alpha_mode == R_ADDSKY);
  const bool use_full_sample = (draw_flags & V3D_OFSDRAW_USE_FULL_SAMPLE);

  /* view state */
  GPUFXSettings fx_settings = v3d->fx_settings;
  bool is_ortho = false;
  float winmat[4][4];

  if (ofs && ((GPU_offscreen_width(ofs) != sizex) || (GPU_offscreen_height(ofs) != sizey))) {
    /* sizes differ, can't reuse */
    ofs = NULL;
  }

  GPUFrameBuffer *old_fb = GPU_framebuffer_active_get();

  if (old_fb) {
    GPU_framebuffer_restore();
  }

  const bool own_ofs = (ofs == NULL);
  DRW_opengl_context_enable();

  if (own_ofs) {
    /* bind */
    ofs = GPU_offscreen_create(sizex, sizey, use_full_sample ? 0 : samples, true, false, err_out);
    if (ofs == NULL) {
      DRW_opengl_context_disable();
      return NULL;
    }
  }

  GPU_offscreen_bind(ofs, true);

  /* read in pixels & stamp */
  ImBuf *ibuf = IMB_allocImBuf(sizex, sizey, 32, flag);

  /* render 3d view */
  if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
    CameraParams params;
    Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);
    const Object *camera_eval = DEG_get_evaluated_object(depsgraph, camera);

    BKE_camera_params_init(&params);
    /* fallback for non camera objects */
    params.clip_start = v3d->clip_start;
    params.clip_end = v3d->clip_end;
    BKE_camera_params_from_object(&params, camera_eval);
    BKE_camera_multiview_params(&scene->r, &params, camera_eval, viewname);
    BKE_camera_params_compute_viewplane(&params, sizex, sizey, scene->r.xasp, scene->r.yasp);
    BKE_camera_params_compute_matrix(&params);

    BKE_camera_to_gpu_dof(camera, &fx_settings);

    is_ortho = params.is_ortho;
    copy_m4_m4(winmat, params.winmat);
  }
  else {
    rctf viewplane;
    float clip_start, clipend;

    is_ortho = ED_view3d_viewplane_get(
        depsgraph, v3d, rv3d, sizex, sizey, &viewplane, &clip_start, &clipend, NULL);
    if (is_ortho) {
      orthographic_m4(winmat,
                      viewplane.xmin,
                      viewplane.xmax,
                      viewplane.ymin,
                      viewplane.ymax,
                      -clipend,
                      clipend);
    }
    else {
      perspective_m4(winmat,
                     viewplane.xmin,
                     viewplane.xmax,
                     viewplane.ymin,
                     viewplane.ymax,
                     clip_start,
                     clipend);
    }
  }

  if ((samples && use_full_sample) == 0) {
    const bool do_color_management = (ibuf->rect_float == NULL);
    /* Single-pass render, common case */
    ED_view3d_draw_offscreen(depsgraph,
                             scene,
                             drawtype,
                             v3d,
                             ar,
                             sizex,
                             sizey,
                             NULL,
                             winmat,
                             draw_sky,
                             !is_ortho,
                             viewname,
                             &fx_settings,
                             do_color_management,
                             ofs,
                             NULL);

    if (ibuf->rect_float) {
      GPU_offscreen_read_pixels(ofs, GL_FLOAT, ibuf->rect_float);
    }
    else if (ibuf->rect) {
      GPU_offscreen_read_pixels(ofs, GL_UNSIGNED_BYTE, ibuf->rect);
    }
  }
  else {
    /* Multi-pass render, use accumulation buffer & jitter for 'full' oversampling.
     * Use because OpenGL may use a lower quality MSAA, and only over-sample edges. */
    static float jit_ofs[32][2];
    float winmat_jitter[4][4];
    float *rect_temp = (ibuf->rect_float) ?
                           ibuf->rect_float :
                           MEM_mallocN(sizex * sizey * sizeof(float[4]), "rect_temp");
    float *accum_buffer = MEM_mallocN(sizex * sizey * sizeof(float[4]), "accum_buffer");
    GPUViewport *viewport = GPU_viewport_create_from_offscreen(ofs);

    BLI_jitter_init(jit_ofs, samples);

    /* first sample buffer, also initializes 'rv3d->persmat' */
    ED_view3d_draw_offscreen(depsgraph,
                             scene,
                             drawtype,
                             v3d,
                             ar,
                             sizex,
                             sizey,
                             NULL,
                             winmat,
                             draw_sky,
                             !is_ortho,
                             viewname,
                             &fx_settings,
                             false,
                             ofs,
                             viewport);
    GPU_offscreen_read_pixels(ofs, GL_FLOAT, accum_buffer);

    /* skip the first sample */
    for (int j = 1; j < samples; j++) {
      copy_m4_m4(winmat_jitter, winmat);
      window_translate_m4(winmat_jitter,
                          rv3d->persmat,
                          (jit_ofs[j][0] * 2.0f) / sizex,
                          (jit_ofs[j][1] * 2.0f) / sizey);

      ED_view3d_draw_offscreen(depsgraph,
                               scene,
                               drawtype,
                               v3d,
                               ar,
                               sizex,
                               sizey,
                               NULL,
                               winmat_jitter,
                               draw_sky,
                               !is_ortho,
                               viewname,
                               &fx_settings,
                               false,
                               ofs,
                               viewport);
      GPU_offscreen_read_pixels(ofs, GL_FLOAT, rect_temp);

      uint i = sizex * sizey * 4;
      while (i--) {
        accum_buffer[i] += rect_temp[i];
      }
    }

    {
      /* don't free data owned by 'ofs' */
      GPU_viewport_clear_from_offscreen(viewport);
      GPU_viewport_free(viewport);
    }

    if (ibuf->rect_float == NULL) {
      MEM_freeN(rect_temp);
    }

    if (ibuf->rect_float) {
      float *rect_float = ibuf->rect_float;
      uint i = sizex * sizey * 4;
      while (i--) {
        rect_float[i] = accum_buffer[i] / samples;
      }
    }
    else {
      uchar *rect_ub = (uchar *)ibuf->rect;
      uint i = sizex * sizey * 4;
      while (i--) {
        rect_ub[i] = (uchar)(255.0f * accum_buffer[i] / samples);
      }
    }

    MEM_freeN(accum_buffer);
  }

  /* unbind */
  GPU_offscreen_unbind(ofs, true);

  if (own_ofs) {
    GPU_offscreen_free(ofs);
  }

  DRW_opengl_context_disable();

  if (old_fb) {
    GPU_framebuffer_bind(old_fb);
  }

  if (ibuf->rect_float && ibuf->rect) {
    IMB_rect_from_float(ibuf);
  }

  return ibuf;
}

/**
 * Creates own fake 3d views (wrapping #ED_view3d_draw_offscreen_imbuf)
 *
 * \param ofs: Optional off-screen buffer can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 *
 * \note used by the sequencer
 */
ImBuf *ED_view3d_draw_offscreen_imbuf_simple(Depsgraph *depsgraph,
                                             Scene *scene,
                                             View3DShading *shading_override,
                                             int drawtype,
                                             Object *camera,
                                             int width,
                                             int height,
                                             uint flag,
                                             uint draw_flags,
                                             int alpha_mode,
                                             int samples,
                                             const char *viewname,
                                             GPUOffScreen *ofs,
                                             char err_out[256])
{
  View3D v3d = {NULL};
  ARegion ar = {NULL};
  RegionView3D rv3d = {{{0}}};

  /* connect data */
  v3d.regionbase.first = v3d.regionbase.last = &ar;
  ar.regiondata = &rv3d;
  ar.regiontype = RGN_TYPE_WINDOW;

  v3d.camera = camera;
  View3DShading *source_shading_settings = &scene->display.shading;
  if (draw_flags & V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS && shading_override != NULL) {
    source_shading_settings = shading_override;
  }
  memcpy(&v3d.shading, source_shading_settings, sizeof(View3DShading));
  v3d.shading.type = drawtype;

  if (drawtype == OB_MATERIAL) {
    v3d.shading.flag = V3D_SHADING_SCENE_WORLD | V3D_SHADING_SCENE_LIGHTS;
  }

  v3d.flag2 = V3D_HIDE_OVERLAYS;

  if (draw_flags & V3D_OFSDRAW_SHOW_ANNOTATION) {
    v3d.flag2 |= V3D_SHOW_ANNOTATION;
  }

  v3d.shading.background_type = V3D_SHADING_BACKGROUND_WORLD;

  rv3d.persp = RV3D_CAMOB;

  copy_m4_m4(rv3d.viewinv, v3d.camera->obmat);
  normalize_m4(rv3d.viewinv);
  invert_m4_m4(rv3d.viewmat, rv3d.viewinv);

  {
    CameraParams params;
    const Object *view_camera_eval = DEG_get_evaluated_object(
        depsgraph, BKE_camera_multiview_render(scene, v3d.camera, viewname));

    BKE_camera_params_init(&params);
    BKE_camera_params_from_object(&params, view_camera_eval);
    BKE_camera_multiview_params(&scene->r, &params, view_camera_eval, viewname);
    BKE_camera_params_compute_viewplane(&params, width, height, scene->r.xasp, scene->r.yasp);
    BKE_camera_params_compute_matrix(&params);

    copy_m4_m4(rv3d.winmat, params.winmat);
    v3d.clip_start = params.clip_start;
    v3d.clip_end = params.clip_end;
    v3d.lens = params.lens;
  }

  mul_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
  invert_m4_m4(rv3d.persinv, rv3d.viewinv);

  return ED_view3d_draw_offscreen_imbuf(depsgraph,
                                        scene,
                                        drawtype,
                                        &v3d,
                                        &ar,
                                        width,
                                        height,
                                        flag,
                                        draw_flags,
                                        alpha_mode,
                                        samples,
                                        viewname,
                                        ofs,
                                        err_out);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport Clipping
 * \{ */

static bool view3d_clipping_test(const float co[3], const float clip[6][4])
{
  if (plane_point_side_v3(clip[0], co) > 0.0f) {
    if (plane_point_side_v3(clip[1], co) > 0.0f) {
      if (plane_point_side_v3(clip[2], co) > 0.0f) {
        if (plane_point_side_v3(clip[3], co) > 0.0f) {
          return false;
        }
      }
    }
  }

  return true;
}

/* for 'local' ED_view3d_clipping_local must run first
 * then all comparisons can be done in localspace */
bool ED_view3d_clipping_test(const RegionView3D *rv3d, const float co[3], const bool is_local)
{
  return view3d_clipping_test(co, is_local ? rv3d->clip_local : rv3d->clip);
}

/** \} */
