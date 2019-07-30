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

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_threads.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_unit.h"
#include "BKE_movieclip.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_screen_types.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "GPU_draw.h"
#include "GPU_framebuffer.h"
#include "GPU_material.h"
#include "GPU_extensions.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_viewport.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "view3d_intern.h" /* own include */

/* ********* custom clipping *********** */

/* Legacy 2.7x, now use shaders that use clip distance instead.
 * Remove once clipping is working properly. */
#define USE_CLIP_PLANES

void ED_view3d_clipping_set(RegionView3D *rv3d)
{
#ifdef USE_CLIP_PLANES
  double plane[4];
  const uint tot = (rv3d->viewlock & RV3D_BOXCLIP) ? 4 : 6;

  for (unsigned a = 0; a < tot; a++) {
    copy_v4db_v4fl(plane, rv3d->clip[a]);
    glClipPlane(GL_CLIP_PLANE0 + a, plane);
    glEnable(GL_CLIP_PLANE0 + a);
    glEnable(GL_CLIP_DISTANCE0 + a);
  }
#else
  for (unsigned a = 0; a < 6; a++) {
    glEnable(GL_CLIP_DISTANCE0 + a);
  }
#endif
}

/* use these to temp disable/enable clipping when 'rv3d->rflag & RV3D_CLIPPING' is set */
void ED_view3d_clipping_disable(void)
{
  for (unsigned a = 0; a < 6; a++) {
#ifdef USE_CLIP_PLANES
    glDisable(GL_CLIP_PLANE0 + a);
#endif
    glDisable(GL_CLIP_DISTANCE0 + a);
  }
}
void ED_view3d_clipping_enable(void)
{
  for (unsigned a = 0; a < 6; a++) {
#ifdef USE_CLIP_PLANES
    glEnable(GL_CLIP_PLANE0 + a);
#endif
    glEnable(GL_CLIP_DISTANCE0 + a);
  }
}

/* *********************** backdraw for selection *************** */

/**
 * \note Only use in object mode.
 */
static void validate_object_select_id(struct Depsgraph *depsgraph,
                                      Scene *scene,
                                      ViewLayer *view_layer,
                                      ARegion *ar,
                                      View3D *v3d,
                                      Object *obact)
{
  Object *obact_eval = DEG_get_evaluated_object(depsgraph, obact);

  BLI_assert(ar->regiontype == RGN_TYPE_WINDOW);

  if (obact_eval && (obact_eval->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT) ||
                     BKE_paint_select_face_test(obact_eval))) {
    /* do nothing */
  }
  /* texture paint mode sampling */
  else if (obact_eval && (obact_eval->mode & OB_MODE_TEXTURE_PAINT) &&
           (v3d->shading.type > OB_WIRE)) {
    /* do nothing */
  }
  else if ((obact_eval && (obact_eval->mode & OB_MODE_PARTICLE_EDIT)) && !XRAY_ENABLED(v3d)) {
    /* do nothing */
  }
  else {
    v3d->flag &= ~V3D_INVALID_BACKBUF;
    return;
  }

  if (!(v3d->flag & V3D_INVALID_BACKBUF)) {
    return;
  }

  if (obact_eval && ((obact_eval->base_flag & BASE_VISIBLE) != 0)) {
    DRW_draw_select_id_object(
        depsgraph, view_layer, ar, v3d, obact, scene->toolsettings->selectmode);
  }

  /* TODO: Create a flag in `DRW_manager` because the drawing is no longer
   *       made on the backbuffer in this case. */
  v3d->flag &= ~V3D_INVALID_BACKBUF;
}

/* TODO: Creating, attaching texture, and destroying a framebuffer is quite slow.
 *       Calling this function should be avoided during interactive drawing. */
static void view3d_opengl_read_Z_pixels(GPUViewport *viewport, rcti *rect, void *data)
{
  DefaultTextureList *dtxl = (DefaultTextureList *)GPU_viewport_texture_list_get(viewport);

  GPUFrameBuffer *tmp_fb = GPU_framebuffer_create();
  GPU_framebuffer_texture_attach(tmp_fb, dtxl->depth, 0, 0);
  GPU_framebuffer_bind(tmp_fb);

  glReadPixels(rect->xmin,
               rect->ymin,
               BLI_rcti_size_x(rect),
               BLI_rcti_size_y(rect),
               GL_DEPTH_COMPONENT,
               GL_FLOAT,
               data);

  GPU_framebuffer_restore();
  GPU_framebuffer_free(tmp_fb);
}

void ED_view3d_select_id_validate(ViewContext *vc)
{
  /* TODO: Create a flag in `DRW_manager` because the drawing is no longer
   *       made on the backbuffer in this case. */
  if (vc->v3d->flag & V3D_INVALID_BACKBUF) {
    validate_object_select_id(
        vc->depsgraph, vc->scene, vc->view_layer, vc->ar, vc->v3d, vc->obact);
  }
}

void ED_view3d_backbuf_depth_validate(ViewContext *vc)
{
  if (vc->v3d->flag & V3D_INVALID_BACKBUF) {
    ARegion *ar = vc->ar;
    Object *obact_eval = DEG_get_evaluated_object(vc->depsgraph, vc->obact);

    if (obact_eval && ((obact_eval->base_flag & BASE_VISIBLE) != 0)) {
      GPUViewport *viewport = WM_draw_region_get_viewport(ar, 0);
      DRW_draw_depth_object(vc->ar, viewport, obact_eval);
    }

    vc->v3d->flag &= ~V3D_INVALID_BACKBUF;
  }
}

uint *ED_view3d_select_id_read_rect(const rcti *clip, uint *r_buf_len)
{
  uint width = BLI_rcti_size_x(clip);
  uint height = BLI_rcti_size_y(clip);
  uint buf_len = width * height;
  uint *buf = MEM_mallocN(buf_len * sizeof(*buf), __func__);

  DRW_framebuffer_select_id_read(clip, buf);

  if (r_buf_len) {
    *r_buf_len = buf_len;
  }

  return buf;
}

/**
 * allow for small values [0.5 - 2.5],
 * and large values, FLT_MAX by clamping by the area size
 */
int ED_view3d_backbuf_sample_size_clamp(ARegion *ar, const float dist)
{
  return (int)min_ff(ceilf(dist), (float)max_ii(ar->winx, ar->winx));
}

/* reads full rect, converts indices */
uint *ED_view3d_select_id_read(int xmin, int ymin, int xmax, int ymax, uint *r_buf_len)
{
  if (UNLIKELY((xmin > xmax) || (ymin > ymax))) {
    return NULL;
  }

  const rcti rect = {
      .xmin = xmin,
      .xmax = xmax + 1,
      .ymin = ymin,
      .ymax = ymax + 1,
  };

  uint buf_len;
  uint *buf = ED_view3d_select_id_read_rect(&rect, &buf_len);

  if (r_buf_len) {
    *r_buf_len = buf_len;
  }

  return buf;
}

/* *********************** */

void view3d_update_depths_rect(ARegion *ar, ViewDepths *d, rcti *rect)
{
  /* clamp rect by region */
  rcti r = {
      .xmin = 0,
      .xmax = ar->winx - 1,
      .ymin = 0,
      .ymax = ar->winy - 1,
  };

  /* Constrain rect to depth bounds */
  BLI_rcti_isect(&r, rect, rect);

  /* assign values to compare with the ViewDepths */
  int x = rect->xmin;
  int y = rect->ymin;

  int w = BLI_rcti_size_x(rect);
  int h = BLI_rcti_size_y(rect);

  if (w <= 0 || h <= 0) {
    if (d->depths) {
      MEM_freeN(d->depths);
    }
    d->depths = NULL;

    d->damaged = false;
  }
  else if (d->w != w || d->h != h || d->x != x || d->y != y || d->depths == NULL) {
    d->x = x;
    d->y = y;
    d->w = w;
    d->h = h;

    if (d->depths) {
      MEM_freeN(d->depths);
    }

    d->depths = MEM_mallocN(sizeof(float) * d->w * d->h, "View depths Subset");

    d->damaged = true;
  }

  if (d->damaged) {
    GPUViewport *viewport = WM_draw_region_get_viewport(ar, 0);
    view3d_opengl_read_Z_pixels(viewport, rect, d->depths);
    glGetDoublev(GL_DEPTH_RANGE, d->depth_range);
    d->damaged = false;
  }
}

/* note, with nouveau drivers the glReadPixels() is very slow. [#24339] */
void ED_view3d_depth_update(ARegion *ar)
{
  RegionView3D *rv3d = ar->regiondata;

  /* Create storage for, and, if necessary, copy depth buffer */
  if (!rv3d->depths) {
    rv3d->depths = MEM_callocN(sizeof(ViewDepths), "ViewDepths");
  }
  if (rv3d->depths) {
    ViewDepths *d = rv3d->depths;
    if (d->w != ar->winx || d->h != ar->winy || !d->depths) {
      d->w = ar->winx;
      d->h = ar->winy;
      if (d->depths) {
        MEM_freeN(d->depths);
      }
      d->depths = MEM_mallocN(sizeof(float) * d->w * d->h, "View depths");
      d->damaged = true;
    }

    if (d->damaged) {
      GPUViewport *viewport = WM_draw_region_get_viewport(ar, 0);
      rcti r = {
          .xmin = 0,
          .xmax = d->w,
          .ymin = 0,
          .ymax = d->h,
      };
      view3d_opengl_read_Z_pixels(viewport, &r, d->depths);
      glGetDoublev(GL_DEPTH_RANGE, d->depth_range);
      d->damaged = false;
    }
  }
}

/* utility function to find the closest Z value, use for autodepth */
float view3d_depth_near(ViewDepths *d)
{
  /* convert to float for comparisons */
  const float near = (float)d->depth_range[0];
  const float far_real = (float)d->depth_range[1];
  float far = far_real;

  const float *depths = d->depths;
  float depth = FLT_MAX;
  int i = (int)d->w * (int)d->h; /* cast to avoid short overflow */

  /* far is both the starting 'far' value
   * and the closest value found. */
  while (i--) {
    depth = *depths++;
    if ((depth < far) && (depth > near)) {
      far = depth;
    }
  }

  return far == far_real ? FLT_MAX : far;
}

void ED_view3d_draw_depth_gpencil(Depsgraph *depsgraph, Scene *scene, ARegion *ar, View3D *v3d)
{
  /* Setup view matrix. */
  ED_view3d_draw_setup_view(NULL, depsgraph, scene, ar, v3d, NULL, NULL, NULL);

  GPU_clear(GPU_DEPTH_BIT);

  GPU_depth_test(true);

  GPUViewport *viewport = WM_draw_region_get_viewport(ar, 0);
  DRW_draw_depth_loop_gpencil(depsgraph, ar, v3d, viewport);

  GPU_depth_test(false);
}

/* *********************** customdata **************** */

void ED_view3d_datamask(const bContext *C,
                        const Scene *UNUSED(scene),
                        const View3D *v3d,
                        CustomData_MeshMasks *r_cddata_masks)
{
  if (ELEM(v3d->shading.type, OB_TEXTURE, OB_MATERIAL, OB_RENDER)) {
    r_cddata_masks->lmask |= CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL;
    r_cddata_masks->vmask |= CD_MASK_ORCO;
  }

  if ((CTX_data_mode_enum(C) == CTX_MODE_EDIT_MESH) &&
      (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_WEIGHT)) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/* goes over all modes and view3d settings */
void ED_view3d_screen_datamask(const bContext *C,
                               const Scene *scene,
                               const bScreen *screen,
                               CustomData_MeshMasks *r_cddata_masks)
{
  CustomData_MeshMasks_update(r_cddata_masks, &CD_MASK_BAREMESH);

  /* check if we need tfaces & mcols due to view mode */
  for (const ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
    if (sa->spacetype == SPACE_VIEW3D) {
      ED_view3d_datamask(C, scene, sa->spacedata.first, r_cddata_masks);
    }
  }
}

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

struct RV3DMatrixStore *ED_view3d_mats_rv3d_backup(struct RegionView3D *rv3d)
{
  struct RV3DMatrixStore *rv3dmat = MEM_mallocN(sizeof(*rv3dmat), __func__);
  copy_m4_m4(rv3dmat->winmat, rv3d->winmat);
  copy_m4_m4(rv3dmat->viewmat, rv3d->viewmat);
  copy_m4_m4(rv3dmat->persmat, rv3d->persmat);
  copy_m4_m4(rv3dmat->persinv, rv3d->persinv);
  copy_m4_m4(rv3dmat->viewinv, rv3d->viewinv);
  copy_v4_v4(rv3dmat->viewcamtexcofac, rv3d->viewcamtexcofac);
  rv3dmat->pixsize = rv3d->pixsize;
  return (void *)rv3dmat;
}

void ED_view3d_mats_rv3d_restore(struct RegionView3D *rv3d, struct RV3DMatrixStore *rv3dmat_pt)
{
  struct RV3DMatrixStore *rv3dmat = rv3dmat_pt;
  copy_m4_m4(rv3d->winmat, rv3dmat->winmat);
  copy_m4_m4(rv3d->viewmat, rv3dmat->viewmat);
  copy_m4_m4(rv3d->persmat, rv3dmat->persmat);
  copy_m4_m4(rv3d->persinv, rv3dmat->persinv);
  copy_m4_m4(rv3d->viewinv, rv3dmat->viewinv);
  copy_v4_v4(rv3d->viewcamtexcofac, rv3dmat->viewcamtexcofac);
  rv3d->pixsize = rv3dmat->pixsize;
}

/**
 * \note The info that this uses is updated in #ED_refresh_viewport_fps,
 * which currently gets called during #SCREEN_OT_animation_step.
 */
void ED_scene_draw_fps(Scene *scene, int xoffset, int *yoffset)
{
  ScreenFrameRateInfo *fpsi = scene->fps_info;
  char printable[16];

  if (!fpsi || !fpsi->lredrawtime || !fpsi->redrawtime) {
    return;
  }

  printable[0] = '\0';

#if 0
  /* this is too simple, better do an average */
  fps = (float)(1.0 / (fpsi->lredrawtime - fpsi->redrawtime))
#else
  fpsi->redrawtimes_fps[fpsi->redrawtime_index] = (float)(1.0 /
                                                          (fpsi->lredrawtime - fpsi->redrawtime));

  float fps = 0.0f;
  int tot = 0;
  for (int i = 0; i < REDRAW_FRAME_AVERAGE; i++) {
    if (fpsi->redrawtimes_fps[i]) {
      fps += fpsi->redrawtimes_fps[i];
      tot++;
    }
  }
  if (tot) {
    fpsi->redrawtime_index = (fpsi->redrawtime_index + 1) % REDRAW_FRAME_AVERAGE;

    // fpsi->redrawtime_index++;
    // if (fpsi->redrawtime >= REDRAW_FRAME_AVERAGE) {
    //  fpsi->redrawtime = 0;
    //}

    fps = fps / tot;
  }
#endif

  const int font_id = BLF_default();

  /* is this more than half a frame behind? */
  if (fps + 0.5f < (float)(FPS)) {
    UI_FontThemeColor(font_id, TH_REDALERT);
    BLI_snprintf(printable, sizeof(printable), IFACE_("fps: %.2f"), fps);
  }
  else {
    UI_FontThemeColor(font_id, TH_TEXT_HI);
    BLI_snprintf(printable, sizeof(printable), IFACE_("fps: %i"), (int)(fps + 0.5f));
  }

  BLF_enable(font_id, BLF_SHADOW);
  BLF_shadow(font_id, 5, (const float[4]){0.0f, 0.0f, 0.0f, 1.0f});
  BLF_shadow_offset(font_id, 1, -1);

  *yoffset -= U.widget_unit;

#ifdef WITH_INTERNATIONAL
  BLF_draw_default(xoffset, *yoffset, 0.0f, printable, sizeof(printable));
#else
  BLF_draw_default_ascii(xoffset, *yoffset, 0.0f, printable, sizeof(printable));
#endif

  BLF_disable(font_id, BLF_SHADOW);
}

static bool view3d_main_region_do_render_draw(const Scene *scene)
{
  RenderEngineType *type = RE_engines_find(scene->r.engine);
  return (type && type->view_update && type->view_draw);
}

bool ED_view3d_calc_render_border(
    const Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *ar, rcti *rect)
{
  RegionView3D *rv3d = ar->regiondata;
  bool use_border;

  /* test if there is a 3d view rendering */
  if (v3d->shading.type != OB_RENDER || !view3d_main_region_do_render_draw(scene)) {
    return false;
  }

  /* test if there is a border render */
  if (rv3d->persp == RV3D_CAMOB) {
    use_border = (scene->r.mode & R_BORDER) != 0;
  }
  else {
    use_border = (v3d->flag2 & V3D_RENDER_BORDER) != 0;
  }

  if (!use_border) {
    return false;
  }

  /* compute border */
  if (rv3d->persp == RV3D_CAMOB) {
    rctf viewborder;
    ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewborder, false);

    rect->xmin = viewborder.xmin + scene->r.border.xmin * BLI_rctf_size_x(&viewborder);
    rect->ymin = viewborder.ymin + scene->r.border.ymin * BLI_rctf_size_y(&viewborder);
    rect->xmax = viewborder.xmin + scene->r.border.xmax * BLI_rctf_size_x(&viewborder);
    rect->ymax = viewborder.ymin + scene->r.border.ymax * BLI_rctf_size_y(&viewborder);
  }
  else {
    rect->xmin = v3d->render_border.xmin * ar->winx;
    rect->xmax = v3d->render_border.xmax * ar->winx;
    rect->ymin = v3d->render_border.ymin * ar->winy;
    rect->ymax = v3d->render_border.ymax * ar->winy;
  }

  BLI_rcti_translate(rect, ar->winrct.xmin, ar->winrct.ymin);
  BLI_rcti_isect(&ar->winrct, rect, rect);

  return true;
}
