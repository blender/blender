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
 * \ingroup edrend
 */

#include <math.h>
#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_timecode.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_blender_undo.h"
#include "BKE_blender_version.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "BIF_glutil.h"

#include "RE_pipeline.h"
#include "RE_engine.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "GPU_shader.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLO_undofile.h"

#include "render_intern.h"

/* Render Callbacks */
static int render_break(void *rjv);

typedef struct RenderJob {
  Main *main;
  Scene *scene;
  ViewLayer *single_layer;
  Scene *current_scene;
  /* TODO(sergey): Should not be needed once engine will have own
   * depsgraph and copy-on-write will be implemented.
   */
  Depsgraph *depsgraph;
  Render *re;
  struct Object *camera_override;
  bool v3d_override;
  bool anim, write_still;
  Image *image;
  ImageUser iuser;
  bool image_outdated;
  short *stop;
  short *do_update;
  float *progress;
  ReportList *reports;
  int orig_layer;
  int last_layer;
  ScrArea *sa;
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  bool supports_glsl_draw;
  bool interface_locked;
} RenderJob;

/* called inside thread! */
static void image_buffer_rect_update(RenderJob *rj,
                                     RenderResult *rr,
                                     ImBuf *ibuf,
                                     ImageUser *iuser,
                                     volatile rcti *renrect,
                                     const char *viewname)
{
  Scene *scene = rj->scene;
  const float *rectf = NULL;
  int ymin, ymax, xmin, xmax;
  int rymin, rxmin;
  int linear_stride, linear_offset_x, linear_offset_y;
  ColorManagedViewSettings *view_settings;
  ColorManagedDisplaySettings *display_settings;

  /* Exception for exr tiles -- display buffer conversion happens here,
   * NOT in the color management pipeline.
   */
  if (ibuf->userflags & IB_DISPLAY_BUFFER_INVALID && rr->do_exr_tile == false) {
    /* The whole image buffer it so be color managed again anyway. */
    return;
  }

  /* if renrect argument, we only refresh scanlines */
  if (renrect) {
    /* if (ymax == recty), rendering of layer is ready,
     * we should not draw, other things happen... */
    if (rr->renlay == NULL || renrect->ymax >= rr->recty) {
      return;
    }

    /* xmin here is first subrect x coord, xmax defines subrect width */
    xmin = renrect->xmin + rr->crop;
    xmax = renrect->xmax - xmin + rr->crop;
    if (xmax < 2) {
      return;
    }

    ymin = renrect->ymin + rr->crop;
    ymax = renrect->ymax - ymin + rr->crop;
    if (ymax < 2) {
      return;
    }
    renrect->ymin = renrect->ymax;
  }
  else {
    xmin = ymin = rr->crop;
    xmax = rr->rectx - 2 * rr->crop;
    ymax = rr->recty - 2 * rr->crop;
  }

  /* xmin ymin is in tile coords. transform to ibuf */
  rxmin = rr->tilerect.xmin + xmin;
  if (rxmin >= ibuf->x) {
    return;
  }
  rymin = rr->tilerect.ymin + ymin;
  if (rymin >= ibuf->y) {
    return;
  }

  if (rxmin + xmax > ibuf->x) {
    xmax = ibuf->x - rxmin;
  }
  if (rymin + ymax > ibuf->y) {
    ymax = ibuf->y - rymin;
  }

  if (xmax < 1 || ymax < 1) {
    return;
  }

  /* The thing here is, the logic below (which was default behavior
   * of how rectf is acquiring since forever) gives float buffer for
   * composite output only. This buffer can not be used for other
   * passes obviously.
   *
   * We might try finding corresponding for pass buffer in render result
   * (which is actually missing when rendering with Cycles, who only
   * writes all the passes when the tile is finished) or use float
   * buffer from image buffer as reference, which is easier to use and
   * contains all the data we need anyway.
   *                                              - sergey -
   */
  /* TODO(sergey): Need to check has_combined here? */
  if (iuser->pass == 0) {
    RenderView *rv;
    const int view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);
    rv = RE_RenderViewGetById(rr, view_id);

    /* find current float rect for display, first case is after composite... still weak */
    if (rv->rectf) {
      rectf = rv->rectf;
    }
    else {
      if (rv->rect32) {
        /* special case, currently only happens with sequencer rendering,
         * which updates the whole frame, so we can only mark display buffer
         * as invalid here (sergey)
         */
        ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
        return;
      }
      else {
        if (rr->renlay == NULL) {
          return;
        }
        rectf = RE_RenderLayerGetPass(rr->renlay, RE_PASSNAME_COMBINED, viewname);
      }
    }
    if (rectf == NULL) {
      return;
    }

    rectf += 4 * (rr->rectx * ymin + xmin);
    linear_stride = rr->rectx;
    linear_offset_x = rxmin;
    linear_offset_y = rymin;
  }
  else {
    rectf = ibuf->rect_float;
    linear_stride = ibuf->x;
    linear_offset_x = 0;
    linear_offset_y = 0;
  }

  view_settings = &scene->view_settings;
  display_settings = &scene->display_settings;

  IMB_partial_display_buffer_update(ibuf,
                                    rectf,
                                    NULL,
                                    linear_stride,
                                    linear_offset_x,
                                    linear_offset_y,
                                    view_settings,
                                    display_settings,
                                    rxmin,
                                    rymin,
                                    rxmin + xmax,
                                    rymin + ymax);
}

/* ****************************** render invoking ***************** */

/* set callbacks, exported to sequence render too.
 * Only call in foreground (UI) renders. */

static void screen_render_single_layer_set(
    wmOperator *op, Main *mainp, ViewLayer *active_layer, Scene **scene, ViewLayer **single_layer)
{
  /* single layer re-render */
  if (RNA_struct_property_is_set(op->ptr, "scene")) {
    Scene *scn;
    char scene_name[MAX_ID_NAME - 2];

    RNA_string_get(op->ptr, "scene", scene_name);
    scn = (Scene *)BLI_findstring(&mainp->scenes, scene_name, offsetof(ID, name) + 2);

    if (scn) {
      /* camera switch wont have updated */
      scn->r.cfra = (*scene)->r.cfra;
      BKE_scene_camera_switch_update(scn);

      *scene = scn;
    }
  }

  if (RNA_struct_property_is_set(op->ptr, "layer")) {
    ViewLayer *rl;
    char rl_name[RE_MAXNAME];

    RNA_string_get(op->ptr, "layer", rl_name);
    rl = (ViewLayer *)BLI_findstring(&(*scene)->view_layers, rl_name, offsetof(ViewLayer, name));

    if (rl) {
      *single_layer = rl;
    }
  }
  else if (((*scene)->r.scemode & R_SINGLE_LAYER) && active_layer) {
    *single_layer = active_layer;
  }
}

/* executes blocking render */
static int screen_render_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  RenderEngineType *re_type = RE_engines_find(scene->r.engine);
  ViewLayer *active_layer = CTX_data_view_layer(C);
  ViewLayer *single_layer = NULL;
  Render *re;
  Image *ima;
  View3D *v3d = CTX_wm_view3d(C);
  Main *mainp = CTX_data_main(C);
  const bool is_animation = RNA_boolean_get(op->ptr, "animation");
  const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
  struct Object *camera_override = v3d ? V3D_CAMERA_LOCAL(v3d) : NULL;

  /* Cannot do render if there is not this function. */
  if (re_type->render == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* custom scene and single layer re-render */
  screen_render_single_layer_set(op, mainp, active_layer, &scene, &single_layer);

  if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
    return OPERATOR_CANCELLED;
  }

  re = RE_NewSceneRender(scene);

  G.is_break = false;
  RE_test_break_cb(re, NULL, render_break);

  ima = BKE_image_verify_viewer(mainp, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_signal(mainp, ima, NULL, IMA_SIGNAL_FREE);
  BKE_image_backup_render(scene, ima, true);

  /* cleanup sequencer caches before starting user triggered render.
   * otherwise, invalidated cache entries can make their way into
   * the output rendering. We can't put that into RE_RenderFrame,
   * since sequence rendering can call that recursively... (peter) */
  BKE_sequencer_cache_cleanup(scene);

  RE_SetReports(re, op->reports);

  BLI_threaded_malloc_begin();
  if (is_animation) {
    RE_RenderAnim(re,
                  mainp,
                  scene,
                  single_layer,
                  camera_override,
                  scene->r.sfra,
                  scene->r.efra,
                  scene->r.frame_step);
  }
  else {
    RE_RenderFrame(re, mainp, scene, single_layer, camera_override, scene->r.cfra, is_write_still);
  }
  BLI_threaded_malloc_end();

  RE_SetReports(re, NULL);

  // no redraw needed, we leave state as we entered it
  ED_update_for_newframe(mainp, CTX_data_depsgraph(C));

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  return OPERATOR_FINISHED;
}

static void render_freejob(void *rjv)
{
  RenderJob *rj = rjv;

  BKE_color_managed_view_settings_free(&rj->view_settings);
  MEM_freeN(rj);
}

/* str is IMA_MAX_RENDER_TEXT in size */
static void make_renderinfo_string(const RenderStats *rs,
                                   const Scene *scene,
                                   const bool v3d_override,
                                   const char *error,
                                   char *str)
{
  char info_time_str[32];  // used to be extern to header_info.c
  uintptr_t mem_in_use, mmap_in_use, peak_memory;
  float megs_used_memory, mmap_used_memory, megs_peak_memory;
  char *spos = str;

  mem_in_use = MEM_get_memory_in_use();
  mmap_in_use = MEM_get_mapped_memory_in_use();
  peak_memory = MEM_get_peak_memory();

  megs_used_memory = (mem_in_use - mmap_in_use) / (1024.0 * 1024.0);
  mmap_used_memory = (mmap_in_use) / (1024.0 * 1024.0);
  megs_peak_memory = (peak_memory) / (1024.0 * 1024.0);

  /* local view */
  if (rs->localview) {
    spos += sprintf(spos, "%s | ", TIP_("3D Local View"));
  }
  else if (v3d_override) {
    spos += sprintf(spos, "%s | ", TIP_("3D View"));
  }

  /* frame number */
  spos += sprintf(spos, TIP_("Frame:%d "), (scene->r.cfra));

  /* previous and elapsed time */
  BLI_timecode_string_from_time_simple(info_time_str, sizeof(info_time_str), rs->lastframetime);

  if (rs->infostr && rs->infostr[0]) {
    if (rs->lastframetime != 0.0) {
      spos += sprintf(spos, TIP_("| Last:%s "), info_time_str);
    }
    else {
      spos += sprintf(spos, "| ");
    }

    BLI_timecode_string_from_time_simple(
        info_time_str, sizeof(info_time_str), PIL_check_seconds_timer() - rs->starttime);
  }
  else {
    spos += sprintf(spos, "| ");
  }

  spos += sprintf(spos, TIP_("Time:%s "), info_time_str);

  /* statistics */
  if (rs->statstr) {
    if (rs->statstr[0]) {
      spos += sprintf(spos, "| %s ", rs->statstr);
    }
  }
  else {
    if (rs->totvert || rs->totface || rs->tothalo || rs->totstrand || rs->totlamp) {
      spos += sprintf(spos, "| ");
    }

    if (rs->totvert) {
      spos += sprintf(spos, TIP_("Ve:%d "), rs->totvert);
    }
    if (rs->totface) {
      spos += sprintf(spos, TIP_("Fa:%d "), rs->totface);
    }
    if (rs->tothalo) {
      spos += sprintf(spos, TIP_("Ha:%d "), rs->tothalo);
    }
    if (rs->totstrand) {
      spos += sprintf(spos, TIP_("St:%d "), rs->totstrand);
    }
    if (rs->totlamp) {
      spos += sprintf(spos, TIP_("Li:%d "), rs->totlamp);
    }

    if (rs->mem_peak == 0.0f) {
      spos += sprintf(spos,
                      TIP_("| Mem:%.2fM (%.2fM, Peak %.2fM) "),
                      megs_used_memory,
                      mmap_used_memory,
                      megs_peak_memory);
    }
    else {
      spos += sprintf(spos, TIP_("| Mem:%.2fM, Peak: %.2fM "), rs->mem_used, rs->mem_peak);
    }

    if (rs->curfield) {
      spos += sprintf(spos, TIP_("Field %d "), rs->curfield);
    }
    if (rs->curblur) {
      spos += sprintf(spos, TIP_("Blur %d "), rs->curblur);
    }
  }

  /* full sample */
  if (rs->curfsa) {
    spos += sprintf(spos, TIP_("| Full Sample %d "), rs->curfsa);
  }

  /* extra info */
  if (rs->infostr && rs->infostr[0]) {
    spos += sprintf(spos, "| %s ", rs->infostr);
  }
  else if (error && error[0]) {
    spos += sprintf(spos, "| %s ", error);
  }

  /* very weak... but 512 characters is quite safe */
  if (spos >= str + IMA_MAX_RENDER_TEXT) {
    if (G.debug & G_DEBUG) {
      printf("WARNING! renderwin text beyond limit\n");
    }
  }
}

static void image_renderinfo_cb(void *rjv, RenderStats *rs)
{
  RenderJob *rj = rjv;
  RenderResult *rr;

  rr = RE_AcquireResultRead(rj->re);

  if (rr) {
    /* malloc OK here, stats_draw is not in tile threads */
    if (rr->text == NULL) {
      rr->text = MEM_callocN(IMA_MAX_RENDER_TEXT, "rendertext");
    }

    make_renderinfo_string(rs, rj->scene, rj->v3d_override, rr->error, rr->text);
  }

  RE_ReleaseResult(rj->re);

  /* make jobs timer to send notifier */
  *(rj->do_update) = true;
}

static void render_progress_update(void *rjv, float progress)
{
  RenderJob *rj = rjv;

  if (rj->progress && *rj->progress != progress) {
    *rj->progress = progress;

    /* make jobs timer to send notifier */
    *(rj->do_update) = true;
  }
}

/* Not totally reliable, but works fine in most of cases and
 * in worst case would just make it so extra color management
 * for the whole render result is applied (which was already
 * happening already).
 */
static void render_image_update_pass_and_layer(RenderJob *rj, RenderResult *rr, ImageUser *iuser)
{
  wmWindowManager *wm;
  ScrArea *first_sa = NULL, *matched_sa = NULL;

  /* image window, compo node users */
  for (wm = rj->main->wm.first; wm && matched_sa == NULL; wm = wm->id.next) { /* only 1 wm */
    wmWindow *win;
    for (win = wm->windows.first; win && matched_sa == NULL; win = win->next) {
      const bScreen *screen = WM_window_get_active_screen(win);

      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        if (sa->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = sa->spacedata.first;
          // sa->spacedata might be empty when toggling fullscreen mode.
          if (sima != NULL && sima->image == rj->image) {
            if (first_sa == NULL) {
              first_sa = sa;
            }
            if (sa == rj->sa) {
              matched_sa = sa;
              break;
            }
          }
        }
      }
    }
  }

  if (matched_sa == NULL) {
    matched_sa = first_sa;
  }

  if (matched_sa) {
    SpaceImage *sima = matched_sa->spacedata.first;
    RenderResult *main_rr = RE_AcquireResultRead(rj->re);

    /* TODO(sergey): is there faster way to get the layer index? */
    if (rr->renlay) {
      int layer = BLI_findstringindex(
          &main_rr->layers, (char *)rr->renlay->name, offsetof(RenderLayer, name));
      sima->iuser.layer = layer;
      rj->last_layer = layer;
    }

    iuser->pass = sima->iuser.pass;
    iuser->layer = sima->iuser.layer;

    RE_ReleaseResult(rj->re);
  }
}

static void image_rect_update(void *rjv, RenderResult *rr, volatile rcti *renrect)
{
  RenderJob *rj = rjv;
  Image *ima = rj->image;
  ImBuf *ibuf;
  void *lock;
  const char *viewname = RE_GetActiveRenderView(rj->re);

  /* only update if we are displaying the slot being rendered */
  if (ima->render_slot != ima->last_render_slot) {
    rj->image_outdated = true;
    return;
  }
  else if (rj->image_outdated) {
    /* update entire render */
    rj->image_outdated = false;
    BKE_image_signal(rj->main, ima, NULL, IMA_SIGNAL_COLORMANAGE);
    *(rj->do_update) = true;
    return;
  }

  if (rr == NULL) {
    return;
  }

  /* update part of render */
  render_image_update_pass_and_layer(rj, rr, &rj->iuser);
  ibuf = BKE_image_acquire_ibuf(ima, &rj->iuser, &lock);
  if (ibuf) {
    /* Don't waste time on CPU side color management if
     * image will be displayed using GLSL.
     *
     * Need to update rect if Save Buffers enabled because in
     * this case GLSL doesn't have original float buffer to
     * operate with.
     */
    if (rr->do_exr_tile || !rj->supports_glsl_draw || ibuf->channels == 1 ||
        ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL) {
      image_buffer_rect_update(rj, rr, ibuf, &rj->iuser, renrect, viewname);
    }

    /* make jobs timer to send notifier */
    *(rj->do_update) = true;
  }
  BKE_image_release_ibuf(ima, ibuf, lock);
}

static void current_scene_update(void *rjv, Scene *scene)
{
  RenderJob *rj = rjv;
  rj->current_scene = scene;
  rj->iuser.scene = scene;
}

static void render_startjob(void *rjv, short *stop, short *do_update, float *progress)
{
  RenderJob *rj = rjv;

  rj->stop = stop;
  rj->do_update = do_update;
  rj->progress = progress;

  RE_SetReports(rj->re, rj->reports);

  if (rj->anim) {
    RE_RenderAnim(rj->re,
                  rj->main,
                  rj->scene,
                  rj->single_layer,
                  rj->camera_override,
                  rj->scene->r.sfra,
                  rj->scene->r.efra,
                  rj->scene->r.frame_step);
  }
  else {
    RE_RenderFrame(rj->re,
                   rj->main,
                   rj->scene,
                   rj->single_layer,
                   rj->camera_override,
                   rj->scene->r.cfra,
                   rj->write_still);
  }

  RE_SetReports(rj->re, NULL);
}

static void render_image_restore_layer(RenderJob *rj)
{
  wmWindowManager *wm;

  /* image window, compo node users */
  for (wm = rj->main->wm.first; wm; wm = wm->id.next) { /* only 1 wm */
    wmWindow *win;
    for (win = wm->windows.first; win; win = win->next) {
      const bScreen *screen = WM_window_get_active_screen(win);

      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        if (sa == rj->sa) {
          if (sa->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = sa->spacedata.first;

            if (RE_HasSingleLayer(rj->re)) {
              /* For single layer renders keep the active layer
               * visible, or show the compositing result. */
              RenderResult *rr = RE_AcquireResultRead(rj->re);
              if (RE_HasCombinedLayer(rr)) {
                sima->iuser.layer = 0;
              }
              RE_ReleaseResult(rj->re);
            }
            else {
              /* For multiple layer render, set back the layer
               * that was set at the start of rendering. */
              sima->iuser.layer = rj->orig_layer;
            }
          }
          return;
        }
      }
    }
  }
}

static void render_endjob(void *rjv)
{
  RenderJob *rj = rjv;

  /* this render may be used again by the sequencer without the active
   * 'Render' where the callbacks would be re-assigned. assign dummy callbacks
   * to avoid referencing freed renderjobs bug T24508. */
  RE_InitRenderCB(rj->re);

  if (rj->main != G_MAIN) {
    BKE_main_free(rj->main);
  }

  /* else the frame will not update for the original value */
  if (rj->anim && !(rj->scene->r.scemode & R_NO_FRAME_UPDATE)) {
    /* possible this fails of loading new file while rendering */
    if (G_MAIN->wm.first) {
      ED_update_for_newframe(G_MAIN, rj->depsgraph);
    }
  }

  /* XXX above function sets all tags in nodes */
  ntreeCompositClearTags(rj->scene->nodetree);

  /* potentially set by caller */
  rj->scene->r.scemode &= ~R_NO_FRAME_UPDATE;

  if (rj->single_layer) {
    nodeUpdateID(rj->scene->nodetree, &rj->scene->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, rj->scene);
  }

  if (rj->sa) {
    render_image_restore_layer(rj);
  }

  /* XXX render stability hack */
  G.is_rendering = false;
  WM_main_add_notifier(NC_SCENE | ND_RENDER_RESULT, NULL);

  /* Partial render result will always update display buffer
   * for first render layer only. This is nice because you'll
   * see render progress during rendering, but it ends up in
   * wrong display buffer shown after rendering.
   *
   * The code below will mark display buffer as invalid after
   * rendering in case multiple layers were rendered, which
   * ensures display buffer matches render layer after
   * rendering.
   *
   * Perhaps proper way would be to toggle active render
   * layer in image editor and job, so we always display
   * layer being currently rendered. But this is not so much
   * trivial at this moment, especially because of external
   * engine API, so lets use simple and robust way for now
   *                                          - sergey -
   */
  if (rj->scene->view_layers.first != rj->scene->view_layers.last || rj->image_outdated) {
    void *lock;
    Image *ima = rj->image;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &rj->iuser, &lock);

    if (ibuf) {
      ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
    }

    BKE_image_release_ibuf(ima, ibuf, lock);
  }

  /* Finally unlock the user interface (if it was locked). */
  if (rj->interface_locked) {
    /* Interface was locked, so window manager couldn't have been changed
     * and using one from Global will unlock exactly the same manager as
     * was locked before running the job.
     */
    WM_set_locked_interface(G_MAIN->wm.first, false);
    DEG_on_visible_update(G_MAIN, false);
  }
}

/* called by render, check job 'stop' value or the global */
static int render_breakjob(void *rjv)
{
  RenderJob *rj = rjv;

  if (G.is_break) {
    return 1;
  }
  if (rj->stop && *(rj->stop)) {
    return 1;
  }
  return 0;
}

/* for exec() when there is no render job
 * note: this wont check for the escape key being pressed, but doing so isnt threadsafe */
static int render_break(void *UNUSED(rjv))
{
  if (G.is_break) {
    return 1;
  }
  return 0;
}

/* runs in thread, no cursor setting here works. careful with notifiers too (malloc conflicts) */
/* maybe need a way to get job send notifier? */
static void render_drawlock(void *rjv, int lock)
{
  RenderJob *rj = rjv;

  /* If interface is locked, renderer callback shall do nothing. */
  if (!rj->interface_locked) {
    BKE_spacedata_draw_locks(lock);
  }
}

/* catch esc */
static int screen_render_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = (Scene *)op->customdata;

  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_RENDER)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  switch (event->type) {
    case ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

static void screen_render_cancel(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = (Scene *)op->customdata;

  /* kill on cancel, because job is using op->reports */
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_RENDER);
}

static void clean_viewport_memory_base(Base *base)
{
  if ((base->flag & BASE_VISIBLE) == 0) {
    return;
  }

  Object *object = base->object;

  if (object->id.tag & LIB_TAG_DOIT) {
    return;
  }

  object->id.tag &= ~LIB_TAG_DOIT;
  if (RE_allow_render_generic_object(object)) {
    BKE_object_free_derived_caches(object);
  }
}

static void clean_viewport_memory(Main *bmain, Scene *scene)
{
  Scene *sce_iter;
  Base *base;

  /* Tag all the available objects. */
  BKE_main_id_tag_listbase(&bmain->objects, LIB_TAG_DOIT, true);

  /* Go over all the visible objects. */
  for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
    for (wmWindow *win = wm->windows.first; win; win = win->next) {
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);

      for (base = view_layer->object_bases.first; base; base = base->next) {
        clean_viewport_memory_base(base);
      }
    }
  }

  for (SETLOOPER_SET_ONLY(scene, sce_iter, base)) {
    clean_viewport_memory_base(base);
  }
}

/* using context, starts job */
static int screen_render_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* new render clears all callbacks */
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *active_layer = CTX_data_view_layer(C);
  ViewLayer *single_layer = NULL;
  RenderEngineType *re_type = RE_engines_find(scene->r.engine);
  Render *re;
  wmJob *wm_job;
  RenderJob *rj;
  Image *ima;
  int jobflag;
  const bool is_animation = RNA_boolean_get(op->ptr, "animation");
  const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
  const bool use_viewport = RNA_boolean_get(op->ptr, "use_viewport");
  View3D *v3d = use_viewport ? CTX_wm_view3d(C) : NULL;
  struct Object *camera_override = v3d ? V3D_CAMERA_LOCAL(v3d) : NULL;
  const char *name;
  ScrArea *sa;

  /* Cannot do render if there is not this function. */
  if (re_type->render == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* custom scene and single layer re-render */
  screen_render_single_layer_set(op, bmain, active_layer, &scene, &single_layer);

  /* only one render job at a time */
  if (WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_RENDER)) {
    return OPERATOR_CANCELLED;
  }

  if (!RE_is_rendering_allowed(scene, single_layer, camera_override, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
    return OPERATOR_CANCELLED;
  }

  /* stop all running jobs, except screen one. currently previews frustrate Render */
  WM_jobs_kill_all_except(CTX_wm_manager(C), CTX_wm_screen(C));

  /* cancel animation playback */
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    ED_screen_animation_play(C, 0, 0);
  }

  /* handle UI stuff */
  WM_cursor_wait(1);

  /* flush sculpt and editmode changes */
  ED_editors_flush_edits(bmain, true);

  /* cleanup sequencer caches before starting user triggered render.
   * otherwise, invalidated cache entries can make their way into
   * the output rendering. We can't put that into RE_RenderFrame,
   * since sequence rendering can call that recursively... (peter) */
  BKE_sequencer_cache_cleanup(scene);

  // store spare
  // get view3d layer, local layer, make this nice api call to render
  // store spare

  /* ensure at least 1 area shows result */
  sa = render_view_open(C, event->x, event->y, op->reports);

  jobflag = WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS;

  if (RNA_struct_property_is_set(op->ptr, "layer")) {
    jobflag |= WM_JOB_SUSPEND;
  }

  /* job custom data */
  rj = MEM_callocN(sizeof(RenderJob), "render job");
  rj->main = bmain;
  rj->scene = scene;
  rj->current_scene = rj->scene;
  rj->single_layer = single_layer;
  /* TODO(sergey): Render engine should be using own depsgraph. */
  rj->depsgraph = CTX_data_depsgraph(C);
  rj->camera_override = camera_override;
  rj->anim = is_animation;
  rj->write_still = is_write_still && !is_animation;
  rj->iuser.scene = scene;
  rj->iuser.ok = 1;
  rj->reports = op->reports;
  rj->orig_layer = 0;
  rj->last_layer = 0;
  rj->sa = sa;
  rj->supports_glsl_draw = IMB_colormanagement_support_glsl_draw(&scene->view_settings);

  BKE_color_managed_display_settings_copy(&rj->display_settings, &scene->display_settings);
  BKE_color_managed_view_settings_copy(&rj->view_settings, &scene->view_settings);

  if (sa) {
    SpaceImage *sima = sa->spacedata.first;
    rj->orig_layer = sima->iuser.layer;
  }

  if (v3d) {
    if (camera_override && camera_override != scene->camera) {
      rj->v3d_override = true;
    }
  }

  /* Lock the user interface depending on render settings. */
  if (scene->r.use_lock_interface) {
    WM_set_locked_interface(CTX_wm_manager(C), true);

    /* Set flag interface need to be unlocked.
     *
     * This is so because we don't have copy of render settings
     * accessible from render job and copy is needed in case
     * of non-locked rendering, so we wouldn't try to unlock
     * anything if option was initially unset but then was
     * enabled during rendering.
     */
    rj->interface_locked = true;

    /* Clean memory used by viewport? */
    clean_viewport_memory(rj->main, scene);
  }

  /* setup job */
  if (RE_seq_render_active(scene, &scene->r)) {
    name = "Sequence Render";
  }
  else {
    name = "Render";
  }

  wm_job = WM_jobs_get(
      CTX_wm_manager(C), CTX_wm_window(C), scene, name, jobflag, WM_JOB_TYPE_RENDER);
  WM_jobs_customdata_set(wm_job, rj, render_freejob);
  WM_jobs_timer(wm_job, 0.2, NC_SCENE | ND_RENDER_RESULT, 0);
  WM_jobs_callbacks(wm_job, render_startjob, NULL, NULL, render_endjob);

  /* get a render result image, and make sure it is empty */
  ima = BKE_image_verify_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_signal(rj->main, ima, NULL, IMA_SIGNAL_FREE);
  BKE_image_backup_render(rj->scene, ima, true);
  rj->image = ima;

  /* setup new render */
  re = RE_NewSceneRender(scene);
  RE_test_break_cb(re, rj, render_breakjob);
  RE_draw_lock_cb(re, rj, render_drawlock);
  RE_display_update_cb(re, rj, image_rect_update);
  RE_current_scene_update_cb(re, rj, current_scene_update);
  RE_stats_draw_cb(re, rj, image_renderinfo_cb);
  RE_progress_cb(re, rj, render_progress_update);
  RE_gl_context_create(re);

  rj->re = re;
  G.is_break = false;

  /* store actual owner of job, so modal operator could check for it,
   * the reason of this is that active scene could change when rendering
   * several layers from compositor [#31800]
   */
  op->customdata = scene;

  WM_jobs_start(CTX_wm_manager(C), wm_job);

  WM_cursor_wait(0);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  /* we set G.is_rendering here already instead of only in the job, this ensure
   * main loop or other scene updates are disabled in time, since they may
   * have started before the job thread */
  G.is_rendering = true;

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* contextual render, using current scene, view3d? */
void RENDER_OT_render(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Render";
  ot->description = "Render active scene";
  ot->idname = "RENDER_OT_render";

  /* api callbacks */
  ot->invoke = screen_render_invoke;
  ot->modal = screen_render_modal;
  ot->cancel = screen_render_cancel;
  ot->exec = screen_render_exec;

  /* this isn't needed, causes failer in background mode */
#if 0
  ot->poll = ED_operator_screenactive;
#endif

  prop = RNA_def_boolean(ot->srna,
                         "animation",
                         0,
                         "Animation",
                         "Render files from the animation range of this scene");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_boolean(
      ot->srna,
      "write_still",
      0,
      "Write Image",
      "Save rendered the image to the output path (used only when animation is disabled)");
  prop = RNA_def_boolean(ot->srna,
                         "use_viewport",
                         0,
                         "Use 3D Viewport",
                         "When inside a 3D viewport, use layers and camera of the viewport");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_string(ot->srna,
                        "layer",
                        NULL,
                        RE_MAXNAME,
                        "Render Layer",
                        "Single render layer to re-render (used only when animation is disabled)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_string(ot->srna,
                        "scene",
                        NULL,
                        MAX_ID_NAME - 2,
                        "Scene",
                        "Scene to render, current scene if not specified");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

Scene *ED_render_job_get_scene(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  RenderJob *rj = (RenderJob *)WM_jobs_customdata_from_type(wm, WM_JOB_TYPE_RENDER);

  if (rj) {
    return rj->scene;
  }

  return NULL;
}

Scene *ED_render_job_get_current_scene(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  RenderJob *rj = (RenderJob *)WM_jobs_customdata_from_type(wm, WM_JOB_TYPE_RENDER);
  if (rj) {
    return rj->current_scene;
  }
  return NULL;
}

/* Motion blur curve preset */

static int render_shutter_curve_preset_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  CurveMapping *mblur_shutter_curve = &scene->r.mblur_shutter_curve;
  CurveMap *cm = mblur_shutter_curve->cm;
  int preset = RNA_enum_get(op->ptr, "shape");

  cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  mblur_shutter_curve->preset = preset;
  curvemap_reset(
      cm, &mblur_shutter_curve->clipr, mblur_shutter_curve->preset, CURVEMAP_SLOPE_POS_NEG);
  curvemapping_changed(mblur_shutter_curve, false);

  return OPERATOR_FINISHED;
}

void RENDER_OT_shutter_curve_preset(wmOperatorType *ot)
{
  PropertyRNA *prop;
  static const EnumPropertyItem prop_shape_items[] = {
      {CURVE_PRESET_SHARP, "SHARP", 0, "Sharp", ""},
      {CURVE_PRESET_SMOOTH, "SMOOTH", 0, "Smooth", ""},
      {CURVE_PRESET_MAX, "MAX", 0, "Max", ""},
      {CURVE_PRESET_LINE, "LINE", 0, "Line", ""},
      {CURVE_PRESET_ROUND, "ROUND", 0, "Round", ""},
      {CURVE_PRESET_ROOT, "ROOT", 0, "Root", ""},
      {0, NULL, 0, NULL, NULL},
  };

  ot->name = "Shutter Curve Preset";
  ot->description = "Set shutter curve";
  ot->idname = "RENDER_OT_shutter_curve_preset";

  ot->exec = render_shutter_curve_preset_exec;

  prop = RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
}
