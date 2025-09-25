/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <sys/types.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_mask_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_mutex.hh"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "ED_clip.hh"
#include "ED_select_utils.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_view2d.hh"

#include "clip_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Operator Poll Functions
 * \{ */

bool ED_space_clip_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc && sc->clip) {
    return true;
  }

  return false;
}

bool ED_space_clip_view_clip_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc) {
    return sc->view == SC_VIEW_CLIP;
  }

  return false;
}

bool ED_space_clip_tracking_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc && sc->clip) {
    return ED_space_clip_check_show_trackedit(sc);
  }

  return false;
}

bool ED_space_clip_maskedit_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc && sc->clip) {
    return ED_space_clip_check_show_maskedit(sc);
  }

  return false;
}

bool ED_space_clip_maskedit_visible_splines_poll(bContext *C)
{
  if (!ED_space_clip_maskedit_poll(C)) {
    return false;
  }

  const SpaceClip *space_clip = CTX_wm_space_clip(C);
  return space_clip->overlay.flag & SC_SHOW_OVERLAYS &&
         space_clip->mask_info.draw_flag & MASK_DRAWFLAG_SPLINE;
}

bool ED_space_clip_maskedit_mask_poll(bContext *C)
{
  if (ED_space_clip_maskedit_poll(C)) {
    MovieClip *clip = CTX_data_edit_movieclip(C);

    if (clip) {
      SpaceClip *sc = CTX_wm_space_clip(C);

      return sc->mask_info.mask != nullptr;
    }
  }

  return false;
}

bool ED_space_clip_maskedit_mask_visible_splines_poll(bContext *C)
{
  if (!ED_space_clip_maskedit_mask_poll(C)) {
    return false;
  }

  const SpaceClip *space_clip = CTX_wm_space_clip(C);
  return space_clip->overlay.flag & SC_SHOW_OVERLAYS &&
         space_clip->mask_info.draw_flag & MASK_DRAWFLAG_SPLINE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Editing Functions
 * \{ */

void ED_space_clip_get_size(const SpaceClip *sc, int *r_width, int *r_height)
{
  if (sc->clip) {
    BKE_movieclip_get_size(sc->clip, &sc->user, r_width, r_height);
  }
  else {
    *r_width = *r_height = IMG_SIZE_FALLBACK;
  }
}

void ED_space_clip_get_size_fl(const SpaceClip *sc, float r_size[2])
{
  int size_i[2];
  ED_space_clip_get_size(sc, &size_i[0], &size_i[1]);
  r_size[0] = size_i[0];
  r_size[1] = size_i[1];
}

void ED_space_clip_get_zoom(const SpaceClip *sc,
                            const ARegion *region,
                            float *r_zoomx,
                            float *r_zoomy)
{
  int width, height;

  ED_space_clip_get_size(sc, &width, &height);

  *r_zoomx = float(BLI_rcti_size_x(&region->winrct) + 1) /
             (BLI_rctf_size_x(&region->v2d.cur) * width);
  *r_zoomy = float(BLI_rcti_size_y(&region->winrct) + 1) /
             (BLI_rctf_size_y(&region->v2d.cur) * height);
}

void ED_space_clip_get_aspect(const SpaceClip *sc, float *r_aspx, float *r_aspy)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if (clip) {
    BKE_movieclip_get_aspect(clip, r_aspx, r_aspy);
  }
  else {
    *r_aspx = *r_aspy = 1.0f;
  }

  if (*r_aspx < *r_aspy) {
    *r_aspy = *r_aspy / *r_aspx;
    *r_aspx = 1.0f;
  }
  else {
    *r_aspx = *r_aspx / *r_aspy;
    *r_aspy = 1.0f;
  }
}

void ED_space_clip_get_aspect_dimension_aware(const SpaceClip *sc, float *r_aspx, float *r_aspy)
{
  int w, h;

  /* most of tools does not require aspect to be returned with dimensions correction
   * due to they're invariant to this stuff, but some transformation tools like rotation
   * should be aware of aspect correction caused by different resolution in different
   * directions.
   * mainly this is used for transformation stuff
   */

  if (!sc->clip) {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;

    return;
  }

  ED_space_clip_get_aspect(sc, r_aspx, r_aspy);
  BKE_movieclip_get_size(sc->clip, &sc->user, &w, &h);

  *r_aspx *= float(w);
  *r_aspy *= float(h);

  if (*r_aspx < *r_aspy) {
    *r_aspy = *r_aspy / *r_aspx;
    *r_aspx = 1.0f;
  }
  else {
    *r_aspx = *r_aspx / *r_aspy;
    *r_aspy = 1.0f;
  }
}

int ED_space_clip_get_clip_frame_number(const SpaceClip *sc)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);

  /* Caller must ensure space does have a valid clip, otherwise it will crash, see #45017. */
  return BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr);
}

ImBuf *ED_space_clip_get_buffer(const SpaceClip *sc)
{
  if (sc->clip) {
    ImBuf *ibuf;

    ibuf = BKE_movieclip_get_postprocessed_ibuf(sc->clip, &sc->user, sc->postproc_flag);

    if (ibuf && (ibuf->byte_buffer.data || ibuf->float_buffer.data)) {
      return ibuf;
    }

    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }
  }

  return nullptr;
}

ImBuf *ED_space_clip_get_stable_buffer(const SpaceClip *sc,
                                       float loc[2],
                                       float *scale,
                                       float *angle)
{
  if (sc->clip) {
    ImBuf *ibuf;

    ibuf = BKE_movieclip_get_stable_ibuf(
        sc->clip, &sc->user, sc->postproc_flag, loc, scale, angle);

    if (ibuf && (ibuf->byte_buffer.data || ibuf->float_buffer.data)) {
      return ibuf;
    }

    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }
  }

  return nullptr;
}

bool ED_space_clip_get_position(const SpaceClip *sc,
                                const ARegion *region,
                                const int mval[2],
                                float r_fpos[2])
{
  ImBuf *ibuf = ED_space_clip_get_buffer(sc);
  if (!ibuf) {
    return false;
  }

  /* map the mouse coords to the backdrop image space */
  ED_clip_mouse_pos(sc, region, mval, r_fpos);

  IMB_freeImBuf(ibuf);
  return true;
}

bool ED_space_clip_color_sample(const SpaceClip *sc,
                                const ARegion *region,
                                const int mval[2],
                                float r_col[3])
{
  ImBuf *ibuf;
  float fx, fy, co[2];
  bool ret = false;

  ibuf = ED_space_clip_get_buffer(sc);
  if (!ibuf) {
    return false;
  }

  /* map the mouse coords to the backdrop image space */
  ED_clip_mouse_pos(sc, region, mval, co);

  fx = co[0];
  fy = co[1];

  if (fx >= 0.0f && fy >= 0.0f && fx < 1.0f && fy < 1.0f) {
    const float *fp;
    uchar *cp;
    int x = int(fx * ibuf->x), y = int(fy * ibuf->y);

    CLAMP(x, 0, ibuf->x - 1);
    CLAMP(y, 0, ibuf->y - 1);

    if (ibuf->float_buffer.data) {
      fp = (ibuf->float_buffer.data + (ibuf->channels) * (y * ibuf->x + x));
      copy_v3_v3(r_col, fp);
      ret = true;
    }
    else if (ibuf->byte_buffer.data) {
      cp = ibuf->byte_buffer.data + 4 * (y * ibuf->x + x);
      rgb_uchar_to_float(r_col, cp);
      IMB_colormanagement_colorspace_to_scene_linear_v3(r_col, ibuf->byte_buffer.colorspace);
      ret = true;
    }
  }

  IMB_freeImBuf(ibuf);

  return ret;
}

void ED_clip_update_frame(const Main *mainp, int cfra)
{
  /* image window, compo node users */
  LISTBASE_FOREACH (wmWindowManager *, wm, &mainp->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      bScreen *screen = WM_window_get_active_screen(win);

      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        if (area->spacetype == SPACE_CLIP) {
          SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);

          sc->scopes.ok = false;

          BKE_movieclip_user_set_frame(&sc->user, cfra);
        }
      }
    }
  }
}

bool ED_clip_view_selection(const bContext *C, const ARegion * /*region*/, bool fit)
{
  float offset_x, offset_y;
  float zoom;
  if (!clip_view_calculate_view_selection(C, fit, &offset_x, &offset_y, &zoom)) {
    return false;
  }

  SpaceClip *sc = CTX_wm_space_clip(C);
  sc->xof = offset_x;
  sc->yof = offset_y;
  sc->zoom = zoom;

  return true;
}

void ED_clip_select_all(const SpaceClip *sc, int action, bool *r_has_selection)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  bool has_selection = false;

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (!TRACK_VIEW_SELECTED(sc, track)) {
        continue;
      }

      const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

      if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
        action = SEL_DESELECT;
        break;
      }
    }

    LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
      if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (track->flag & TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
      switch (action) {
        case SEL_SELECT:
          track->flag |= SELECT;
          track->pat_flag |= SELECT;
          track->search_flag |= SELECT;
          break;
        case SEL_DESELECT:
          track->flag &= ~SELECT;
          track->pat_flag &= ~SELECT;
          track->search_flag &= ~SELECT;
          break;
        case SEL_INVERT:
          track->flag ^= SELECT;
          track->pat_flag ^= SELECT;
          track->search_flag ^= SELECT;
          break;
      }
    }

    if (TRACK_VIEW_SELECTED(sc, track)) {
      has_selection = true;
    }
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if (plane_track->flag & PLANE_TRACK_HIDDEN) {
      continue;
    }

    switch (action) {
      case SEL_SELECT:
        plane_track->flag |= SELECT;
        break;
      case SEL_DESELECT:
        plane_track->flag &= ~SELECT;
        break;
      case SEL_INVERT:
        plane_track->flag ^= SELECT;
        break;
    }
    if (plane_track->flag & SELECT) {
      has_selection = true;
    }
  }

  if (r_has_selection) {
    *r_has_selection = has_selection;
  }
}

void ED_clip_point_undistorted_pos(const SpaceClip *sc, const float co[2], float r_co[2])
{
  copy_v2_v2(r_co, co);

  if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    float aspy = 1.0f / clip->tracking.camera.pixel_aspect;
    int width, height;

    BKE_movieclip_get_size(sc->clip, &sc->user, &width, &height);

    r_co[0] *= width;
    r_co[1] *= height * aspy;

    BKE_tracking_undistort_v2(&clip->tracking, width, height, r_co, r_co);

    r_co[0] /= width;
    r_co[1] /= height * aspy;
  }
}

void ED_clip_point_stable_pos(
    const SpaceClip *sc, const ARegion *region, float x, float y, float *xr, float *yr)
{
  int sx, sy, width, height;
  float zoomx, zoomy, pos[3], imat[4][4];

  ED_space_clip_get_zoom(sc, region, &zoomx, &zoomy);
  ED_space_clip_get_size(sc, &width, &height);

  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &sx, &sy);

  pos[0] = (x - sx) / zoomx;
  pos[1] = (y - sy) / zoomy;
  pos[2] = 0.0f;

  invert_m4_m4(imat, sc->stabmat);
  mul_v3_m4v3(pos, imat, pos);

  *xr = pos[0] / width;
  *yr = pos[1] / height;

  if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    if (clip != nullptr) {
      MovieTracking *tracking = &clip->tracking;
      float aspy = 1.0f / tracking->camera.pixel_aspect;
      float tmp[2] = {*xr * width, *yr * height * aspy};

      BKE_tracking_distort_v2(tracking, width, height, tmp, tmp);

      *xr = tmp[0] / width;
      *yr = tmp[1] / (height * aspy);
    }
  }
}

void ED_clip_point_stable_pos__reverse(const SpaceClip *sc,
                                       const ARegion *region,
                                       const float co[2],
                                       float r_co[2])
{
  float zoomx, zoomy;
  float pos[3];
  int width, height;
  int sx, sy;

  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &sx, &sy);
  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_zoom(sc, region, &zoomx, &zoomy);

  ED_clip_point_undistorted_pos(sc, co, pos);
  pos[2] = 0.0f;

  /* untested */
  mul_v3_m4v3(pos, sc->stabmat, pos);

  r_co[0] = (pos[0] * width * zoomx) + float(sx);
  r_co[1] = (pos[1] * height * zoomy) + float(sy);
}

void ED_clip_mouse_pos(const SpaceClip *sc,
                       const ARegion *region,
                       const int mval[2],
                       float r_co[2])
{
  ED_clip_point_stable_pos(sc, region, mval[0], mval[1], &r_co[0], &r_co[1]);
}

bool ED_space_clip_check_show_trackedit(const SpaceClip *sc)
{
  if (sc) {
    return sc->mode == SC_MODE_TRACKING;
  }

  return false;
}

bool ED_space_clip_check_show_maskedit(const SpaceClip *sc)
{
  if (sc) {
    return sc->mode == SC_MODE_MASKEDIT;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clip Editing Functions
 * \{ */

MovieClip *ED_space_clip_get_clip(const SpaceClip *sc)
{
  return sc->clip;
}

void ED_space_clip_set_clip(bContext *C, bScreen *screen, SpaceClip *sc, MovieClip *clip)
{
  MovieClip *old_clip;
  bool old_clip_visible = false;

  if (!screen && C) {
    screen = CTX_wm_screen(C);
  }

  old_clip = sc->clip;
  sc->clip = clip;

  id_us_ensure_real((ID *)sc->clip);

  if (screen && sc->view == SC_VIEW_CLIP) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_CLIP) {
          SpaceClip *cur_sc = (SpaceClip *)sl;

          if (cur_sc != sc) {
            if (cur_sc->view == SC_VIEW_CLIP) {
              if (cur_sc->clip == old_clip) {
                old_clip_visible = true;
              }
            }
            else {
              if (ELEM(cur_sc->clip, old_clip, nullptr)) {
                cur_sc->clip = clip;
              }
            }
          }
        }
      }
    }
  }

  /* If clip is no longer visible on screen, free memory used by its cache */
  if (old_clip && old_clip != clip && !old_clip_visible) {
    BKE_movieclip_clear_cache(old_clip);
  }

  if (C) {
    WM_event_add_notifier(C, NC_MOVIECLIP | NA_SELECTED, sc->clip);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Masking Editing Functions
 * \{ */

Mask *ED_space_clip_get_mask(const SpaceClip *sc)
{
  return sc->mask_info.mask;
}

void ED_space_clip_set_mask(bContext *C, SpaceClip *sc, Mask *mask)
{
  sc->mask_info.mask = mask;

  id_us_ensure_real((ID *)sc->mask_info.mask);

  if (C) {
    WM_event_add_notifier(C, NC_MASK | NA_SELECTED, mask);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pre-Fetching Functions
 * \{ */

struct PrefetchJob {
  /** Clip into which cache the frames will be pre-fetched into. */
  MovieClip *clip;

  /* Local copy of the clip which is used to decouple reading in a way which does not require
   * threading lock which might "conflict" with the main thread,
   *
   * Used, for example, for animation prefetching (`clip->anim` can not be used from multiple
   * threads and main thread might need it). */
  MovieClip *clip_local;

  int start_frame, current_frame, end_frame;
  short render_size, render_flag;
};

struct PrefetchQueue {
  int initial_frame, current_frame, start_frame, end_frame;
  short render_size, render_flag;

  /* If true pre-fetching goes forward in time,
   * otherwise it goes backwards in time (starting from current frame).
   */
  bool forward;

  blender::Mutex mutex;

  bool *stop;
  bool *do_update;
  float *progress;
};

/* check whether pre-fetching is allowed */
static bool check_prefetch_break()
{
  return G.is_break;
}

/* read file for specified frame number to the memory */
static uchar *prefetch_read_file_to_memory(
    MovieClip *clip, int current_frame, short render_size, short render_flag, size_t *r_size)
{
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  user.framenr = current_frame;
  user.render_size = render_size;
  user.render_flag = render_flag;

  char filepath[FILE_MAX];
  BKE_movieclip_filepath_for_frame(clip, &user, filepath);

  int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }

  const size_t size = BLI_file_descriptor_size(file);
  if (UNLIKELY(ELEM(size, 0, size_t(-1)))) {
    close(file);
    return nullptr;
  }

  uchar *mem = MEM_calloc_arrayN<uchar>(size, "movieclip prefetch memory file");
  if (mem == nullptr) {
    close(file);
    return nullptr;
  }

  if (BLI_read(file, mem, size) != size) {
    close(file);
    MEM_freeN(mem);
    return nullptr;
  }

  *r_size = size;

  close(file);

  return mem;
}

/* find first uncached frame within prefetching frame range */
static int prefetch_find_uncached_frame(MovieClip *clip,
                                        int from_frame,
                                        int end_frame,
                                        short render_size,
                                        short render_flag,
                                        short direction)
{
  int current_frame;
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);

  user.render_size = render_size;
  user.render_flag = render_flag;

  if (direction > 0) {
    for (current_frame = from_frame; current_frame <= end_frame; current_frame++) {
      user.framenr = current_frame;

      if (!BKE_movieclip_has_cached_frame(clip, &user)) {
        break;
      }
    }
  }
  else {
    for (current_frame = from_frame; current_frame >= end_frame; current_frame--) {
      user.framenr = current_frame;

      if (!BKE_movieclip_has_cached_frame(clip, &user)) {
        break;
      }
    }
  }

  return current_frame;
}

/* get memory buffer for first uncached frame within prefetch frame range */
static uchar *prefetch_thread_next_frame(PrefetchQueue *queue,
                                         MovieClip *clip,
                                         size_t *r_size,
                                         int *r_current_frame)
{
  uchar *mem = nullptr;

  std::lock_guard lock(queue->mutex);
  if (!*queue->stop && !check_prefetch_break() &&
      IN_RANGE_INCL(queue->current_frame, queue->start_frame, queue->end_frame))
  {
    int current_frame;

    if (queue->forward) {
      current_frame = prefetch_find_uncached_frame(clip,
                                                   queue->current_frame + 1,
                                                   queue->end_frame,
                                                   queue->render_size,
                                                   queue->render_flag,
                                                   1);
      /* switch direction if read frames from current up to scene end frames */
      if (current_frame > queue->end_frame) {
        queue->current_frame = queue->initial_frame;
        queue->forward = false;
      }
    }

    if (!queue->forward) {
      current_frame = prefetch_find_uncached_frame(clip,
                                                   queue->current_frame - 1,
                                                   queue->start_frame,
                                                   queue->render_size,
                                                   queue->render_flag,
                                                   -1);
    }

    if (IN_RANGE_INCL(current_frame, queue->start_frame, queue->end_frame)) {
      int frames_processed;

      mem = prefetch_read_file_to_memory(
          clip, current_frame, queue->render_size, queue->render_flag, r_size);

      *r_current_frame = current_frame;

      queue->current_frame = current_frame;

      if (queue->forward) {
        frames_processed = queue->current_frame - queue->initial_frame;
      }
      else {
        frames_processed = (queue->end_frame - queue->initial_frame) +
                           (queue->initial_frame - queue->current_frame);
      }

      *queue->do_update = true;
      *queue->progress = float(frames_processed) / (queue->end_frame - queue->start_frame);
    }
  }

  return mem;
}

static void prefetch_task_func(TaskPool *__restrict pool, void *task_data)
{
  PrefetchQueue *queue = (PrefetchQueue *)BLI_task_pool_user_data(pool);
  MovieClip *clip = (MovieClip *)task_data;
  uchar *mem;
  size_t size;
  int current_frame;

  while ((mem = prefetch_thread_next_frame(queue, clip, &size, &current_frame))) {
    ImBuf *ibuf;
    MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
    int flag = IB_byte_data | IB_multilayer | IB_alphamode_detect | IB_metadata;
    int result;
    char *colorspace_name = nullptr;
    const bool use_proxy = (clip->flag & MCLIP_USE_PROXY) &&
                           (queue->render_size != MCLIP_PROXY_RENDER_SIZE_FULL);

    user.framenr = current_frame;
    user.render_size = queue->render_size;
    user.render_flag = queue->render_flag;

    /* Proxies are stored in the display space. */
    if (!use_proxy) {
      colorspace_name = clip->colorspace_settings.name;
    }

    ibuf = IMB_load_image_from_memory(mem, size, flag, "prefetch frame", nullptr, colorspace_name);
    if (ibuf == nullptr) {
      continue;
    }
    BKE_movieclip_convert_multilayer_ibuf(ibuf);

    result = BKE_movieclip_put_frame_if_possible(clip, &user, ibuf);

    IMB_freeImBuf(ibuf);

    MEM_freeN(mem);

    if (!result) {
      /* no more space in the cache, stop reading frames */
      *queue->stop = true;
      break;
    }
  }
}

static void start_prefetch_threads(MovieClip *clip,
                                   int start_frame,
                                   int current_frame,
                                   int end_frame,
                                   short render_size,
                                   short render_flag,
                                   bool *stop,
                                   bool *do_update,
                                   float *progress)
{
  int tot_thread = BLI_task_scheduler_num_threads();

  /* initialize queue */
  PrefetchQueue queue;
  queue.current_frame = current_frame;
  queue.initial_frame = current_frame;
  queue.start_frame = start_frame;
  queue.end_frame = end_frame;
  queue.render_size = render_size;
  queue.render_flag = render_flag;
  queue.forward = true;

  queue.stop = stop;
  queue.do_update = do_update;
  queue.progress = progress;

  TaskPool *task_pool = BLI_task_pool_create(&queue, TASK_PRIORITY_LOW);
  for (int i = 0; i < tot_thread; i++) {
    BLI_task_pool_push(task_pool, prefetch_task_func, clip, false, nullptr);
  }
  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);
}

/* NOTE: Reading happens from `clip_local` into `clip->cache`. */
static bool prefetch_movie_frame(MovieClip *clip,
                                 MovieClip *clip_local,
                                 int frame,
                                 short render_size,
                                 short render_flag,
                                 bool *stop)
{
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);

  if (check_prefetch_break() || *stop) {
    return false;
  }

  user.framenr = frame;
  user.render_size = render_size;
  user.render_flag = render_flag;

  if (!BKE_movieclip_has_cached_frame(clip, &user)) {
    ImBuf *ibuf = BKE_movieclip_anim_ibuf_for_frame_no_lock(clip_local, &user);

    if (ibuf) {
      int result;

      result = BKE_movieclip_put_frame_if_possible(clip, &user, ibuf);

      if (!result) {
        /* no more space in the cache, we could stop prefetching here */
        *stop = true;
      }

      IMB_freeImBuf(ibuf);
    }
    else {
      /* error reading frame, fair enough stop attempting further reading */
      *stop = true;
    }
  }

  return true;
}

static void do_prefetch_movie(MovieClip *clip,
                              MovieClip *clip_local,
                              int start_frame,
                              int current_frame,
                              int end_frame,
                              short render_size,
                              short render_flag,
                              bool *stop,
                              bool *do_update,
                              float *progress)
{
  int frame;
  int frames_processed = 0;

  /* read frames starting from current frame up to scene end frame */
  for (frame = current_frame; frame <= end_frame; frame++) {
    if (!prefetch_movie_frame(clip, clip_local, frame, render_size, render_flag, stop)) {
      return;
    }

    frames_processed++;

    *do_update = true;
    *progress = float(frames_processed) / (end_frame - start_frame);
  }

  /* read frames starting from current frame up to scene start frame */
  for (frame = current_frame; frame >= start_frame; frame--) {
    if (!prefetch_movie_frame(clip, clip_local, frame, render_size, render_flag, stop)) {
      return;
    }

    frames_processed++;

    *do_update = true;
    *progress = float(frames_processed) / (end_frame - start_frame);
  }
}

static void prefetch_startjob(void *pjv, wmJobWorkerStatus *worker_status)
{
  PrefetchJob *pj = static_cast<PrefetchJob *>(pjv);

  if (pj->clip->source == MCLIP_SRC_SEQUENCE) {
    /* read sequence files in multiple threads */
    start_prefetch_threads(pj->clip,
                           pj->start_frame,
                           pj->current_frame,
                           pj->end_frame,
                           pj->render_size,
                           pj->render_flag,
                           &worker_status->stop,
                           &worker_status->do_update,
                           &worker_status->progress);
  }
  else if (pj->clip->source == MCLIP_SRC_MOVIE) {
    /* read movie in a single thread */
    do_prefetch_movie(pj->clip,
                      pj->clip_local,
                      pj->start_frame,
                      pj->current_frame,
                      pj->end_frame,
                      pj->render_size,
                      pj->render_flag,
                      &worker_status->stop,
                      &worker_status->do_update,
                      &worker_status->progress);
  }
  else {
    BLI_assert_msg(0, "Unknown movie clip source when prefetching frames");
  }
}

static void prefetch_freejob(void *pjv)
{
  PrefetchJob *pj = static_cast<PrefetchJob *>(pjv);

  MovieClip *clip_local = pj->clip_local;
  if (clip_local != nullptr) {
    BKE_libblock_free_datablock(&clip_local->id, 0);
    BKE_libblock_free_data(&clip_local->id, false);
    BLI_assert(!clip_local->id.py_instance); /* Or call #BKE_libblock_free_data_py. */
    MEM_freeN(clip_local);
  }

  MEM_freeN(pj);
}

static int prefetch_get_start_frame(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  return scene->r.sfra;
}

static int prefetch_get_final_frame(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int end_frame;

  /* check whether all the frames from prefetch range are cached */
  end_frame = scene->r.efra;

  if (clip->len) {
    end_frame = min_ii(end_frame, scene->r.sfra + clip->len - 1);
  }

  return end_frame;
}

/* returns true if early out is possible */
static bool prefetch_check_early_out(const bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int first_uncached_frame, end_frame;
  int clip_len;

  if (clip == nullptr) {
    return true;
  }

  clip_len = BKE_movieclip_get_duration(clip);

  /* check whether all the frames from prefetch range are cached */
  end_frame = prefetch_get_final_frame(C);

  first_uncached_frame = prefetch_find_uncached_frame(
      clip, sc->user.framenr, end_frame, sc->user.render_size, sc->user.render_flag, 1);

  if (first_uncached_frame > end_frame || first_uncached_frame == clip_len) {
    int start_frame = prefetch_get_start_frame(C);

    first_uncached_frame = prefetch_find_uncached_frame(
        clip, sc->user.framenr, start_frame, sc->user.render_size, sc->user.render_flag, -1);

    if (first_uncached_frame < start_frame) {
      return true;
    }
  }

  return false;
}

void clip_start_prefetch_job(const bContext *C)
{
  wmJob *wm_job;
  PrefetchJob *pj;
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (prefetch_check_early_out(C)) {
    return;
  }

  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       CTX_data_scene(C),
                       "Prefetching...",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_CLIP_PREFETCH);

  /* create new job */
  pj = MEM_callocN<PrefetchJob>("prefetch job");
  pj->clip = ED_space_clip_get_clip(sc);
  pj->start_frame = prefetch_get_start_frame(C);
  pj->current_frame = sc->user.framenr;
  pj->end_frame = prefetch_get_final_frame(C);
  pj->render_size = sc->user.render_size;
  pj->render_flag = sc->user.render_flag;

  /* Create a local copy of the clip, so that video file (clip->anim) access can happen without
   * acquiring the lock which will interfere with the main thread. */
  if (pj->clip->source == MCLIP_SRC_MOVIE) {
    BKE_id_copy_ex(nullptr, (&pj->clip->id), (ID **)&pj->clip_local, LIB_ID_COPY_LOCALIZE);
  }

  WM_jobs_customdata_set(wm_job, pj, prefetch_freejob);
  WM_jobs_timer(wm_job, 0.2, NC_MOVIECLIP | ND_DISPLAY, 0);
  WM_jobs_callbacks(wm_job, prefetch_startjob, nullptr, nullptr, nullptr);

  G.is_break = false;

  /* and finally start the job */
  WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void ED_clip_view_lock_state_store(const bContext *C, ClipViewLockState *state)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  BLI_assert(space_clip != nullptr);

  state->offset_x = space_clip->xof;
  state->offset_y = space_clip->yof;
  state->zoom = space_clip->zoom;

  state->lock_offset_x = 0.0f;
  state->lock_offset_y = 0.0f;

  if ((space_clip->flag & SC_LOCK_SELECTION) == 0) {
    return;
  }

  if (!clip_view_calculate_view_selection(
          C, false, &state->offset_x, &state->offset_y, &state->zoom))
  {
    return;
  }

  state->lock_offset_x = space_clip->xlockof;
  state->lock_offset_y = space_clip->ylockof;
}

void ED_clip_view_lock_state_restore_no_jump(const bContext *C, const ClipViewLockState *state)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  BLI_assert(space_clip != nullptr);

  if ((space_clip->flag & SC_LOCK_SELECTION) == 0) {
    return;
  }

  float offset_x, offset_y;
  float zoom;
  if (!clip_view_calculate_view_selection(C, false, &offset_x, &offset_y, &zoom)) {
    return;
  }

  space_clip->xlockof = state->offset_x + state->lock_offset_x - offset_x;
  space_clip->ylockof = state->offset_y + state->lock_offset_y - offset_y;
}

/** \} */
