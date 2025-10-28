/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#include <cstddef>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_time.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "NOD_composite.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "BIF_glutil.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "SEQ_relations.hh"

#include "render_intern.hh"

/* Render Callbacks */
static bool render_break(void *rjv);

struct RenderJob : public RenderJobBase {
  Main *main;
  ViewLayer *view_layer;
  ViewLayer *single_layer;
  Render *re;
  Object *camera_override;
  bool v3d_override;
  bool anim, write_still;
  Image *image;
  ImageUser iuser;
  bool image_outdated;
  bool *stop;
  bool *do_update;
  float *progress;
  ReportList *reports;
  int orig_layer;
  int last_layer;
  ScrArea *area;
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  bool interface_locked;
  int frame_start;
  int frame_end;
};

/* called inside thread! */
static bool image_buffer_calc_tile_rect(const RenderResult *rr,
                                        const ImBuf *ibuf,
                                        rcti *renrect,
                                        rcti *r_ibuf_rect,
                                        int *r_offset_x,
                                        int *r_offset_y)
{
  int tile_y, tile_height, tile_x, tile_width;

  /* When `renrect` argument is not nullptr, we only refresh scan-lines. */
  if (renrect) {
    /* `if (tile_height == recty)`, rendering of layer is ready,
     * we should not draw, other things happen... */
    if (rr->renlay == nullptr || renrect->ymax >= rr->recty) {
      return false;
    }

    /* `tile_x` here is first sub-rectangle x coord, tile_width defines sub-rectangle width. */
    tile_x = renrect->xmin;
    tile_width = renrect->xmax - tile_x;
    if (tile_width < 2) {
      return false;
    }

    tile_y = renrect->ymin;
    tile_height = renrect->ymax - tile_y;
    if (tile_height < 2) {
      return false;
    }
    renrect->ymin = renrect->ymax;
  }
  else {
    tile_x = tile_y = 0;
    tile_width = rr->rectx;
    tile_height = rr->recty;
  }

  /* tile_x tile_y is in tile coords. transform to ibuf */
  int offset_x = rr->tilerect.xmin;
  if (offset_x >= ibuf->x) {
    return false;
  }
  int offset_y = rr->tilerect.ymin;
  if (offset_y >= ibuf->y) {
    return false;
  }

  if (offset_x + tile_width > ibuf->x) {
    tile_width = ibuf->x - offset_x;
  }
  if (offset_y + tile_height > ibuf->y) {
    tile_height = ibuf->y - offset_y;
  }

  if (tile_width < 1 || tile_height < 1) {
    return false;
  }

  r_ibuf_rect->xmax = tile_x + tile_width;
  r_ibuf_rect->ymax = tile_y + tile_height;
  r_ibuf_rect->xmin = tile_x;
  r_ibuf_rect->ymin = tile_y;
  *r_offset_x = offset_x;
  *r_offset_y = offset_y;
  return true;
}

static void image_buffer_rect_update(RenderJob *rj,
                                     RenderResult *rr,
                                     ImBuf *ibuf,
                                     ImageUser *iuser,
                                     const rcti *tile_rect,
                                     int offset_x,
                                     int offset_y,
                                     const char *viewname)
{
  Scene *scene = rj->scene;
  const float *rectf = nullptr;
  int linear_stride, linear_offset_x, linear_offset_y;
  const ColorManagedViewSettings *view_settings;
  const ColorManagedDisplaySettings *display_settings;

  if (ibuf->userflags & IB_DISPLAY_BUFFER_INVALID) {
    /* The whole image buffer is to be color managed again anyway. */
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
    const int view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);
    const RenderView *rv = RE_RenderViewGetById(rr, view_id);

    if (rv->ibuf == nullptr) {
      return;
    }

    /* find current float rect for display, first case is after composite... still weak */
    if (rv->ibuf->float_buffer.data) {
      rectf = rv->ibuf->float_buffer.data;
    }
    else {
      if (rv->ibuf->byte_buffer.data) {
        /* special case, currently only happens with sequencer rendering,
         * which updates the whole frame, so we can only mark display buffer
         * as invalid here (sergey)
         */
        ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
        return;
      }
      if (rr->renlay == nullptr) {
        return;
      }
      rectf = RE_RenderLayerGetPass(rr->renlay, RE_PASSNAME_COMBINED, viewname);
    }
    if (rectf == nullptr) {
      return;
    }

    rectf += 4 * (rr->rectx * tile_rect->ymin + tile_rect->xmin);
    linear_stride = rr->rectx;
    linear_offset_x = offset_x;
    linear_offset_y = offset_y;
  }
  else {
    rectf = ibuf->float_buffer.data;
    linear_stride = ibuf->x;
    linear_offset_x = 0;
    linear_offset_y = 0;
  }

  view_settings = &scene->view_settings;
  display_settings = &scene->display_settings;

  IMB_partial_display_buffer_update(ibuf,
                                    rectf,
                                    nullptr,
                                    linear_stride,
                                    linear_offset_x,
                                    linear_offset_y,
                                    view_settings,
                                    display_settings,
                                    offset_x,
                                    offset_y,
                                    offset_x + BLI_rcti_size_x(tile_rect),
                                    offset_y + BLI_rcti_size_y(tile_rect));
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
      /* camera switch won't have updated */
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

static bool render_operator_has_custom_frame_range(wmOperator *render_operator)
{
  return RNA_struct_property_is_set(render_operator->ptr, "frame_start") ||
         RNA_struct_property_is_set(render_operator->ptr, "frame_end");
}

static void get_render_operator_frame_range(wmOperator *render_operator,
                                            const Scene *scene,
                                            int &frame_start,
                                            int &frame_end)
{
  if (RNA_struct_property_is_set(render_operator->ptr, "frame_start")) {
    frame_start = RNA_int_get(render_operator->ptr, "frame_start");
  }
  else {
    frame_start = scene->r.sfra;
  }

  if (RNA_struct_property_is_set(render_operator->ptr, "frame_end")) {
    frame_end = RNA_int_get(render_operator->ptr, "frame_end");
  }
  else {
    frame_end = scene->r.efra;
  }
}

/* executes blocking render */
static wmOperatorStatus screen_render_exec(bContext *C, wmOperator *op)
{
  ViewLayer *single_layer = nullptr;
  Render *re;
  Image *ima;
  View3D *v3d = CTX_wm_view3d(C);
  Main *mainp = CTX_data_main(C);

  const bool is_animation = RNA_boolean_get(op->ptr, "animation");
  const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
  const bool use_sequencer_scene = RNA_boolean_get(op->ptr, "use_sequencer_scene");

  Scene *scene = use_sequencer_scene ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  ViewLayer *active_layer = use_sequencer_scene ? BKE_view_layer_default_render(scene) :
                                                  CTX_data_view_layer(C);
  RenderEngineType *re_type = RE_engines_find(scene->r.engine);
  Object *camera_override = v3d ? V3D_CAMERA_LOCAL(v3d) : nullptr;

  /* Cannot do render if there is not this function. */
  if (re_type->render == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (use_sequencer_scene && !RE_seq_render_active(scene, &scene->r)) {
    BKE_report(op->reports, RPT_ERROR, "No sequencer scene with video strips to render");
    return OPERATOR_CANCELLED;
  }

  if (!is_animation && render_operator_has_custom_frame_range(op)) {
    BKE_report(op->reports, RPT_ERROR, "Frame start/end specified in a non-animation render");
    return OPERATOR_CANCELLED;
  }

  int frame_start, frame_end;
  get_render_operator_frame_range(op, scene, frame_start, frame_end);
  if (is_animation && frame_start > frame_end) {
    BKE_report(op->reports, RPT_ERROR, "Start frame is larger than end frame");
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

  RE_draw_lock_cb(re, nullptr, nullptr);
  RE_test_break_cb(re, nullptr, render_break);

  ima = BKE_image_ensure_viewer(mainp, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_signal(mainp, ima, nullptr, IMA_SIGNAL_FREE);
  BKE_image_backup_render(scene, ima, true);

  /* cleanup sequencer caches before starting user triggered render.
   * otherwise, invalidated cache entries can make their way into
   * the output rendering. We can't put that into RE_RenderFrame,
   * since sequence rendering can call that recursively... */
  blender::seq::cache_cleanup(scene, blender::seq::CacheCleanup::FinalAndIntra);

  RE_SetReports(re, op->reports);

  if (is_animation) {
    RE_RenderAnim(re,
                  mainp,
                  scene,
                  single_layer,
                  camera_override,
                  frame_start,
                  frame_end,
                  scene->r.frame_step);
  }
  else {
    RE_RenderFrame(re,
                   mainp,
                   scene,
                   single_layer,
                   camera_override,
                   scene->r.cfra,
                   scene->r.subframe,
                   is_write_still);
  }

  RE_SetReports(re, nullptr);

  const bool cancelled = G.is_break;

  if (cancelled) {
    RenderResult *rr = RE_AcquireResultRead(re);
    if (rr && rr->error) {
      /* NOTE(@ideasman42): Report, otherwise the error is entirely hidden from script authors.
       * This is only done for the #wmOperatorType::exec function because it's assumed users
       * rendering interactively will view the render and see the error message there. */
      BKE_report(op->reports, RPT_ERROR, rr->error);
    }
    RE_ReleaseResult(re);
  }

  /* No redraw needed, we leave state as we entered it. */
  ED_update_for_newframe(mainp, CTX_data_depsgraph_pointer(C));

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  if (cancelled) {
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static void render_freejob(void *rjv)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);

  BKE_color_managed_view_settings_free(&rj->view_settings);
  MEM_delete(rj);
}

static void make_renderinfo_string(const RenderStats *rs,
                                   const Scene *scene,
                                   const bool v3d_override,
                                   const char *error,
                                   char ret[IMA_MAX_RENDER_TEXT_SIZE])
{
  const char *info_space = " ";
  const char *info_sep = "| ";
  struct {
    char time_last[32];
    char time_elapsed[32];
    char frame[16];
    char statistics[64];
  } info_buffers;

  const char *ret_array[32];
  int i = 0;

  const uintptr_t mem_in_use = MEM_get_memory_in_use();
  const uintptr_t peak_memory = MEM_get_peak_memory();

  const int megs_used_memory = ceilf(mem_in_use / (1024.0 * 1024.0));
  const int megs_peak_memory = ceilf(peak_memory / (1024.0 * 1024.0));

  /* local view */
  if (rs->localview) {
    ret_array[i++] = RPT_("3D Local View ");
    ret_array[i++] = info_sep;
  }
  else if (v3d_override) {
    ret_array[i++] = RPT_("3D View ");
    ret_array[i++] = info_sep;
  }

  /* frame number */
  SNPRINTF_UTF8(info_buffers.frame, "%d ", scene->r.cfra);
  ret_array[i++] = RPT_("Frame:");
  ret_array[i++] = info_buffers.frame;

  /* Previous and elapsed time. */
  const char *info_time = info_buffers.time_last;
  BLI_timecode_string_from_time_simple(
      info_buffers.time_last, sizeof(info_buffers.time_last), rs->lastframetime);

  ret_array[i++] = info_sep;
  if (rs->infostr && rs->infostr[0]) {
    if (rs->lastframetime != 0.0) {
      ret_array[i++] = "Last:";
      ret_array[i++] = info_buffers.time_last;
      ret_array[i++] = info_space;
    }

    info_time = info_buffers.time_elapsed;
    BLI_timecode_string_from_time_simple(info_buffers.time_elapsed,
                                         sizeof(info_buffers.time_elapsed),
                                         BLI_time_now_seconds() - rs->starttime);
  }

  ret_array[i++] = RPT_("Time:");
  ret_array[i++] = info_time;
  ret_array[i++] = info_space;

  /* Statistics. */
  {
    const char *info_statistics = nullptr;
    if (rs->statstr) {
      if (rs->statstr[0]) {
        info_statistics = rs->statstr;
      }
    }
    else {
      if (rs->mem_peak == 0.0f) {
        SNPRINTF_UTF8(info_buffers.statistics,
                      RPT_("Mem:%dM, Peak: %dM"),
                      megs_used_memory,
                      megs_peak_memory);
      }
      else {
        SNPRINTF_UTF8(
            info_buffers.statistics, RPT_("Mem:%dM, Peak: %dM"), rs->mem_used, rs->mem_peak);
      }
      info_statistics = info_buffers.statistics;
    }

    if (info_statistics) {
      ret_array[i++] = info_sep;
      ret_array[i++] = info_statistics;
      ret_array[i++] = info_space;
    }
  }

  /* Extra info. */
  {
    const char *info_extra = nullptr;
    if (rs->infostr && rs->infostr[0]) {
      info_extra = rs->infostr;
    }
    else if (error && error[0]) {
      info_extra = error;
    }

    if (info_extra) {
      ret_array[i++] = info_sep;
      ret_array[i++] = info_extra;
      ret_array[i++] = info_space;
    }
  }

  if (G.debug & G_DEBUG) {
    if (BLI_string_len_array(ret_array, i) >= IMA_MAX_RENDER_TEXT_SIZE) {
      printf("WARNING! renderwin text beyond limit\n");
    }
  }

  BLI_assert(i < int(BOUNDED_ARRAY_TYPE_SIZE<decltype(ret_array)>()));
  BLI_string_join_array(ret, IMA_MAX_RENDER_TEXT_SIZE, ret_array, i);
}

static void image_renderinfo_cb(void *rjv, RenderStats *rs)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);
  RenderResult *rr;

  rr = RE_AcquireResultRead(rj->re);

  if (rr) {
    /* `malloc` is OK here, `stats_draw` is not in tile threads. */
    if (rr->text == nullptr) {
      rr->text = MEM_calloc_arrayN<char>(IMA_MAX_RENDER_TEXT_SIZE, "rendertext");
    }

    make_renderinfo_string(rs, rj->scene, rj->v3d_override, rr->error, rr->text);
  }

  RE_ReleaseResult(rj->re);

  /* make jobs timer to send notifier */
  *(rj->do_update) = true;
}

static void render_progress_update(void *rjv, float progress)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);

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
  ScrArea *first_area = nullptr, *matched_area = nullptr;

  /* image window, compo node users */

  /* Only ever 1 `wm`. */
  for (wmWindowManager *wm = static_cast<wmWindowManager *>(rj->main->wm.first);
       wm && matched_area == nullptr;
       wm = static_cast<wmWindowManager *>(wm->id.next))
  {
    wmWindow *win;
    for (win = static_cast<wmWindow *>(wm->windows.first); win && matched_area == nullptr;
         win = win->next)
    {
      const bScreen *screen = WM_window_get_active_screen(win);

      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        if (area->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
          /* area->spacedata might be empty when toggling full-screen mode. */
          if (sima != nullptr && sima->image == rj->image) {
            if (first_area == nullptr) {
              first_area = area;
            }
            if (area == rj->area) {
              matched_area = area;
              break;
            }
          }
        }
      }
    }
  }

  if (matched_area == nullptr) {
    matched_area = first_area;
  }

  if (matched_area) {
    SpaceImage *sima = static_cast<SpaceImage *>(matched_area->spacedata.first);
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

static void image_rect_update(void *rjv, RenderResult *rr, rcti *renrect)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);
  Image *ima = rj->image;
  ImBuf *ibuf;
  void *lock;
  const char *viewname = RE_GetActiveRenderView(rj->re);

  /* only update if we are displaying the slot being rendered */
  if (ima->render_slot != ima->last_render_slot) {
    rj->image_outdated = true;
    return;
  }
  if (rj->image_outdated) {
    /* Free all render buffer caches when switching slots, with lock to ensure main
     * thread is not drawing the buffer at the same time. */
    rj->image_outdated = false;
    ibuf = BKE_image_acquire_ibuf(ima, &rj->iuser, &lock);
    BKE_image_free_buffers(ima);
    BKE_image_release_ibuf(ima, ibuf, lock);
    *(rj->do_update) = true;
    return;
  }

  if (rr == nullptr) {
    return;
  }

  /* update part of render */
  render_image_update_pass_and_layer(rj, rr, &rj->iuser);
  rcti tile_rect;
  int offset_x;
  int offset_y;
  ibuf = BKE_image_acquire_ibuf(ima, &rj->iuser, &lock);
  if (ibuf) {
    if (!image_buffer_calc_tile_rect(rr, ibuf, renrect, &tile_rect, &offset_x, &offset_y)) {
      BKE_image_release_ibuf(ima, ibuf, lock);
      return;
    }

    /* Don't waste time on CPU side color management if
     * image will be displayed using GLSL.
     *
     * Need to update rect if Save Buffers enabled because in
     * this case GLSL doesn't have original float buffer to
     * operate with.
     */
    if (ibuf->channels == 1 || ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL) {
      image_buffer_rect_update(rj, rr, ibuf, &rj->iuser, &tile_rect, offset_x, offset_y, viewname);
    }
    ImageTile *image_tile = BKE_image_get_tile(ima, 0);
    BKE_image_update_gputexture_delayed(ima,
                                        image_tile,
                                        ibuf,
                                        offset_x,
                                        offset_y,
                                        BLI_rcti_size_x(&tile_rect),
                                        BLI_rcti_size_y(&tile_rect));

    /* make jobs timer to send notifier */
    *(rj->do_update) = true;
  }
  BKE_image_release_ibuf(ima, ibuf, lock);
}

static void current_scene_update(void *rjv, Scene *scene)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);

  if (rj->current_scene != scene) {
    /* Image must be updated when rendered scene changes. */
    BKE_image_partial_update_mark_full_update(rj->image);
  }

  rj->current_scene = scene;
  rj->iuser.scene = scene;
}

static void render_startjob(void *rjv, wmJobWorkerStatus *worker_status)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);

  rj->stop = &worker_status->stop;
  rj->do_update = &worker_status->do_update;
  rj->progress = &worker_status->progress;

  RE_SetReports(rj->re, rj->reports);

  if (rj->anim) {
    RE_RenderAnim(rj->re,
                  rj->main,
                  rj->scene,
                  rj->single_layer,
                  rj->camera_override,
                  rj->frame_start,
                  rj->frame_end,
                  rj->scene->r.frame_step);
  }
  else {
    RE_RenderFrame(rj->re,
                   rj->main,
                   rj->scene,
                   rj->single_layer,
                   rj->camera_override,
                   rj->scene->r.cfra,
                   rj->scene->r.subframe,
                   rj->write_still);
  }

  RE_SetReports(rj->re, nullptr);
}

static void render_image_restore_layer(RenderJob *rj)
{
  /* image window, compo node users */

  /* Only ever 1 `wm`. */
  LISTBASE_FOREACH (wmWindowManager *, wm, &rj->main->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      const bScreen *screen = WM_window_get_active_screen(win);

      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        if (area == rj->area) {
          if (area->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);

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
  RenderJob *rj = static_cast<RenderJob *>(rjv);

  /* This render may be used again by the sequencer without the active
   * 'Render' where the callbacks would be re-assigned. assign dummy callbacks
   * to avoid referencing freed render-jobs bug #24508. */
  RE_InitRenderCB(rj->re);

  if (rj->main != G_MAIN) {
    BKE_main_free(rj->main);
  }

  /* Update depsgraph for returning to the original frame before animation render job. */
  if (rj->anim && !(rj->scene->r.scemode & R_NO_FRAME_UPDATE)) {
    /* Possible this fails when loading new file while rendering. */
    if (G_MAIN->wm.first) {
      /* Check view layer was not deleted during render. Technically another view layer
       * may get allocated with the same pointer, but worst case it will cause an
       * unnecessary update. */
      if (BLI_findindex(&rj->scene->view_layers, rj->view_layer) != -1) {
        Depsgraph *depsgraph = BKE_scene_get_depsgraph(rj->scene, rj->view_layer);
        if (depsgraph) {
          ED_update_for_newframe(G_MAIN, depsgraph);
        }
      }
    }
  }

  /* XXX above function sets all tags in nodes */
  ntreeCompositClearTags(rj->scene->compositing_node_group);

  /* potentially set by caller */
  rj->scene->r.scemode &= ~R_NO_FRAME_UPDATE;

  if (rj->single_layer) {
    BKE_ntree_update_tag_id_changed(rj->main, &rj->scene->id);
    BKE_ntree_update(*rj->main);
    WM_main_add_notifier(NC_NODE | NA_EDITED, rj->scene);
  }

  if (rj->area) {
    render_image_restore_layer(rj);
  }

  /* XXX render stability hack */
  G.is_rendering = false;
  WM_main_add_notifier(NC_SCENE | ND_RENDER_RESULT, nullptr);

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
    WM_locked_interface_set(static_cast<wmWindowManager *>(G_MAIN->wm.first), false);
    DEG_tag_on_visible_update(G_MAIN, false);
  }
}

/* called by render, check job 'stop' value or the global */
static bool render_breakjob(void *rjv)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);

  if (G.is_break) {
    return true;
  }
  if (rj->stop && *(rj->stop)) {
    return true;
  }
  return false;
}

/**
 * For exec() when there is no render job
 * NOTE: this won't check for the escape key being pressed, but doing so isn't thread-safe.
 */
static bool render_break(void * /*rjv*/)
{
  if (G.is_break) {
    return true;
  }
  return false;
}

/* runs in thread, no cursor setting here works. careful with notifiers too (`malloc` conflicts) */
/* maybe need a way to get job send notifier? */
static void render_drawlock(void *rjv, bool lock)
{
  RenderJob *rj = static_cast<RenderJob *>(rjv);

  /* If interface is locked, renderer callback shall do nothing. */
  if (!rj->interface_locked) {
    BKE_spacedata_draw_locks(lock ? REGION_DRAW_LOCK_RENDER : REGION_DRAW_LOCK_NONE);
  }
}

/** Catch escape key to cancel. */
static wmOperatorStatus screen_render_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = (Scene *)op->customdata;

  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_RENDER)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  return (event->type == EVT_ESCKEY) ? OPERATOR_RUNNING_MODAL : OPERATOR_PASS_THROUGH;
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
  if ((base->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) == 0) {
    return;
  }

  Object *object = base->object;

  if (object->id.tag & ID_TAG_DOIT) {
    return;
  }

  object->id.tag &= ~ID_TAG_DOIT;
  if (RE_allow_render_generic_object(object)) {
    BKE_object_free_derived_caches(object);
  }
}

static void clean_viewport_memory(Main *bmain, Scene *scene)
{
  Scene *sce_iter;
  Base *base;

  /* Tag all the available objects. */
  BKE_main_id_tag_listbase(&bmain->objects, ID_TAG_DOIT, true);

  /* Go over all the visible objects. */

  /* Only ever 1 `wm`. */
  LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      BKE_view_layer_synced_ensure(scene, view_layer);

      LISTBASE_FOREACH (Base *, b, BKE_view_layer_object_bases_get(view_layer)) {
        clean_viewport_memory_base(b);
      }
    }
  }

  for (SETLOOPER_SET_ONLY(scene, sce_iter, base)) {
    clean_viewport_memory_base(base);
  }
}

/* using context, starts job */
static wmOperatorStatus screen_render_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* new render clears all callbacks */
  Main *bmain = CTX_data_main(C);
  ViewLayer *single_layer = nullptr;
  Render *re;
  wmJob *wm_job;
  RenderJob *rj;
  Image *ima;
  ScrArea *area;

  const bool is_animation = RNA_boolean_get(op->ptr, "animation");
  const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
  const bool use_viewport = RNA_boolean_get(op->ptr, "use_viewport");
  const bool use_sequencer_scene = RNA_boolean_get(op->ptr, "use_sequencer_scene");

  View3D *v3d = use_viewport ? CTX_wm_view3d(C) : nullptr;
  Scene *scene = use_sequencer_scene ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  ViewLayer *active_layer = use_sequencer_scene ? BKE_view_layer_default_render(scene) :
                                                  CTX_data_view_layer(C);
  RenderEngineType *re_type = RE_engines_find(scene->r.engine);
  Object *camera_override = v3d ? V3D_CAMERA_LOCAL(v3d) : nullptr;

  /* Cannot do render if there is not this function. */
  if (re_type->render == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (use_sequencer_scene && !RE_seq_render_active(scene, &scene->r)) {
    BKE_report(op->reports, RPT_ERROR, "No sequencer scene with video strips to render");
    return OPERATOR_CANCELLED;
  }

  if (!is_animation && render_operator_has_custom_frame_range(op)) {
    BKE_report(op->reports, RPT_ERROR, "Frame start/end specified in a non-animation render");
    return OPERATOR_CANCELLED;
  }

  int frame_start, frame_end;
  get_render_operator_frame_range(op, scene, frame_start, frame_end);
  if (is_animation && frame_start > frame_end) {
    BKE_report(op->reports, RPT_ERROR, "Start frame is larger than end frame");
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

  /* Reports are done inside check function, and it will return false if there are other strips to
   * render. */
  if ((scene->r.scemode & R_DOSEQ) &&
      blender::seq::relations_check_scene_recursion(scene, op->reports))
  {
    return OPERATOR_CANCELLED;
  }

  /* stop all running jobs, except screen one. currently previews frustrate Render */
  WM_jobs_kill_all_except(CTX_wm_manager(C), CTX_wm_screen(C));

  /* cancel animation playback */
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    ED_screen_animation_play(C, 0, 0);
  }

  /* handle UI stuff */
  WM_cursor_wait(true);

  /* flush sculpt and editmode changes */
  ED_editors_flush_edits_ex(bmain, true, false);

  /* Cleanup VSE cache, since it is not guaranteed that stored images are invalid. */
  blender::seq::cache_cleanup(scene, blender::seq::CacheCleanup::FinalAndIntra);

  /* store spare
   * get view3d layer, local layer, make this nice API call to render
   * store spare */

  /* ensure at least 1 area shows result */
  area = render_view_open(C, event->xy[0], event->xy[1], op->reports);

  /* job custom data */
  rj = MEM_new<RenderJob>("render job");
  rj->main = bmain;
  rj->scene = scene;
  rj->current_scene = rj->scene;
  rj->view_layer = active_layer;
  rj->single_layer = single_layer;
  rj->camera_override = camera_override;
  rj->anim = is_animation;
  rj->write_still = is_write_still && !is_animation;
  rj->iuser.scene = scene;
  rj->reports = op->reports;
  rj->orig_layer = 0;
  rj->last_layer = 0;
  rj->area = area;
  rj->frame_start = frame_start;
  rj->frame_end = frame_end;

  BKE_color_managed_display_settings_copy(&rj->display_settings, &scene->display_settings);
  BKE_color_managed_view_settings_copy(&rj->view_settings, &scene->view_settings);

  if (area) {
    SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
    rj->orig_layer = sima->iuser.layer;
  }

  if (v3d) {
    if (camera_override && camera_override != scene->camera) {
      rj->v3d_override = true;
    }
  }

  /* Lock the user interface depending on render settings. */
  if (scene->r.use_lock_interface) {
    WM_locked_interface_set_with_flags(CTX_wm_manager(C), REGION_DRAW_LOCK_RENDER);

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
  const char *name;
  if (RE_seq_render_active(scene, &scene->r)) {
    name = RPT_("Rendering sequence...");
  }
  else {
    name = RPT_("Rendering...");
  }

  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       scene,
                       name,
                       WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS,
                       WM_JOB_TYPE_RENDER);
  WM_jobs_customdata_set(wm_job, rj, render_freejob);
  WM_jobs_timer(wm_job, 0.2, NC_SCENE | ND_RENDER_RESULT, 0);
  WM_jobs_callbacks(wm_job, render_startjob, nullptr, nullptr, render_endjob);

  if (RNA_struct_property_is_set(op->ptr, "layer")) {
    WM_jobs_delay_start(wm_job, 0.2);
  }

  /* get a render result image, and make sure it is empty */
  ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_signal(rj->main, ima, nullptr, IMA_SIGNAL_FREE);
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
  RE_system_gpu_context_ensure(re);

  rj->re = re;
  G.is_break = false;

  /* store actual owner of job, so modal operator could check for it,
   * the reason of this is that active scene could change when rendering
   * several layers from compositor #31800. */
  op->customdata = scene;

  WM_jobs_start(CTX_wm_manager(C), wm_job);

  WM_cursor_wait(false);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  /* we set G.is_rendering here already instead of only in the job, this ensure
   * main loop or other scene updates are disabled in time, since they may
   * have started before the job thread */
  G.is_rendering = true;

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static std::string screen_render_get_description(bContext * /*C*/,
                                                 wmOperatorType * /*ot*/,
                                                 PointerRNA *ptr)
{
  const bool use_sequencer_scene = RNA_boolean_get(ptr, "use_sequencer_scene");
  if (use_sequencer_scene) {
    return TIP_("Render active sequencer scene");
  }
  return TIP_("Render active scene");
}

void RENDER_OT_render(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Render";
  ot->idname = "RENDER_OT_render";

  /* API callbacks. */
  ot->invoke = screen_render_invoke;
  ot->modal = screen_render_modal;
  ot->cancel = screen_render_cancel;
  ot->exec = screen_render_exec;
  ot->get_description = screen_render_get_description;

  /* This isn't needed, causes failure in background mode. */
#if 0
  ot->poll = ED_operator_screenactive;
#endif

  prop = RNA_def_boolean(ot->srna,
                         "animation",
                         false,
                         "Animation",
                         "Render files from the animation range of this scene");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_boolean(
      ot->srna,
      "write_still",
      false,
      "Write Image",
      "Save the rendered image to the output path (used only when animation is disabled)");
  prop = RNA_def_boolean(ot->srna,
                         "use_viewport",
                         false,
                         "Use 3D Viewport",
                         "When inside a 3D viewport, use layers and camera of the viewport");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "use_sequencer_scene",
                         false,
                         "Use Sequencer Scene",
                         "Render the sequencer scene instead of the active scene");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_string(ot->srna,
                        "layer",
                        nullptr,
                        RE_MAXNAME,
                        "Render Layer",
                        "Single render layer to re-render (used only when animation is disabled)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_string(ot->srna,
                        "scene",
                        nullptr,
                        MAX_ID_NAME - 2,
                        "Scene",
                        "Scene to render, current scene if not specified");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_int(
      ot->srna,
      "frame_start",
      0,
      INT_MIN,
      INT_MAX,
      "Start Frame",
      "Frame to start rendering animation at. If not specified, the scene start frame will be "
      "assumed. This should only be specified if doing an animation render",
      INT_MIN,
      INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_int(
      ot->srna,
      "frame_end",
      0,
      INT_MIN,
      INT_MAX,
      "End Frame",
      "Frame to end rendering animation at. If not specified, the scene end frame will be "
      "assumed. This should only be specified if doing an animation render",
      INT_MIN,
      INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static RenderJobBase *render_job_get(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  RenderJobBase *rj;

  /* Try to find job tied to active scene first. */
  rj = static_cast<RenderJobBase *>(
      WM_jobs_customdata_from_type(wm, CTX_data_scene(C), WM_JOB_TYPE_RENDER));

  /* If not found, attempt to find job tied to sequencer scene. */
  if (rj == nullptr) {
    return static_cast<RenderJobBase *>(
        WM_jobs_customdata_from_type(wm, CTX_data_sequencer_scene(C), WM_JOB_TYPE_RENDER));
  }

  return rj;
}

Scene *ED_render_job_get_scene(const bContext *C)
{
  RenderJobBase *rj = render_job_get(C);
  return rj ? rj->scene : nullptr;
}

Scene *ED_render_job_get_current_scene(const bContext *C)
{
  RenderJobBase *rj = render_job_get(C);
  return rj ? rj->current_scene : nullptr;
}

/* Motion blur curve preset */

static wmOperatorStatus render_shutter_curve_preset_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  CurveMapping *mblur_shutter_curve = &scene->r.mblur_shutter_curve;
  CurveMap *cm = mblur_shutter_curve->cm;
  int preset = RNA_enum_get(op->ptr, "shape");

  mblur_shutter_curve->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  mblur_shutter_curve->preset = preset;
  BKE_curvemap_reset(cm,
                     &mblur_shutter_curve->clipr,
                     mblur_shutter_curve->preset,
                     CurveMapSlopeType::PositiveNegative);
  BKE_curvemapping_changed(mblur_shutter_curve, false);

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
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Shutter Curve Preset";
  ot->description = "Set shutter curve";
  ot->idname = "RENDER_OT_shutter_curve_preset";

  ot->exec = render_shutter_curve_preset_exec;

  prop = RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
}
