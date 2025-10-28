/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <cmath>

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"
#include "BLI_math_half.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"

#include "BKE_armature.hh"
#include "BKE_camera.h"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_image.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_unit.hh"
#include "BKE_viewer_path.hh"

#include "BLF_api.hh"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "DRW_engine.hh"
#include "DRW_select_buffer.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_info.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_view3d_offscreen.hh"
#include "ED_viewer_path.hh"

#include "ANIM_bone_collections.hh"

#include "DEG_depsgraph_query.hh"

#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh"

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RE_engine.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "ANIM_keyframing.hh"

#include "view3d_intern.hh" /* own include */

using blender::float4;

#define M_GOLDEN_RATIO_CONJUGATE 0.618033988749895f

#define VIEW3D_OVERLAY_LINEHEIGHT (UI_style_get()->widget.points * UI_SCALE_FAC * 1.6f)

/* -------------------------------------------------------------------- */
/** \name General Functions
 * \{ */

void ED_view3d_update_viewmat(const Depsgraph *depsgraph,
                              const Scene *scene,
                              View3D *v3d,
                              ARegion *region,
                              const float viewmat[4][4],
                              const float winmat[4][4],
                              const rcti *rect,
                              bool offscreen)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  /* setup window matrices */
  if (winmat) {
    copy_m4_m4(rv3d->winmat, winmat);
  }
  else {
    view3d_winmatrix_set(depsgraph, region, v3d, rect);
  }

  /* setup view matrix */
  if (viewmat) {
    copy_m4_m4(rv3d->viewmat, viewmat);
  }
  else {
    float rect_scale[2];
    if (rect) {
      rect_scale[0] = float(BLI_rcti_size_x(rect)) / float(region->winx);
      rect_scale[1] = float(BLI_rcti_size_y(rect)) / float(region->winy);
    }
    /* NOTE: calls BKE_object_where_is_calc for camera... */
    view3d_viewmatrix_set(depsgraph, scene, v3d, rv3d, rect ? rect_scale : nullptr);
  }
  /* update utility matrices */
  mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
  invert_m4_m4(rv3d->persinv, rv3d->persmat);
  invert_m4_m4(rv3d->viewinv, rv3d->viewmat);

  /* calculate GLSL view dependent values */

  /* store window coordinates scaling/offset */
  if (!offscreen && rv3d->persp == RV3D_CAMOB && v3d->camera) {
    rctf cameraborder;
    ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, false, &cameraborder);
    rv3d->viewcamtexcofac[0] = float(region->winx) / BLI_rctf_size_x(&cameraborder);
    rv3d->viewcamtexcofac[1] = float(region->winy) / BLI_rctf_size_y(&cameraborder);

    rv3d->viewcamtexcofac[2] = -rv3d->viewcamtexcofac[0] * cameraborder.xmin / float(region->winx);
    rv3d->viewcamtexcofac[3] = -rv3d->viewcamtexcofac[1] * cameraborder.ymin / float(region->winy);
  }
  else {
    rv3d->viewcamtexcofac[0] = rv3d->viewcamtexcofac[1] = 1.0f;
    rv3d->viewcamtexcofac[2] = rv3d->viewcamtexcofac[3] = 0.0f;
  }

  /* Calculate pixel-size factor once, this is used for lights and object-centers. */
  {
    /* NOTE:  '1.0f / len_v3(v1)'  replaced  'len_v3(rv3d->viewmat[0])'
     * because of float point precision problems at large values #23908. */
    float v1[3], v2[3];
    float len_px, len_sc;

    v1[0] = rv3d->persmat[0][0];
    v1[1] = rv3d->persmat[1][0];
    v1[2] = rv3d->persmat[2][0];

    v2[0] = rv3d->persmat[0][1];
    v2[1] = rv3d->persmat[1][1];
    v2[2] = rv3d->persmat[2][1];

    len_px = 2.0f / sqrtf(min_ff(len_squared_v3(v1), len_squared_v3(v2)));

    if (rect) {
      len_sc = float(max_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)));
    }
    else {
      len_sc = float(std::max(region->winx, region->winy));
    }

    rv3d->pixsize = len_px / len_sc;
  }
}

static void view3d_main_region_setup_view(Depsgraph *depsgraph,
                                          Scene *scene,
                                          View3D *v3d,
                                          ARegion *region,
                                          const float viewmat[4][4],
                                          const float winmat[4][4],
                                          const rcti *rect)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_update_viewmat(depsgraph, scene, v3d, region, viewmat, winmat, rect, false);

  /* Set for GPU drawing. */
  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);
}

static void view3d_main_region_setup_offscreen(Depsgraph *depsgraph,
                                               const Scene *scene,
                                               View3D *v3d,
                                               ARegion *region,
                                               const float viewmat[4][4],
                                               const float winmat[4][4])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  ED_view3d_update_viewmat(depsgraph, scene, v3d, region, viewmat, winmat, nullptr, true);

  /* Set for GPU drawing. */
  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);
}

static bool view3d_stereo3d_active(wmWindow *win,
                                   const Scene *scene,
                                   View3D *v3d,
                                   RegionView3D *rv3d)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if ((v3d->camera == nullptr) || (v3d->camera->type != OB_CAMERA) || rv3d->persp != RV3D_CAMOB) {
    return false;
  }

  switch (v3d->stereo3d_camera) {
    case STEREO_MONO_ID:
      return false;
      break;
    case STEREO_3D_ID:
      /* win will be nullptr when calling this from the selection or draw loop. */
      if ((win == nullptr) || (WM_stereo3d_enabled(win, true) == false)) {
        return false;
      }
      if (((scene->r.views_format & SCE_VIEWS_FORMAT_MULTIVIEW) != 0) &&
          !BKE_scene_multiview_is_stereo3d(&scene->r))
      {
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
    Depsgraph *depsgraph, Scene *scene, View3D *v3d, ARegion *region, const rcti *rect)
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
    data_eval = DEG_get_evaluated(depsgraph, data);

    shiftx = data_eval->shiftx;

    BLI_thread_lock(LOCK_VIEW3D);
    data_eval->shiftx = BKE_camera_multiview_shift_x(&scene->r, v3d->camera, viewname);

    BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
    view3d_main_region_setup_view(depsgraph, scene, v3d, region, viewmat, nullptr, rect);

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
    view3d_main_region_setup_view(depsgraph, scene, v3d, region, viewmat, nullptr, rect);

    v3d->camera = view_ob;
    BLI_thread_unlock(LOCK_VIEW3D);
  }
}

#ifdef WITH_XR_OPENXR
static void view3d_xr_mirror_setup(const wmWindowManager *wm,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   View3D *v3d,
                                   ARegion *region,
                                   const rcti *rect)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float viewmat[4][4];
  const float lens_old = v3d->lens;

  if (!WM_xr_session_state_viewer_pose_matrix_info_get(&wm->xr, viewmat, &v3d->lens)) {
    /* Can't get info from XR session, use fallback values. */
    copy_m4_m4(viewmat, rv3d->viewmat);
    v3d->lens = lens_old;
  }
  view3d_main_region_setup_view(depsgraph, scene, v3d, region, viewmat, nullptr, rect);

  /* Set draw flags. */
  SET_FLAG_FROM_TEST(v3d->flag2,
                     (wm->xr.session_settings.draw_flags & V3D_OFSDRAW_XR_SHOW_CONTROLLERS) != 0,
                     V3D_XR_SHOW_CONTROLLERS);
  SET_FLAG_FROM_TEST(v3d->flag2,
                     (wm->xr.session_settings.draw_flags & V3D_OFSDRAW_XR_SHOW_CUSTOM_OVERLAYS) !=
                         0,
                     V3D_XR_SHOW_CUSTOM_OVERLAYS);
  /* Hide navigation gizmo since it gets distorted if the view matrix has a scale factor. */
  v3d->gizmo_flag |= V3D_GIZMO_HIDE_NAVIGATE;

  /* Reset overridden View3D data. */
  v3d->lens = lens_old;
}
#endif /* WITH_XR_OPENXR */

void ED_view3d_draw_setup_view(const wmWindowManager *wm,
                               wmWindow *win,
                               Depsgraph *depsgraph,
                               Scene *scene,
                               ARegion *region,
                               View3D *v3d,
                               const float viewmat[4][4],
                               const float winmat[4][4],
                               const rcti *rect)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

#ifdef WITH_XR_OPENXR
  /* Setup the view matrix. */
  if (ED_view3d_is_region_xr_mirror_active(wm, v3d, region)) {
    view3d_xr_mirror_setup(wm, depsgraph, scene, v3d, region, rect);
  }
  else
#endif
      if (view3d_stereo3d_active(win, scene, v3d, rv3d))
  {
    view3d_stereo3d_setup(depsgraph, scene, v3d, region, rect);
  }
  else {
    view3d_main_region_setup_view(depsgraph, scene, v3d, region, viewmat, winmat, rect);
  }

#ifndef WITH_XR_OPENXR
  UNUSED_VARS(wm);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View Border
 * \{ */

static void view3d_camera_border(const Scene *scene,
                                 const Depsgraph *depsgraph,
                                 const ARegion *region,
                                 const View3D *v3d,
                                 const RegionView3D *rv3d,
                                 rctf *r_viewborder,
                                 const bool no_shift,
                                 const bool no_zoom)
{
  CameraParams params;
  rctf rect_view, rect_camera;
  Object *camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);

  /* get viewport viewplane */
  BKE_camera_params_init(&params);
  BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);
  if (no_zoom) {
    params.zoom = 1.0f;
  }
  BKE_camera_params_compute_viewplane(&params, region->winx, region->winy, 1.0f, 1.0f);
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
                       region->winx;
  r_viewborder->xmax = ((rect_camera.xmax - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) *
                       region->winx;
  r_viewborder->ymin = ((rect_camera.ymin - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) *
                       region->winy;
  r_viewborder->ymax = ((rect_camera.ymax - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) *
                       region->winy;
}

void ED_view3d_calc_camera_border_size(const Scene *scene,
                                       Depsgraph *depsgraph,
                                       const ARegion *region,
                                       const View3D *v3d,
                                       const RegionView3D *rv3d,
                                       float r_size[2])
{
  rctf viewborder;

  view3d_camera_border(scene, depsgraph, region, v3d, rv3d, &viewborder, true, true);
  r_size[0] = BLI_rctf_size_x(&viewborder);
  r_size[1] = BLI_rctf_size_y(&viewborder);
}

void ED_view3d_calc_camera_border(const Scene *scene,
                                  const Depsgraph *depsgraph,
                                  const ARegion *region,
                                  const View3D *v3d,
                                  const RegionView3D *rv3d,
                                  const bool no_shift,
                                  rctf *r_viewborder)
{
  view3d_camera_border(scene, depsgraph, region, v3d, rv3d, r_viewborder, no_shift, false);
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
      ofs = w * (1.0f - M_GOLDEN_RATIO_CONJUGATE);
    }
    else {
      ofs = h * (h / w);
    }
    if (dir == 'B') {
      std::swap(y1, y2);
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
      ofs = h * (1.0f - M_GOLDEN_RATIO_CONJUGATE);
    }
    else {
      ofs = w * (w / h);
    }
    if (dir == 'B') {
      std::swap(x1, x2);
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

static void drawviewborder(Scene *scene, Depsgraph *depsgraph, ARegion *region, View3D *v3d)
{
  float x1, x2, y1, y2;
  float x1i, x2i, y1i, y2i;

  rctf viewborder;
  Camera *ca = nullptr;
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  if (v3d->camera == nullptr) {
    return;
  }
  if (v3d->camera->type == OB_CAMERA) {
    ca = static_cast<Camera *>(v3d->camera->data);
  }

  ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, false, &viewborder);
  /* the offsets */
  x1 = viewborder.xmin;
  y1 = viewborder.ymin;
  x2 = viewborder.xmax;
  y2 = viewborder.ymax;

  GPU_line_width(1.0f);

  /* apply offsets so the real 3D camera shows through */

  /* NOTE: quite un-scientific but without this bit extra
   * 0.0001 on the lower left the 2D border sometimes
   * obscures the 3D camera border */
  /* NOTE: with VIEW3D_CAMERA_BORDER_HACK defined this error isn't noticeable
   * but keep it here in case we need to remove the workaround */
  x1i = int(x1 - 1.0001f);
  y1i = int(y1 - 1.0001f);
  x2i = int(x2 + (1.0f - 0.0001f));
  y2i = int(y2 + (1.0f - 0.0001f));

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  /* First, solid lines. */
  {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    /* passepartout, specified in camera edit buttons */
    if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001f &&
        v3d->flag2 & V3D_SHOW_CAMERA_PASSEPARTOUT)
    {
      const float winx = (region->winx + 1);
      const float winy = (region->winy + 1);

      float alpha = 1.0f;

      if (ca->passepartalpha != 1.0f) {
        GPU_blend(GPU_BLEND_ALPHA);
        alpha = ca->passepartalpha;
      }

      immUniformThemeColorAlpha(TH_CAMERA_PASSEPARTOUT, alpha);

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

      GPU_blend(GPU_BLEND_NONE);
      immUniformThemeColor3(TH_BACK);
      imm_draw_box_wire_2d(shdr_pos, x1i, y1i, x2i, y2i);
    }

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
  if (v3d->flag2 & V3D_HIDE_OVERLAYS || !(v3d->flag2 & V3D_SHOW_CAMERA_GUIDES)) {
    return;
  }

  /* And now, the dashed lines! */
  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  {
    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniform1f("dash_width", 6.0f);
    immUniform1f("udash_factor", 0.5f);

    /* outer line not to confuse with object selection */
    if (v3d->flag2 & V3D_LOCK_CAMERA) {
      immUniformThemeColor(TH_REDALERT);
      imm_draw_box_wire_2d(shdr_pos, x1i - 1, y1i - 1, x2i + 1, y2i + 1);
    }

    immUniformThemeColor3(TH_VIEW_OVERLAY);
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
  if (ca && (v3d->flag2 & V3D_SHOW_CAMERA_GUIDES)) {
    GPU_blend(GPU_BLEND_ALPHA);
    immUniformColor4fv(ca->composition_guide_color);

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
      drawviewborder_grid3(shdr_pos, x1, x2, y1, y2, 1.0f - M_GOLDEN_RATIO_CONJUGATE);
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
      rctf margins_rect{};
      margins_rect.xmin = x1;
      margins_rect.xmax = x2;
      margins_rect.ymin = y1;
      margins_rect.ymax = y2;

      /* draw */
      immUniformThemeColorAlpha(TH_VIEW_OVERLAY, 0.75f);

      UI_draw_safe_areas(
          shdr_pos, &margins_rect, scene->safe_areas.title, scene->safe_areas.action);

      if (ca->flag & CAM_SHOW_SAFE_CENTER) {
        rctf center_rect{};
        center_rect.xmin = x1;
        center_rect.xmax = x2;
        center_rect.ymin = y1;
        center_rect.ymax = y2;
        UI_draw_safe_areas(shdr_pos,
                           &center_rect,
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
      immUniformThemeColorShadeAlpha(TH_VIEW_OVERLAY, 100, 255);

      /* TODO: Was using:
       * `UI_draw_roundbox_4fv(false, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 2.0f, color);`
       * We'll probably need a new imm_draw_line_roundbox_dashed or that - though in practice the
       * 2.0f round corner effect was nearly not visible anyway. */
      imm_draw_box_wire_2d(shdr_pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    }

    GPU_blend(GPU_BLEND_NONE);
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

static void drawrenderborder(ARegion *region, View3D *v3d)
{
  /* use the same program for everything */
  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform4f("color", 1.0f, 0.25f, 0.25f, 1.0f);
  immUniform1f("dash_width", 6.0f);
  immUniform1f("udash_factor", 0.5f);

  imm_draw_box_wire_2d(shdr_pos,
                       v3d->render_border.xmin * region->winx,
                       v3d->render_border.ymin * region->winy,
                       v3d->render_border.xmax * region->winx,
                       v3d->render_border.ymax * region->winy);

  immUnbindProgram();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Other Elements
 * \{ */

float ED_scene_grid_scale(const Scene *scene, const char **r_grid_unit)
{
  /* apply units */
  if (scene->unit.system) {
    const void *usys;
    int len;

    BKE_unit_system_get(scene->unit.system, B_UNIT_LENGTH, &usys, &len);

    if (usys) {
      int i = BKE_unit_base_get(usys);
      if (r_grid_unit) {
        *r_grid_unit = IFACE_(BKE_unit_display_name_get(usys, i));
      }
      return float(BKE_unit_scalar_get(usys, i)) / scene->unit.scale_length;
    }
  }

  return 1.0f;
}

float ED_view3d_grid_scale(const Scene *scene, const View3D *v3d, const char **r_grid_unit)
{
  return v3d->grid * ED_scene_grid_scale(scene, r_grid_unit);
}

#define STEPS_LEN 8
static void view3d_grid_steps_ex(const Scene *scene,
                                 const View3D *v3d,
                                 const RegionView3D *rv3d,
                                 float r_grid_steps[STEPS_LEN],
                                 void const **r_usys_pt,
                                 int *r_len)
{
  const void *usys;
  int len;
  BKE_unit_system_get(scene->unit.system, B_UNIT_LENGTH, &usys, &len);
  float grid_scale = v3d->grid;
  BLI_assert(STEPS_LEN >= len);

  if (usys) {
    if (rv3d->view == RV3D_VIEW_USER) {
      /* Skip steps */
      len = BKE_unit_base_get(usys) + 1;
    }

    grid_scale /= scene->unit.scale_length;

    int i;
    for (i = 0; i < len; i++) {
      r_grid_steps[i] = float(BKE_unit_scalar_get(usys, len - 1 - i)) * grid_scale;
    }
    for (; i < STEPS_LEN; i++) {
      /* Fill last slots */
      r_grid_steps[i] = r_grid_steps[len - 1];
    }
  }
  else {
    if (rv3d->view != RV3D_VIEW_USER) {
      /* Allow 3 more subdivisions. */
      grid_scale /= powf(v3d->gridsubdiv, 3);
    }
    int subdiv = 1;
    for (int i = 0;; i++) {
      r_grid_steps[i] = grid_scale * subdiv;

      if (i == STEPS_LEN - 1) {
        break;
      }
      subdiv *= v3d->gridsubdiv;
    }
  }
  if (r_usys_pt) {
    *r_usys_pt = usys;
  }
  if (r_len) {
    *r_len = len;
  }
}

void ED_view3d_grid_steps(const Scene *scene,
                          const View3D *v3d,
                          const RegionView3D *rv3d,
                          float r_grid_steps[STEPS_LEN])
{
  view3d_grid_steps_ex(scene, v3d, rv3d, r_grid_steps, nullptr, nullptr);
}

float ED_view3d_grid_view_scale(const Scene *scene,
                                const View3D *v3d,
                                const ARegion *region,
                                const char **r_grid_unit)
{
  float grid_scale;
  const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  if (!rv3d->is_persp && RV3D_VIEW_IS_AXIS(rv3d->view)) {
    /* Decrease the distance between grid snap points depending on zoom. */
    float dist = 12.0f / (region->sizex * rv3d->winmat[0][0]);
    float grid_steps[STEPS_LEN];
    const void *usys;
    int grid_steps_len;
    view3d_grid_steps_ex(scene, v3d, rv3d, grid_steps, &usys, &grid_steps_len);
    int i = 0;
    while (true) {
      grid_scale = grid_steps[i];
      if (grid_scale > dist || i == (grid_steps_len - 1)) {
        break;
      }
      i++;
    }

    if (r_grid_unit && usys) {
      *r_grid_unit = IFACE_(BKE_unit_display_name_get(usys, grid_steps_len - i - 1));
    }
  }
  else {
    grid_scale = ED_view3d_grid_scale(scene, v3d, r_grid_unit);
  }

  return grid_scale;
}

#undef STEPS_LEN

static void draw_view_axis(RegionView3D *rv3d, const rcti *rect)
{
  const float k = U.rvisize * UI_SCALE_FAC; /* axis size */
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
  float axis_col[3][4];

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
    UI_GetThemeColorShade3fv(TH_AXIS_X + i, bright, axis_col[i]); /* rgb */
    axis_col[i][3] = hypotf(vec[0], vec[1]);                      /* alpha */
  }

  /* draw axis lines */
  GPU_line_width(2.0f);
  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  uint col = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
  immBegin(GPU_PRIM_LINES, 6);

  for (int axis_i = 0; axis_i < 3; axis_i++) {
    int i = axis_order[axis_i];

    immAttr4fv(col, axis_col[i]);
    immVertex2f(pos, startx, starty);
    immAttr4fv(col, axis_col[i]);
    immVertex2fv(pos, axis_pos[i]);
  }

  immEnd();
  immUnbindProgram();
  GPU_line_smooth(false);

  /* draw axis names */
  for (int axis_i = 0; axis_i < 3; axis_i++) {
    int i = axis_order[axis_i];

    const char axis_text[2] = {char('x' + i), '\0'};
    BLF_color4fv(BLF_default(), axis_col[i]);
    BLF_draw_default(axis_pos[i][0] + 2, axis_pos[i][1] + 2, 0.0f, axis_text, 1);
  }
}

#ifdef WITH_INPUT_NDOF
/**
 * Draw center and axis of rotation for ongoing 3D mouse navigation.
 */
static void draw_ndof_guide_orbit_axis(const RegionView3D *rv3d)
{
  float o[3];   /* center of rotation */
  float end[3]; /* endpoints for drawing */

  float color[4] = {0.0f, 0.42f, 1.0f, 1.0f}; /* bright blue so it matches device LEDs */

  negate_v3_v3(o, rv3d->ofs);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_depth_mask(false); /* Don't overwrite the Z-buffer. */

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  uint col = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  if (rv3d->ndof_rot_angle != 0.0f) {
    /* -- draw rotation axis -- */
    float scaled_axis[3];
    const float scale = rv3d->dist;
    mul_v3_v3fl(scaled_axis, rv3d->ndof_rot_axis, scale);

    immBegin(GPU_PRIM_LINE_STRIP, 3);
    color[3] = 0; /* more transparent toward the ends */
    immAttr4fv(col, color);
    add_v3_v3v3(end, o, scaled_axis);
    immVertex3fv(pos, end);

#  if 0
    color[3] = 0.2f + fabsf(rv3d->rot_angle); /* modulate opacity with angle */
    /* ^^ neat idea, but angle is frame-rate dependent, so it's usually close to 0.2 */
#  endif

    color[3] = 0.5f; /* more opaque toward the center */
    immAttr4fv(col, color);
    immVertex3fv(pos, o);

    color[3] = 0;
    immAttr4fv(col, color);
    sub_v3_v3v3(end, o, scaled_axis);
    immVertex3fv(pos, end);
    immEnd();

    /* -- draw ring around rotation center -- */
    {
#  define ROT_AXIS_DETAIL 13

      const float s = 0.05f * scale;
      const float step = 2.0f * float(M_PI / ROT_AXIS_DETAIL);

      float q[4]; /* rotate ring so it's perpendicular to axis */
      const int upright = fabsf(rv3d->ndof_rot_axis[2]) >= 0.95f;
      if (!upright) {
        const float up[3] = {0.0f, 0.0f, 1.0f};
        float vis_angle, vis_axis[3];

        cross_v3_v3v3(vis_axis, up, rv3d->ndof_rot_axis);
        vis_angle = acosf(dot_v3v3(up, rv3d->ndof_rot_axis));
        axis_angle_to_quat(q, vis_axis, vis_angle);
      }

      immBegin(GPU_PRIM_LINE_LOOP, ROT_AXIS_DETAIL);
      color[3] = 0.25f; /* somewhat faint */
      immAttr4fv(col, color);
      float angle = 0.0f;
      for (int i = 0; i < ROT_AXIS_DETAIL; i++, angle += step) {
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

    color[3] = 1.0f; /* solid dot */
  }
  else {
    color[3] = 0.5f; /* see-through dot */
  }

  immUnbindProgram();

  /* -- draw rotation center -- */
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
  immUniform1f("size", 7.0f);
  immUniform4fv("color", float4(color));
  immBegin(GPU_PRIM_POINTS, 1);
  immAttr4fv(col, color);
  immVertex3fv(pos, o);
  immEnd();
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
  GPU_depth_mask(true);
}

static void draw_ndof_guide_orbit_center(const RegionView3D *rv3d)
{
  uchar color[4] = {0, 108, 255, 255}; /* bright blue so it matches device LEDs */
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_depth_mask(false); /* Don't overwrite the Z-buffer. */

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  uint col = GPU_vertformat_attr_add(format, "color", blender::gpu::VertAttrType::UNORM_8_8_8_8);

  immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
  immUniform1f("size", 7.0f);
  immUniform4fv("color", float4(color));
  immBegin(GPU_PRIM_POINTS, 1);
  immAttr4ubv(col, color);
  float center[3];
  negate_v3_v3(center, rv3d->ndof_ofs);
  immVertex3fv(pos, center);
  immEnd();
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
  GPU_depth_mask(true);
}

#endif /* WITH_INPUT_NDOF */

/**
 * Render and camera border
 */
static void view3d_draw_border(const bContext *C, ARegion *region)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  View3D *v3d = CTX_wm_view3d(C);

  if (rv3d->persp == RV3D_CAMOB) {
    drawviewborder(scene, depsgraph, region, v3d);
  }
  else if (v3d->flag2 & V3D_RENDER_BORDER) {
    drawrenderborder(region, v3d);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Text & Info
 * \{ */

/**
 * Draw Info
 */
static void view3d_draw_grease_pencil(const bContext * /*C*/)
{
  /* TODO: viewport. */
}

/**
 * Viewport Name
 */
static const char *view3d_get_name(View3D *v3d, RegionView3D *rv3d)
{
  const char *name = nullptr;

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
          const Camera *cam = static_cast<const Camera *>(v3d->camera->data);
          if (cam->type == CAM_PERSP) {
            name = IFACE_("Camera Perspective");
          }
          else if (cam->type == CAM_ORTHO) {
            name = IFACE_("Camera Orthographic");
          }
          else if (cam->type == CAM_PANO) {
            name = IFACE_("Camera Panoramic");
          }
          else {
            BLI_assert(cam->type == CAM_CUSTOM);
            name = IFACE_("Camera Custom");
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

static void draw_viewport_name(ARegion *region, View3D *v3d, int xoffset, int *yoffset)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const char *name = view3d_get_name(v3d, rv3d);
  const char *name_array[3] = {name, nullptr, nullptr};
  int name_array_len = 1;

  /* 6 is the maximum size of the axis roll text. */
  /* increase size for unicode languages (Chinese in UTF8...). */
  char tmpstr[96 + 6];

  if (RV3D_VIEW_IS_AXIS(rv3d->view) && (rv3d->view_axis_roll != RV3D_VIEW_AXIS_ROLL_0)) {
    const char *axis_roll;
    switch (rv3d->view_axis_roll) {
      case RV3D_VIEW_AXIS_ROLL_90:
        axis_roll = " 90\xC2\xB0";
        break;
      case RV3D_VIEW_AXIS_ROLL_180:
        axis_roll = " 180\xC2\xB0";
        break;
      default:
        axis_roll = " -90\xC2\xB0";
        break;
    }
    name_array[name_array_len++] = axis_roll;
  }

  if (v3d->localvd) {
    name_array[name_array_len++] = IFACE_(" (Local)");
  }

  /* Indicate that clipping region is enabled. */
  if (rv3d->rflag & RV3D_CLIPPING) {
    name_array[name_array_len++] = IFACE_(" (Clipped)");
  }

  if (name_array_len > 1) {
    BLI_string_join_array(tmpstr, sizeof(tmpstr), name_array, name_array_len);
    name = tmpstr;
  }
  *yoffset -= VIEW3D_OVERLAY_LINEHEIGHT;
  BLF_draw_default(xoffset, *yoffset, 0.0f, name, sizeof(tmpstr));
}

static bool is_grease_pencil_with_layer_keyframe(const Object &ob)
{
  if (ob.type != OB_GREASE_PENCIL) {
    return false;
  }

  using namespace blender::bke::greasepencil;
  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob.data);
  for (const Layer *layer : grease_pencil.layers()) {
    if (!layer->frames().is_empty()) {
      return true;
    }
  }
  return false;
}

/**
 * Draw info beside axes in top-left corner:
 * frame-number, collection, object name, bone name (if available), marker name (if available).
 */
static void draw_selected_name(
    const View3D *v3d, Scene *scene, ViewLayer *view_layer, Object *ob, int xoffset, int *yoffset)
{
  const int cfra = scene->r.cfra;
  const char *msg_pin = " (Soloed)";
  const char *msg_sep = " : ";
  const char *msg_space = " ";

  const int font_id = BLF_default();

  const char *info_array[16];
  int i = 0;

  struct {
    char frame[16];
  } info_buffers;

  /* Info can contain:
   * - 1 frame number `(7 + 2)`.
   * - 1 collection name `(MAX_ID_NAME - 2 + 3)`.
   * - 1 object name `(MAX_ID_NAME - 2)`.
   * - 1 object data name `(MAX_ID_NAME - 2)`.
   * - 2 non-ID data names (bones, shape-keys...) `(MAX_NAME * 2)`.
   * - 2 BREAD_CRUMB_SEPARATOR(s) `(6)`.
   * - 1 SHAPE_KEY_PINNED marker and a trailing '\0' `(9+1)` - translated, so give some room!
   * - 1 marker name `(MAX_NAME + 3)`.
   */

  SNPRINTF_UTF8(info_buffers.frame, "(%d)", cfra);
  info_array[i++] = info_buffers.frame;

  if ((ob == nullptr) || (ob->mode == OB_MODE_OBJECT)) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    LayerCollection *layer_collection = BKE_view_layer_active_collection_get(view_layer);
    info_array[i++] = msg_space;
    info_array[i++] = BKE_collection_ui_name_get(layer_collection->collection);
    if (ob != nullptr) {
      info_array[i++] = " |";
    }
  }

  /* get name of marker on current frame (if available) */
  const char *markern = BKE_scene_find_marker_name(scene, cfra);

  /* check if there is an object */
  if (ob) {
    info_array[i++] = msg_space;
    info_array[i++] = ob->id.name + 2;

    /* Show object data name when not in object mode. */
    if (ob->mode != OB_MODE_OBJECT) {
      if (const ID *data_id = static_cast<const ID *>(ob->data)) {
        info_array[i++] = " | ";
        info_array[i++] = data_id->name + 2;
      }
    }

    /* name(s) to display depends on type of object */
    if (ob->type == OB_ARMATURE) {
      bArmature *arm = static_cast<bArmature *>(ob->data);

      /* show name of active bone too (if possible) */
      if (arm->edbo) {
        if (arm->act_edbone) {
          info_array[i++] = msg_sep;
          info_array[i++] = arm->act_edbone->name;
        }
      }
      else if (ob->mode & OB_MODE_POSE) {
        if (arm->act_bone) {

          if (ANIM_bonecoll_is_visible_actbone(arm)) {
            info_array[i++] = msg_sep;
            info_array[i++] = arm->act_bone->name;
          }
        }
      }
    }
    else if (ELEM(ob->type, OB_MESH, OB_LATTICE, OB_CURVES_LEGACY)) {
      /* Try to display active bone and active shape-key too (if they exist). */

      if (ob->type == OB_MESH && ob->mode & OB_MODE_WEIGHT_PAINT) {
        Object *armobj = BKE_object_pose_armature_get(ob);
        if (armobj && armobj->mode & OB_MODE_POSE) {
          bArmature *arm = static_cast<bArmature *>(armobj->data);
          if (arm->act_bone) {
            if (ANIM_bonecoll_is_visible_actbone(arm)) {
              info_array[i++] = msg_sep;
              info_array[i++] = arm->act_bone->name;
            }
          }
        }
      }

      Key *key = BKE_key_from_object(ob);
      if (key) {
        KeyBlock *kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));
        if (kb) {
          info_array[i++] = msg_sep;
          info_array[i++] = kb->name;
          if (ob->shapeflag & OB_SHAPE_LOCK) {
            info_array[i++] = IFACE_(msg_pin);
          }
        }
      }
    }

    /* color depends on whether there is a keyframe */
    if (is_grease_pencil_with_layer_keyframe(*ob)) {
      UI_FontThemeColor(font_id, TH_TIME_GP_KEYFRAME);
    }

    if (blender::animrig::id_frame_has_keyframe((ID *)ob,
                                                /* BKE_scene_ctime_get(scene) */ float(cfra)))
    {
      UI_FontThemeColor(font_id, TH_KEYTYPE_KEYFRAME_SELECT);
    }
  }

  if (markern) {
    info_array[i++] = " <";
    info_array[i++] = markern;
    info_array[i++] = ">";
  }

  if (v3d->flag2 & V3D_SHOW_VIEWER) {
    if (!BLI_listbase_is_empty(&v3d->viewer_path.path)) {
      info_array[i++] = IFACE_(" (Viewer)");
    }
  }

  BLI_assert(i < int(ARRAY_SIZE(info_array)));

  char info[MAX_ID_NAME * 4];
  /* It's expected there will be enough room for the whole string in the buffer.
   * If not, increase it. */
  BLI_assert(BLI_string_len_array(info_array, i) < sizeof(info));

  BLI_string_join_array(info, sizeof(info), info_array, i);

  *yoffset -= VIEW3D_OVERLAY_LINEHEIGHT;
  BLF_draw_default(xoffset, *yoffset, 0.0f, info, sizeof(info));
}

static void draw_grid_unit_name(
    Scene *scene, ARegion *region, View3D *v3d, int xoffset, int *yoffset)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  if (!rv3d->is_persp && RV3D_VIEW_IS_AXIS(rv3d->view)) {
    const char *grid_unit = nullptr;
    ED_view3d_grid_view_scale(scene, v3d, region, &grid_unit);

    if (grid_unit) {
      char numstr[32] = "";
      if (v3d->grid != 1.0f) {
        SNPRINTF_UTF8(
            numstr, "%s " BLI_STR_UTF8_MULTIPLICATION_SIGN " %.4g", grid_unit, v3d->grid);
      }

      *yoffset -= VIEW3D_OVERLAY_LINEHEIGHT;
      BLF_draw_default(xoffset, *yoffset, 0.0f, numstr[0] ? numstr : grid_unit, sizeof(numstr));
    }
  }
}

void view3d_draw_region_info(const bContext *C, ARegion *region)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

#ifdef WITH_INPUT_NDOF
  if (U.ndof_flag & NDOF_SHOW_GUIDE_ORBIT_AXIS) {
    if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0) {
      /* It only makes sense to show when orbiting. */
      if (rv3d->ndof_rot_angle != 0.0f) {
        /* TODO: draw something else (but not this) during fly mode. */
        draw_ndof_guide_orbit_axis(rv3d);
      }
    }
  }

  if (U.ndof_flag & NDOF_SHOW_GUIDE_ORBIT_CENTER) {
    /* Draw this only when orbiting and auto orbit-center is enabled */
    if (NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U) && (U.ndof_flag & NDOF_ORBIT_CENTER_AUTO)) {
      if (rv3d->ndof_flag & RV3D_NDOF_OFS_IS_VALID) {
        /* When the center is locked, the auto-center is not used. */
        if (!(v3d->ob_center_cursor || v3d->ob_center)) {
          /* It only makes sense to show when orbiting. */
          if (rv3d->ndof_rot_angle != 0.0f) {
            draw_ndof_guide_orbit_center(rv3d);
          }
        }
      }
    }
  }
#endif

  /* correct projection matrix */
  ED_region_pixelspace(region);

  /* local coordinate visible rect inside region, to accommodate overlapping ui */
  const rcti *rect = ED_region_visible_rect(region);

  view3d_draw_border(C, region);
  view3d_draw_grease_pencil(C);

  BLF_batch_draw_begin();

  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_NAVIGATE)) {
    /* pass */
  }
  else {
    switch ((eUserpref_MiniAxisType)U.mini_axis_type) {
      case USER_MINI_AXIS_TYPE_GIZMO:
        /* The gizmo handles its own drawing. */
        break;
      case USER_MINI_AXIS_TYPE_MINIMAL:
        draw_view_axis(rv3d, rect);
      case USER_MINI_AXIS_TYPE_NONE:
        break;
    }
  }

  if ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) {
    int xoffset = rect->xmin + (0.5f * U.widget_unit);
    int yoffset = rect->ymax - (0.1f * U.widget_unit);

    const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
    UI_fontstyle_set(fstyle);
    BLF_default_size(fstyle->points);
    BLF_set_default();

    const int font_id = BLF_default();
    float text_color[4], shadow_color[4];
    ED_view3d_text_colors_get(scene, v3d, text_color, shadow_color);
    BLF_color4fv(font_id, text_color);
    BLF_enable(font_id, BLF_SHADOW);
    BLF_shadow_offset(font_id, 0, 0);
    BLF_shadow(font_id, FontShadowType::Outline, shadow_color);

    if ((v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) == 0) {
      if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(wm)) {
        ED_scene_draw_fps(scene, xoffset, &yoffset);
        BLF_color4fv(font_id, text_color);
      }
      else if (U.uiflag & USER_SHOW_VIEWPORTNAME) {
        draw_viewport_name(region, v3d, xoffset, &yoffset);
      }

      if (U.uiflag & USER_DRAWVIEWINFO) {
        BKE_view_layer_synced_ensure(scene, view_layer);
        Object *ob = BKE_view_layer_active_object_get(view_layer);
        draw_selected_name(v3d, scene, view_layer, ob, xoffset, &yoffset);
        BLF_color4fv(font_id, text_color);
      }

      if (v3d->gridflag & (V3D_SHOW_FLOOR | V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z)) {
        /* draw below the viewport name */
        draw_grid_unit_name(scene, region, v3d, xoffset, &yoffset);
      }

      DRW_draw_region_engine_info(xoffset, &yoffset, VIEW3D_OVERLAY_LINEHEIGHT);
    }

    if (v3d->overlay.flag & V3D_OVERLAY_STATS) {
      View3D *v3d_local = v3d->localvd ? v3d : nullptr;
      ED_info_draw_stats(
          bmain, scene, view_layer, v3d_local, xoffset, &yoffset, VIEW3D_OVERLAY_LINEHEIGHT);
    }

    /* Set the size back to the default hard-coded size. Otherwise anyone drawing after this,
     * without setting explicit size, will draw with widget size. That is probably ideal,
     * but size should be set at the calling site not just carried over from here. */
    BLF_default_size(UI_DEFAULT_TEXT_POINTS);
    BLF_disable(font_id, BLF_SHADOW);
  }

  BLF_batch_draw_end();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Viewport Contents
 * \{ */

static void view3d_draw_view(const bContext *C, ARegion *region)
{
  ED_view3d_draw_setup_view(CTX_wm_manager(C),
                            CTX_wm_window(C),
                            CTX_data_expect_evaluated_depsgraph(C),
                            CTX_data_scene(C),
                            region,
                            CTX_wm_view3d(C),
                            nullptr,
                            nullptr,
                            nullptr);

  /* Only 100% compliant on new spec goes below */
  DRW_draw_view(C);
}

RenderEngineType *ED_view3d_engine_type(const Scene *scene, int drawtype)
{
  /*
   * Temporary viewport draw modes until we have a proper system.
   * all modes are done in the draw manager, except external render
   * engines like Cycles.
   */
  RenderEngineType *type = RE_engines_find(scene->r.engine);
  if (drawtype == OB_MATERIAL && (type->flag & RE_USE_EEVEE_VIEWPORT)) {
    return RE_engines_find(RE_engine_id_BLENDER_EEVEE);
  }
  return type;
}

static void view3d_update_viewer_path(const bContext *C)
{
  View3D *v3d = CTX_wm_view3d(C);
  WorkSpace *workspace = CTX_wm_workspace(C);
  /* Always use viewer path from workspace, pinning is not supported currently. */
  if (!BKE_viewer_path_equal(&v3d->viewer_path, &workspace->viewer_path)) {
    BKE_viewer_path_clear(&v3d->viewer_path);
    BKE_viewer_path_copy(&v3d->viewer_path, &workspace->viewer_path);
  }
}

void view3d_main_region_draw(const bContext *C, ARegion *region)
{
  using namespace blender::draw;
  Main *bmain = CTX_data_main(C);
  View3D *v3d = CTX_wm_view3d(C);

  view3d_update_viewer_path(C);
  view3d_draw_view(C, region);

  DRW_cache_free_old_subdiv();
  DRW_cache_free_old_batches(bmain);
  BKE_image_free_old_gputextures(bmain);

  /* No depth test for drawing action zones afterwards. */
  GPU_depth_test(GPU_DEPTH_NONE);

  v3d->runtime.flag &= ~V3D_RUNTIME_DEPTHBUF_OVERRIDDEN;
  /* TODO: Clear cache? */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Off-screen Drawing
 * \{ */

static void view3d_stereo3d_setup_offscreen(Depsgraph *depsgraph,
                                            const Scene *scene,
                                            View3D *v3d,
                                            ARegion *region,
                                            const float winmat[4][4],
                                            const char *viewname)
{
  /* update the viewport matrices with the new camera */
  if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
    float viewmat[4][4];
    const bool is_left = STREQ(viewname, STEREO_LEFT_NAME);

    BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
    view3d_main_region_setup_offscreen(depsgraph, scene, v3d, region, viewmat, winmat);
  }
  else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
    float viewmat[4][4];
    Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

    BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
    view3d_main_region_setup_offscreen(depsgraph, scene, v3d, region, viewmat, winmat);
  }
}

void ED_view3d_draw_offscreen(Depsgraph *depsgraph,
                              const Scene *scene,
                              eDrawType drawtype,
                              View3D *v3d,
                              ARegion *region,
                              int winx,
                              int winy,
                              const float viewmat[4][4],
                              const float winmat[4][4],
                              bool is_image_render,
                              bool draw_background,
                              const char *viewname,
                              const bool do_color_management,
                              const bool restore_rv3d_mats,
                              GPUOffScreen *ofs,
                              GPUViewport *viewport)
{
  using namespace blender::draw;
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, drawtype);

  /* Store `orig` variables. */
  struct {
    bThemeState theme_state;

    /* #View3D */
    eDrawType v3d_shading_type;
    Object *v3d_camera;
    float v3d_lens;

    /* #Region */
    int region_winx, region_winy;
    rcti region_winrct;

    /* #RegionView3D */
    /**
     * Needed so the value won't be left overwritten,
     * Without this the #wmPaintCursor can't use the pixel size & view matrices for drawing.
     */
    RV3DMatrixStore *rv3d_mats;
    char rv3d_persp;
  } orig{};
  orig.v3d_shading_type = eDrawType(v3d->shading.type);
  orig.v3d_camera = v3d->camera;
  orig.v3d_lens = v3d->lens;
  orig.region_winx = region->winx;
  orig.region_winy = region->winy;
  orig.region_winrct = region->winrct;
  orig.rv3d_persp = rv3d->persp;
  orig.rv3d_mats = ED_view3d_mats_rv3d_backup(static_cast<RegionView3D *>(region->regiondata));

  UI_Theme_Store(&orig.theme_state);
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* Set temporary new size. */
  region->winx = winx;
  region->winy = winy;
  region->winrct.xmin = 0;
  region->winrct.ymin = 0;
  region->winrct.xmax = winx;
  region->winrct.ymax = winy;

  /* There are too many functions inside the draw manager that check the shading type,
   * so use a temporary override instead. */
  v3d->shading.type = drawtype;

  /* Set flags. */
  G.f |= G_FLAG_RENDER_VIEWPORT;

  {
    /* Free images which can have changed on frame-change.
     * WARNING(@ideasman42): can be slow so only free animated images. */
    BKE_image_free_anim_gputextures(G.main);
  }

  if (viewmat) {
    /* WORKAROUND: Disable camera view to avoid EEVEE being confused and try to
     * get the projection matrix from the camera.
     * Set the `lens` parameter to 0 to make EEVEE prefer the `winmat` from the rv3d instead of
     * trying to rederive it. Note that this produces incorrect result with over-scan. */
    rv3d->persp = (winmat[3][3] == 0.0f) ? RV3D_PERSP : RV3D_ORTHO;
    v3d->camera = nullptr;
    v3d->lens = 0.0f;
  }

  GPU_matrix_push_projection();
  GPU_matrix_identity_set();
  GPU_matrix_push();
  GPU_matrix_identity_set();

  if ((viewname != nullptr && viewname[0] != '\0') && (viewmat == nullptr) &&
      rv3d->persp == RV3D_CAMOB && v3d->camera)
  {
    view3d_stereo3d_setup_offscreen(depsgraph, scene, v3d, region, winmat, viewname);
  }
  else {
    view3d_main_region_setup_offscreen(depsgraph, scene, v3d, region, viewmat, winmat);
  }

  if (viewport) {
    GPU_viewport_tag_update(viewport);
  }

  /* main drawing call */
  DRW_draw_render_loop_offscreen(depsgraph,
                                 engine_type,
                                 region,
                                 v3d,
                                 is_image_render,
                                 draw_background,
                                 do_color_management,
                                 ofs,
                                 viewport);
  DRW_cache_free_old_subdiv();
  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  /* Restore all `orig` members. */
  region->winx = orig.region_winx;
  region->winy = orig.region_winy;
  region->winrct = orig.region_winrct;

  /* Optionally do _not_ restore rv3d matrices (e.g. they are used/stored in the ImBuff for
   * reprojection, see texture_paint_image_from_view_exec(). */
  if (restore_rv3d_mats) {
    ED_view3d_mats_rv3d_restore(static_cast<RegionView3D *>(region->regiondata), orig.rv3d_mats);
  }
  MEM_freeN(orig.rv3d_mats);
  rv3d->persp = orig.rv3d_persp;

  UI_Theme_Restore(&orig.theme_state);

  v3d->shading.type = orig.v3d_shading_type;
  v3d->camera = orig.v3d_camera;
  v3d->lens = orig.v3d_lens;

  G.f &= ~G_FLAG_RENDER_VIEWPORT;
}

void ED_view3d_draw_offscreen_simple(Depsgraph *depsgraph,
                                     Scene *scene,
                                     View3DShading *shading_override,
                                     eDrawType drawtype,
                                     int object_type_exclude_viewport_override,
                                     int object_type_exclude_select_override,
                                     int winx,
                                     int winy,
                                     uint draw_flags,
                                     const float viewmat[4][4],
                                     const float winmat[4][4],
                                     float clip_start,
                                     float clip_end,
                                     float vignette_aperture,
                                     bool is_xr_surface,
                                     bool is_image_render,
                                     bool draw_background,
                                     const char *viewname,
                                     const bool do_color_management,
                                     GPUOffScreen *ofs,
                                     GPUViewport *viewport)
{
  View3D v3d = blender::dna::shallow_zero_initialize();
  ARegion ar = {nullptr};
  blender::bke::ARegionRuntime region_runtime{};
  ar.runtime = &region_runtime;
  RegionView3D rv3d = {{{0}}};

  v3d.regionbase.first = v3d.regionbase.last = &ar;
  ar.regiondata = &rv3d;
  ar.regiontype = RGN_TYPE_WINDOW;

  View3DShading *source_shading_settings = &scene->display.shading;
  if (draw_flags & V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS && shading_override != nullptr) {
    source_shading_settings = shading_override;
  }
  memcpy(&v3d.shading, source_shading_settings, sizeof(View3DShading));
  v3d.shading.type = drawtype;

  if (shading_override) {
    /* Pass. */
  }
  else if (drawtype == OB_MATERIAL) {
    v3d.shading.flag = V3D_SHADING_SCENE_WORLD | V3D_SHADING_SCENE_LIGHTS;
  }

  if ((draw_flags & ~V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS) == V3D_OFSDRAW_NONE) {
    v3d.flag2 = V3D_HIDE_OVERLAYS;
  }
  else {
    if (draw_flags & V3D_OFSDRAW_SHOW_ANNOTATION) {
      v3d.flag2 |= V3D_SHOW_ANNOTATION;
    }
    if (draw_flags & V3D_OFSDRAW_SHOW_GRIDFLOOR) {
      v3d.gridflag |= V3D_SHOW_FLOOR | V3D_SHOW_X | V3D_SHOW_Y;
      v3d.grid = 1.0f;
      v3d.gridlines = 16;
      v3d.gridsubdiv = 10;
    }
    if (draw_flags & V3D_OFSDRAW_SHOW_SELECTION) {
      v3d.flag |= V3D_SELECT_OUTLINE;
    }
    if (draw_flags & V3D_OFSDRAW_XR_SHOW_CONTROLLERS) {
      v3d.flag2 |= V3D_XR_SHOW_CONTROLLERS;
    }
    if (draw_flags & V3D_OFSDRAW_XR_SHOW_CUSTOM_OVERLAYS) {
      v3d.flag2 |= V3D_XR_SHOW_CUSTOM_OVERLAYS;
    }
    if (draw_flags & V3D_OFSDRAW_XR_SHOW_PASSTHROUGH) {
      v3d.flag2 |= V3D_XR_SHOW_PASSTHROUGH;
    }
    /* Disable other overlays (set all available _HIDE_ flags). */
    v3d.overlay.flag |= V3D_OVERLAY_HIDE_CURSOR | V3D_OVERLAY_HIDE_TEXT |
                        V3D_OVERLAY_HIDE_MOTION_PATHS | V3D_OVERLAY_HIDE_OBJECT_ORIGINS;
    if ((draw_flags & V3D_OFSDRAW_SHOW_OBJECT_EXTRAS) == 0) {
      v3d.overlay.flag |= V3D_OVERLAY_HIDE_OBJECT_XTRAS;
    }
    if ((object_type_exclude_viewport_override & (1 << OB_ARMATURE)) != 0) {
      v3d.overlay.flag |= V3D_OVERLAY_HIDE_BONES;
    }
    v3d.flag |= V3D_HIDE_HELPLINES;
  }

  if (is_xr_surface) {
    v3d.flag |= V3D_XR_SESSION_SURFACE;
  }

  v3d.object_type_exclude_viewport = object_type_exclude_viewport_override;
  v3d.object_type_exclude_select = object_type_exclude_select_override;

  rv3d.persp = RV3D_PERSP;
  v3d.clip_start = clip_start;
  v3d.clip_end = clip_end;
  /* Actually not used since we pass in the projection matrix. */
  v3d.lens = 0;
  v3d.vignette_aperture = vignette_aperture;

  /* WORKAROUND: Disable overscan because it is not supported for arbitrary input matrices.
   * The proper fix to this would be to support arbitrary matrices in `eevee::Camera::sync()`. */
  float overscan = scene->eevee.overscan;
  scene->eevee.overscan = 0.0f;

  ED_view3d_draw_offscreen(depsgraph,
                           scene,
                           drawtype,
                           &v3d,
                           &ar,
                           winx,
                           winy,
                           viewmat,
                           winmat,
                           is_image_render,
                           draw_background,
                           viewname,
                           do_color_management,
                           true,
                           ofs,
                           viewport);

  /* Restore overscan. */
  scene->eevee.overscan = overscan;
}

ImBuf *ED_view3d_draw_offscreen_imbuf(Depsgraph *depsgraph,
                                      Scene *scene,
                                      eDrawType drawtype,
                                      View3D *v3d,
                                      ARegion *region,
                                      int sizex,
                                      int sizey,
                                      eImBufFlags imbuf_flag,
                                      int alpha_mode,
                                      const char *viewname,
                                      const bool restore_rv3d_mats,
                                      GPUOffScreen *ofs,
                                      GPUViewport *viewport,
                                      /* output vars */
                                      char err_out[256])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const bool draw_sky = (alpha_mode == R_ADDSKY);

  /* view state */
  bool is_ortho = false;
  float winmat[4][4];

  /* Guess format based on output buffer. */
  blender::gpu::TextureFormat desired_format =
      (imbuf_flag & IB_float_data) ? blender::gpu::TextureFormat::SFLOAT_16_16_16_16 :
                                     blender::gpu::TextureFormat::UNORM_8_8_8_8;

  if (ofs && ((GPU_offscreen_width(ofs) != sizex) || (GPU_offscreen_height(ofs) != sizey))) {
    /* If offscreen has already been created, recreate with the same format. */
    desired_format = GPU_offscreen_format(ofs);
    /* sizes differ, can't reuse */
    ofs = nullptr;
  }

  blender::gpu::FrameBuffer *old_fb = GPU_framebuffer_active_get();

  if (old_fb) {
    GPU_framebuffer_restore();
  }

  const bool own_ofs = (ofs == nullptr);
  DRW_gpu_context_enable();

  if (own_ofs) {
    /* bind */
    ofs = GPU_offscreen_create(sizex,
                               sizey,
                               true,
                               desired_format,
                               GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_HOST_READ,
                               false,
                               err_out);
    if (ofs == nullptr) {
      DRW_gpu_context_disable();
      return nullptr;
    }
  }

  GPU_offscreen_bind(ofs, true);

  /* read in pixels & stamp */
  ImBuf *ibuf = IMB_allocImBuf(sizex, sizey, 32, imbuf_flag);

  /* render 3d view */
  if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
    CameraParams params;
    Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);
    const Object *camera_eval = DEG_get_evaluated(depsgraph, camera);

    BKE_camera_params_init(&params);
    /* fallback for non camera objects */
    params.clip_start = v3d->clip_start;
    params.clip_end = v3d->clip_end;
    BKE_camera_params_from_object(&params, camera_eval);
    BKE_camera_multiview_params(&scene->r, &params, camera_eval, viewname);
    BKE_camera_params_compute_viewplane(&params, sizex, sizey, scene->r.xasp, scene->r.yasp);
    BKE_camera_params_compute_matrix(&params);

    is_ortho = params.is_ortho;
    copy_m4_m4(winmat, params.winmat);
  }
  else {
    rctf viewplane;
    float clip_start, clipend;

    is_ortho = ED_view3d_viewplane_get(
        depsgraph, v3d, rv3d, sizex, sizey, &viewplane, &clip_start, &clipend, nullptr);
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

  /* XXX(jbakker): `do_color_management` should be controlled by the caller. Currently when doing a
   * viewport render animation and saving to an 8bit file format, color management would be applied
   * twice. Once here, and once when saving the saving to disk. In this case the Save As Render
   * option cannot be controlled either. But when doing an off-screen render you want to do the
   * color management here.
   *
   * This option was added here to increase the performance for quick view-port preview renders.
   * When using workbench the color differences haven't been reported as a bug. But users also use
   * the viewport rendering to render Eevee scenes. In the later situation the saved colors are
   * totally wrong. */
  const bool do_color_management = (ibuf->float_buffer.data == nullptr);
  ED_view3d_draw_offscreen(depsgraph,
                           scene,
                           drawtype,
                           v3d,
                           region,
                           sizex,
                           sizey,
                           nullptr,
                           winmat,
                           true,
                           draw_sky,
                           viewname,
                           do_color_management,
                           restore_rv3d_mats,
                           ofs,
                           viewport);

  if (ibuf->float_buffer.data) {
    GPU_offscreen_read_color(ofs, GPU_DATA_FLOAT, ibuf->float_buffer.data);
  }
  else if (ibuf->byte_buffer.data) {
    GPU_offscreen_read_color(ofs, GPU_DATA_UBYTE, ibuf->byte_buffer.data);
  }

  /* unbind */
  GPU_offscreen_unbind(ofs, true);

  if (own_ofs) {
    GPU_offscreen_free(ofs);
  }

  DRW_gpu_context_disable();

  if (old_fb) {
    GPU_framebuffer_bind(old_fb);
  }

  if (ibuf->float_buffer.data && ibuf->byte_buffer.data) {
    IMB_byte_from_float(ibuf);
  }

  return ibuf;
}

ImBuf *ED_view3d_draw_offscreen_imbuf_simple(Depsgraph *depsgraph,
                                             Scene *scene,
                                             View3DShading *shading_override,
                                             eDrawType drawtype,
                                             Object *camera,
                                             int width,
                                             int height,
                                             eImBufFlags imbuf_flag,
                                             eV3DOffscreenDrawFlag draw_flags,
                                             int alpha_mode,
                                             const char *viewname,
                                             GPUOffScreen *ofs,
                                             GPUViewport *viewport,
                                             char err_out[256])
{
  View3D v3d = blender::dna::shallow_zero_initialize();
  ARegion region = {nullptr};
  blender::bke::ARegionRuntime region_runtime{};
  region.runtime = &region_runtime;
  RegionView3D rv3d = {{{0}}};

  /* connect data */
  v3d.regionbase.first = v3d.regionbase.last = &region;
  region.regiondata = &rv3d;
  region.regiontype = RGN_TYPE_WINDOW;

  v3d.camera = camera;
  View3DShading *source_shading_settings = &scene->display.shading;
  if (draw_flags & V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS && shading_override != nullptr) {
    source_shading_settings = shading_override;
  }
  memcpy(&v3d.shading, source_shading_settings, sizeof(View3DShading));

  if (drawtype == OB_RENDER) {
    /* Don't use external engines for preview. Fall back to solid instead of Eevee as rendering
     * with Eevee is potentially slow due to compiling shaders and loading textures, and the
     * depsgraph may not have been updated to have all the right geometry attributes. */
    if (!(BKE_scene_uses_blender_eevee(scene) || BKE_scene_uses_blender_workbench(scene))) {
      drawtype = OB_SOLID;
    }
  }

  if (drawtype == OB_MATERIAL) {
    v3d.shading.flag = V3D_SHADING_SCENE_WORLD | V3D_SHADING_SCENE_LIGHTS;
    v3d.shading.render_pass = SCE_PASS_COMBINED;
  }
  else if (drawtype == OB_RENDER) {
    v3d.shading.flag = V3D_SHADING_SCENE_WORLD_RENDER | V3D_SHADING_SCENE_LIGHTS_RENDER;
    v3d.shading.render_pass = SCE_PASS_COMBINED;
  }
  else if (drawtype == OB_TEXTURE) {
    drawtype = OB_SOLID;
    v3d.shading.light = V3D_LIGHTING_STUDIO;
    v3d.shading.color_type = V3D_SHADING_TEXTURE_COLOR;
  }
  v3d.shading.type = drawtype;

  v3d.flag2 = V3D_HIDE_OVERLAYS;
  /* HACK: When rendering gpencil objects this opacity is used to mix vertex colors in when not in
   * render mode (e.g. in the sequencer). */
  v3d.overlay.gpencil_vertex_paint_opacity = 1.0f;

  /* Also initialize wire-frame properties to the default so it renders properly in sequencer.
   * Should find some way to use the viewport's current opacity and threshold,
   * but this is a start. */
  v3d.overlay.wireframe_opacity = 1.0f;
  v3d.overlay.wireframe_threshold = 0.5f;

  if (draw_flags & V3D_OFSDRAW_SHOW_ANNOTATION) {
    v3d.flag2 |= V3D_SHOW_ANNOTATION;
  }
  if (draw_flags & V3D_OFSDRAW_SHOW_GRIDFLOOR) {
    v3d.gridflag |= V3D_SHOW_FLOOR | V3D_SHOW_X | V3D_SHOW_Y;
  }

  v3d.shading.background_type = V3D_SHADING_BACKGROUND_WORLD;

  rv3d.persp = RV3D_CAMOB;

  copy_m4_m4(rv3d.viewinv, v3d.camera->object_to_world().ptr());
  normalize_m4(rv3d.viewinv);
  invert_m4_m4(rv3d.viewmat, rv3d.viewinv);

  {
    CameraParams params;
    const Object *view_camera_eval = DEG_get_evaluated(
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
                                        eDrawType(v3d.shading.type),
                                        &v3d,
                                        &region,
                                        width,
                                        height,
                                        imbuf_flag,
                                        alpha_mode,
                                        viewname,
                                        true,
                                        ofs,
                                        viewport,
                                        err_out);
}

bool ED_view3d_draw_offscreen_check_nested()
{
  return DRW_draw_in_progress();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport Clipping
 * \{ */

static bool view3d_clipping_test(const float co[3], const float clip[6][4])
{
  if (plane_point_side_v3(clip[0], co) > 0.0f && plane_point_side_v3(clip[1], co) > 0.0f &&
      plane_point_side_v3(clip[2], co) > 0.0f && plane_point_side_v3(clip[3], co) > 0.0f)
  {
    return false;
  }
  return true;
}

bool ED_view3d_clipping_test(const RegionView3D *rv3d, const float co[3], const bool is_local)
{
  return view3d_clipping_test(co, is_local ? rv3d->clip_local : rv3d->clip);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Back-Draw for Selection
 * \{ */

/**
 * \note Only use in object mode.
 */
static void validate_object_select_id(Depsgraph *depsgraph,
                                      const Scene *scene,
                                      ViewLayer *view_layer,
                                      ARegion *region,
                                      View3D *v3d,
                                      Object *obact)
{
  /* TODO: Use a flag in the selection engine itself. */
  if (v3d->runtime.flag & V3D_RUNTIME_DEPTHBUF_OVERRIDDEN) {
    return;
  }
  Object *obact_eval = DEG_get_evaluated(depsgraph, obact);

  BLI_assert(region->regiontype == RGN_TYPE_WINDOW);
  UNUSED_VARS_NDEBUG(region);

  if (obact_eval && (obact_eval->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT) ||
                     BKE_paint_select_face_test(obact_eval)))
  {
    /* do nothing */
  }
  /* texture paint mode sampling */
  else if (obact_eval && (obact_eval->mode & OB_MODE_TEXTURE_PAINT) &&
           (v3d->shading.type > OB_WIRE))
  {
    /* do nothing */
  }
  else if ((obact_eval && (obact_eval->mode & OB_MODE_PARTICLE_EDIT)) && !XRAY_ENABLED(v3d)) {
    /* do nothing */
  }
  else {
    v3d->runtime.flag |= V3D_RUNTIME_DEPTHBUF_OVERRIDDEN;
    return;
  }

  if (obact_eval && ((obact_eval->base_flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) != 0)) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base = BKE_view_layer_base_find(view_layer, obact);
    DRW_select_buffer_context_create(depsgraph, {base}, -1);
  }

  v3d->runtime.flag |= V3D_RUNTIME_DEPTHBUF_OVERRIDDEN;
}

/* Avoid calling this function multiple times in sequence to prevent frequent CPU-GPU
 * synchronization (which can be very slow). */
static void view3d_gpu_read_Z_pixels(GPUViewport *viewport, rcti *rect, void *data)
{
  blender::gpu::Texture *depth_tx = GPU_viewport_depth_texture(viewport);

  blender::gpu::FrameBuffer *depth_read_fb = nullptr;
  GPU_framebuffer_ensure_config(&depth_read_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(depth_tx),
                                    GPU_ATTACHMENT_NONE,
                                });

  GPU_framebuffer_bind(depth_read_fb);
  GPU_framebuffer_read_depth(depth_read_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             GPU_DATA_FLOAT,
                             data);

  GPU_framebuffer_restore();
  GPU_framebuffer_free(depth_read_fb);
}

void ED_view3d_select_id_validate(const ViewContext *vc)
{
  validate_object_select_id(
      vc->depsgraph, vc->scene, vc->view_layer, vc->region, vc->v3d, vc->obact);
}

int ED_view3d_backbuf_sample_size_clamp(ARegion *region, const float dist)
{
  return int(min_ff(ceilf(dist), float(max_ii(region->winx, region->winy))));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Z-Depth Utilities
 * \{ */

void view3d_depths_rect_create(ARegion *region, rcti *rect, ViewDepths *r_d)
{
  /* clamp rect by region */
  rcti r{};
  r.xmin = 0;
  r.xmax = region->winx - 1;
  r.ymin = 0;
  r.ymax = region->winy - 1;

  /* Constrain rect to depth bounds */
  BLI_rcti_isect(&r, rect, rect);

  /* assign values to compare with the ViewDepths */
  int x = rect->xmin;
  int y = rect->ymin;

  int w = BLI_rcti_size_x(rect);
  int h = BLI_rcti_size_y(rect);

  if (w <= 0 || h <= 0) {
    r_d->depths = nullptr;
    return;
  }

  r_d->x = x;
  r_d->y = y;
  r_d->w = w;
  r_d->h = h;

  r_d->depths = MEM_malloc_arrayN<float>(w * h, "View depths Subset");

  {
    GPUViewport *viewport = WM_draw_region_get_viewport(region);
    view3d_gpu_read_Z_pixels(viewport, rect, r_d->depths);
    /* Range is assumed to be this as they are never changed. */
    r_d->depth_range[0] = 0.0;
    r_d->depth_range[1] = 1.0;
  }
}

/* NOTE: with NOUVEAU drivers the #glReadPixels() is very slow. #24339. */
static ViewDepths *view3d_depths_create(ARegion *region)
{
  ViewDepths *d = MEM_callocN<ViewDepths>("ViewDepths");

  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  blender::gpu::Texture *depth_tx = GPU_viewport_depth_texture(viewport);
  d->w = GPU_texture_width(depth_tx);
  d->h = GPU_texture_height(depth_tx);
  d->depths = static_cast<float *>(GPU_texture_read(depth_tx, GPU_DATA_FLOAT, 0));
  /* Assumed to be this as they are never changed. */
  d->depth_range[0] = 0.0;
  d->depth_range[1] = 1.0;

  return d;
}

float view3d_depth_near_ex(ViewDepths *d, int r_xy[2])
{
  /* Convert to float for comparisons. */
  const float near = float(d->depth_range[0]);
  const float far_real = float(d->depth_range[1]);
  float far = far_real;

  const float *depths = d->depths;
  const int depth_num = int(d->w) * int(d->h); /* Cast to avoid short overflow. */

  /* Far is both the starting 'far' value
   * and the closest value found. */
  if (r_xy != nullptr) {
    int index_found = -1;
    for (int i = 0; i < depth_num; i++) {
      const float depth = *depths++;
      if ((depth < far) && (depth > near)) {
        far = depth;
        index_found = i;
      }
    }
    if (index_found != -1) {
      r_xy[0] = d->x + (index_found % int(d->w));
      r_xy[1] = d->y + (index_found / int(d->w));
    }
  }
  else {
    for (int i = 0; i < depth_num; i++) {
      const float depth = depths[i];
      if ((depth < far) && (depth > near)) {
        far = depth;
      }
    }
  }

  return far == far_real ? FLT_MAX : far;
}

float view3d_depth_near(ViewDepths *d)
{
  return view3d_depth_near_ex(d, nullptr);
}

void ED_view3d_depth_override(Depsgraph *depsgraph,
                              ARegion *region,
                              View3D *v3d,
                              Object * /* obact */,
                              eV3DDepthOverrideMode mode,
                              bool use_overlay,
                              ViewDepths **r_depths)
{
  if (v3d->runtime.flag & V3D_RUNTIME_DEPTHBUF_OVERRIDDEN) {
    /* Force redraw if `r_depths` is required. */
    if (!r_depths || *r_depths != nullptr) {
      return;
    }
  }
  bThemeState theme_state;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  short flag = v3d->flag;
  int flag2 = v3d->flag2;
  /* Setting these temporarily is not nice */
  v3d->flag &= ~V3D_SELECT_OUTLINE;

  if (v3d->flag2 & V3D_HIDE_OVERLAYS) {
    use_overlay = false;
  }

  if (!use_overlay) {
    v3d->flag2 |= V3D_HIDE_OVERLAYS;
  }

  /* Tools may request depth outside of regular drawing code. */
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  ED_view3d_draw_setup_view(static_cast<wmWindowManager *>(G_MAIN->wm.first),
                            nullptr,
                            depsgraph,
                            scene,
                            region,
                            v3d,
                            nullptr,
                            nullptr,
                            nullptr);

  /* get surface depth without bias */
  rv3d->rflag |= RV3D_ZOFFSET_DISABLED;

  /* Needed in cases the 3D Viewport isn't already setup. */
  WM_draw_region_viewport_ensure(scene, region, SPACE_VIEW3D);
  WM_draw_region_viewport_bind(region);

  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  /* When Blender is starting, a click event can trigger a depth test while the viewport is not
   * yet available. */
  if (viewport != nullptr) {
    switch (mode) {
      case V3D_DEPTH_ALL:
        DRW_draw_depth_loop(depsgraph, region, v3d, viewport, true, false, false);
        break;
      case V3D_DEPTH_NO_GPENCIL:
        DRW_draw_depth_loop(depsgraph, region, v3d, viewport, false, false, false);
        break;
      case V3D_DEPTH_GPENCIL_ONLY:
        DRW_draw_depth_loop(depsgraph, region, v3d, viewport, true, false, false);
        break;
      case V3D_DEPTH_OBJECT_ONLY:
        DRW_draw_depth_loop(depsgraph, region, v3d, viewport, false, false, true);
        break;
      case V3D_DEPTH_SELECTED_ONLY:
        DRW_draw_depth_loop(depsgraph, region, v3d, viewport, false, true, false);
        break;
    }

    if (r_depths) {
      if (*r_depths) {
        ED_view3d_depths_free(*r_depths);
      }
      *r_depths = view3d_depths_create(region);
    }
  }

  WM_draw_region_viewport_unbind(region);

  rv3d->rflag &= ~RV3D_ZOFFSET_DISABLED;

  /* Restore. */
  v3d->flag = flag;
  v3d->flag2 = flag2;
  v3d->runtime.flag |= V3D_RUNTIME_DEPTHBUF_OVERRIDDEN;

  UI_Theme_Restore(&theme_state);
}

void ED_view3d_depths_free(ViewDepths *depths)
{
  if (depths->depths) {
    MEM_freeN(depths->depths);
  }
  MEM_freeN(depths);
}

bool ED_view3d_has_depth_buffer_updated(const Depsgraph *depsgraph, const View3D *v3d)
{
#ifdef REUSE_DEPTH_BUFFER
  /* Check if the depth buffer was drawn by any engine and thus can be reused.
   *
   * The idea is good, but it is too error prone.
   * Even when updated by an engine, the depth buffer can still be cleared by drawing callbacks and
   * by the GPU_select API used by gizmos.
   * Check #GPU_clear_depth to track when the depth buffer is cleared. */
  const char *engine_name = DEG_get_evaluated_scene(depsgraph)->r.engine;
  RenderEngineType *engine_type = RE_engines_find(engine_name);

  bool is_viewport_wire_no_xray = v3d->shading.type < OB_SOLID && !XRAY_ENABLED(v3d);
  bool is_viewport_preview_solid = v3d->shading.type == OB_SOLID;
  bool is_viewport_preview_material = v3d->shading.type == OB_MATERIAL;
  bool is_viewport_render_eevee = v3d->shading.type == OB_RENDER &&
                                  (STREQ(engine_name, RE_engine_id_BLENDER_EEVEE));
  bool is_viewport_render_workbench = v3d->shading.type == OB_RENDER &&
                                      STREQ(engine_name, RE_engine_id_BLENDER_WORKBENCH);
  bool is_viewport_render_external_with_overlay = v3d->shading.type == OB_RENDER &&
                                                  !(engine_type->flag & RE_INTERNAL) &&
                                                  !(v3d->flag2 & V3D_HIDE_OVERLAYS);

  return is_viewport_preview_solid || is_viewport_preview_material || is_viewport_wire_no_xray ||
         is_viewport_render_eevee || is_viewport_render_workbench ||
         is_viewport_render_external_with_overlay;
#else
  UNUSED_VARS(depsgraph, v3d);
  return false;
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom-data Utilities
 * \{ */

void ED_view3d_datamask(const Scene *scene,
                        ViewLayer *view_layer,
                        const View3D *v3d,
                        CustomData_MeshMasks *r_cddata_masks)
{
  /* NOTE(@ideasman42): as this function runs continuously while idle
   * (from #wm_event_do_depsgraph) take care to avoid expensive lookups.
   * While they won't hurt performance noticeably, they will increase CPU usage while idle. */
  if (ELEM(v3d->shading.type, OB_TEXTURE, OB_MATERIAL, OB_RENDER)) {
    r_cddata_masks->lmask |= CD_MASK_PROP_FLOAT2 | CD_MASK_PROP_BYTE_COLOR;
    r_cddata_masks->vmask |= CD_MASK_ORCO | CD_MASK_PROP_COLOR;
  }
  else if (v3d->shading.type == OB_SOLID) {
    if (v3d->shading.color_type == V3D_SHADING_TEXTURE_COLOR) {
      r_cddata_masks->lmask |= CD_MASK_PROP_FLOAT2;
    }
    if (v3d->shading.color_type == V3D_SHADING_VERTEX_COLOR) {
      r_cddata_masks->lmask |= CD_MASK_PROP_BYTE_COLOR;
      r_cddata_masks->vmask |= CD_MASK_ORCO | CD_MASK_PROP_COLOR;
    }
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact) {
    switch (obact->type) {
      case OB_MESH: {
        switch (obact->mode) {
          case OB_MODE_EDIT: {
            if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_WEIGHT) {
              r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
            }
            break;
          }
        }
        break;
      }
    }
  }
}

void ED_view3d_screen_datamask(const Scene *scene,
                               ViewLayer *view_layer,
                               const bScreen *screen,
                               CustomData_MeshMasks *r_cddata_masks)
{
  CustomData_MeshMasks_update(r_cddata_masks, &CD_MASK_BAREMESH);

  /* Check if we need UV or color data due to the view mode. */
  LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
    if (area->spacetype == SPACE_VIEW3D) {
      ED_view3d_datamask(
          scene, view_layer, static_cast<View3D *>(area->spacedata.first), r_cddata_masks);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region View Matrix Backup/Restore
 * \{ */

/**
 * Store values from #RegionView3D, set when drawing.
 * This is needed when we draw with to a viewport using a different matrix
 * (offscreen drawing for example).
 *
 * Values set by #ED_view3d_update_viewmat should be handled here.
 */
struct RV3DMatrixStore {
  float winmat[4][4];
  float viewmat[4][4];
  float viewinv[4][4];
  float persmat[4][4];
  float persinv[4][4];
  float viewcamtexcofac[4];
  float pixsize;
};

RV3DMatrixStore *ED_view3d_mats_rv3d_backup(RegionView3D *rv3d)
{
  RV3DMatrixStore *rv3dmat = static_cast<RV3DMatrixStore *>(
      MEM_mallocN(sizeof(*rv3dmat), __func__));
  copy_m4_m4(rv3dmat->winmat, rv3d->winmat);
  copy_m4_m4(rv3dmat->viewmat, rv3d->viewmat);
  copy_m4_m4(rv3dmat->persmat, rv3d->persmat);
  copy_m4_m4(rv3dmat->persinv, rv3d->persinv);
  copy_m4_m4(rv3dmat->viewinv, rv3d->viewinv);
  copy_v4_v4(rv3dmat->viewcamtexcofac, rv3d->viewcamtexcofac);
  rv3dmat->pixsize = rv3d->pixsize;
  return rv3dmat;
}

void ED_view3d_mats_rv3d_restore(RegionView3D *rv3d, RV3DMatrixStore *rv3dmat_pt)
{
  RV3DMatrixStore *rv3dmat = rv3dmat_pt;
  copy_m4_m4(rv3d->winmat, rv3dmat->winmat);
  copy_m4_m4(rv3d->viewmat, rv3dmat->viewmat);
  copy_m4_m4(rv3d->persmat, rv3dmat->persmat);
  copy_m4_m4(rv3d->persinv, rv3dmat->persinv);
  copy_m4_m4(rv3d->viewinv, rv3dmat->viewinv);
  copy_v4_v4(rv3d->viewcamtexcofac, rv3dmat->viewcamtexcofac);
  rv3d->pixsize = rv3dmat->pixsize;
}

void ED_view3D_mats_rv3d_free(RV3DMatrixStore *rv3d_mat)
{
  MEM_freeN(rv3d_mat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FPS Drawing
 * \{ */

void ED_scene_draw_fps(const Scene *scene, int xoffset, int *yoffset)
{
  *yoffset -= VIEW3D_OVERLAY_LINEHEIGHT;

  SceneFPS_State state;
  if (!ED_scene_fps_average_calc(scene, &state)) {
    return;
  }

  /* 8 4-bytes chars (complex writing systems like Devanagari in UTF8 encoding) */
  char printable[32];
  printable[0] = '\0';

  bool show_fractional = state.fps_target_is_fractional;

  const int font_id = BLF_default();

  /* Is this more than half a frame behind? */
  if (state.fps_average + 0.5f < state.fps_target) {
    /* Always show fractional when under performing. */
    show_fractional = true;
    float alert_rgb[4];
    float alert_hsv[4];
    UI_GetThemeColor4fv(TH_REDALERT, alert_rgb);
    /* Brighten since we favor dark shadows to increase contrast.
     * This gives similar results to the old hardcoded 225, 36, 36. */
    rgb_to_hsv_v(alert_rgb, alert_hsv);
    alert_hsv[2] = 1.0;
    hsv_to_rgb_v(alert_hsv, alert_rgb);
    BLF_color4fv(font_id, alert_rgb);
  }

  if (show_fractional) {
    SNPRINTF_UTF8(printable, IFACE_("fps: %.2f"), state.fps_average);
  }
  else {
    SNPRINTF_UTF8(printable, IFACE_("fps: %i"), int(state.fps_average + 0.5f));
  }

  BLF_draw_default(xoffset, *yoffset, 0.0f, printable, sizeof(printable));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Render Border
 * \{ */

static bool view3d_main_region_do_render_draw(const Scene *scene)
{
  RenderEngineType *type = RE_engines_find(scene->r.engine);
  return (type && type->view_update && type->view_draw);
}

bool ED_view3d_calc_render_border(
    const Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *region, rcti *r_rect)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  bool use_border;

  /* Test if there is a 3d view rendering. */
  if (v3d->shading.type != OB_RENDER || !view3d_main_region_do_render_draw(scene)) {
    return false;
  }

  /* Test if there is a border render. */
  if (rv3d->persp == RV3D_CAMOB) {
    use_border = (scene->r.mode & R_BORDER) != 0;
  }
  else {
    use_border = (v3d->flag2 & V3D_RENDER_BORDER) != 0;
  }

  if (!use_border) {
    return false;
  }

  /* Compute border. */
  if (rv3d->persp == RV3D_CAMOB) {
    rctf viewborder;
    ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, false, &viewborder);

    r_rect->xmin = viewborder.xmin + scene->r.border.xmin * BLI_rctf_size_x(&viewborder);
    r_rect->ymin = viewborder.ymin + scene->r.border.ymin * BLI_rctf_size_y(&viewborder);
    r_rect->xmax = viewborder.xmin + scene->r.border.xmax * BLI_rctf_size_x(&viewborder);
    r_rect->ymax = viewborder.ymin + scene->r.border.ymax * BLI_rctf_size_y(&viewborder);
  }
  else {
    r_rect->xmin = v3d->render_border.xmin * region->winx;
    r_rect->xmax = v3d->render_border.xmax * region->winx;
    r_rect->ymin = v3d->render_border.ymin * region->winy;
    r_rect->ymax = v3d->render_border.ymax * region->winy;
  }

  BLI_rcti_translate(r_rect, region->winrct.xmin, region->winrct.ymin);
  BLI_rcti_isect(&region->winrct, r_rect, r_rect);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport color picker
 * \{ */

bool ViewportColorSampleSession::init(ARegion *region)
{
  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  if (viewport == nullptr) {
    return false;
  }

  blender::gpu::Texture *color_tex = GPU_viewport_color_texture(viewport, 0);
  if (color_tex == nullptr) {
    return false;
  }

  tex_w = GPU_texture_width(color_tex);
  tex_h = GPU_texture_height(color_tex);
  BLI_rcti_init(&valid_rect, 0, min_ii(region->winx, tex_w) - 1, 0, min_ii(region->winy, tex_h));

  /* Copying pixels from textures only works when HOST_READ usage is enabled on them.
   * However, doing so can have performance impact, which we don't want for the viewport.
   * So, instead allocate a separate texture with HOST_READ here, copy to it, and then
   * copy that back to the host.
   * Since color picking is a fairly rare operation, the inefficiency here doesn't really
   * matter, and it means the viewport doesn't need HOST_READ. */
  tex = GPU_texture_create_2d("copy_tex",
                              tex_w,
                              tex_h,
                              1,
                              blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
                              GPU_TEXTURE_USAGE_HOST_READ,
                              nullptr);
  if (tex == nullptr) {
    return false;
  }

  GPU_texture_copy(tex, color_tex);
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  data = static_cast<blender::ushort4 *>(GPU_texture_read(tex, GPU_DATA_HALF_FLOAT, 0));

  return true;
}

bool ViewportColorSampleSession::sample(const int mval[2], float r_col[3])
{
  if (tex == nullptr || data == nullptr) {
    return false;
  }

  if (!BLI_rcti_isect_pt_v(&valid_rect, mval)) {
    return false;
  }

  blender::ushort4 pixel = data[mval[1] * tex_w + mval[0]];

  if (blender::math::half_to_float(pixel.w) < 0.5f) {
    /* Background etc. are not rendered to the viewport texture, so fall back to basic color
     * picking for those. */
    return false;
  }

  r_col[0] = blender::math::half_to_float(pixel.x);
  r_col[1] = blender::math::half_to_float(pixel.y);
  r_col[2] = blender::math::half_to_float(pixel.z);

  return true;
}

ViewportColorSampleSession::~ViewportColorSampleSession()
{
  if (data != nullptr) {
    MEM_freeN(data);
  }
  if (tex != nullptr) {
    GPU_texture_free(tex);
  }
}

/** \} */
