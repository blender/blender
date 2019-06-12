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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_main.h"
#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "clip_intern.h"  // own include
#include "tracking_ops_intern.h"

/********************** Track operator *********************/

typedef struct TrackMarkersJob {
  struct AutoTrackContext *context; /* Tracking context */
  int sfra, efra, lastfra;          /* Start, end and recently tracked frames */
  int backwards;                    /* Backwards tracking flag */
  MovieClip *clip;                  /* Clip which is tracking */
  float delay;                      /* Delay in milliseconds to allow
                                     * tracking at fixed FPS */

  struct Main *main;
  struct Scene *scene;
  struct bScreen *screen;
} TrackMarkersJob;

static bool track_markers_testbreak(void)
{
  return G.is_break;
}

static int track_count_markers(SpaceClip *sc, MovieClip *clip, int framenr)
{
  int tot = 0;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    bool selected = (sc != NULL) ? TRACK_VIEW_SELECTED(sc, track) : TRACK_SELECTED(track);
    if (selected && (track->flag & TRACK_LOCKED) == 0) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
      if (!marker || (marker->flag & MARKER_DISABLED) == 0) {
        tot++;
      }
    }
  }
  return tot;
}

static void track_init_markers(SpaceClip *sc, MovieClip *clip, int framenr, int *frames_limit_r)
{
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  int frames_limit = 0;
  if (sc != NULL) {
    clip_tracking_clear_invisible_track_selection(sc, clip);
  }
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    bool selected = (sc != NULL) ? TRACK_VIEW_SELECTED(sc, track) : TRACK_SELECTED(track);
    if (selected) {
      if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
        BKE_tracking_marker_ensure(track, framenr);
        if (track->frames_limit) {
          if (frames_limit == 0) {
            frames_limit = track->frames_limit;
          }
          else {
            frames_limit = min_ii(frames_limit, (int)track->frames_limit);
          }
        }
      }
    }
  }
  *frames_limit_r = frames_limit;
}

static bool track_markers_check_direction(int backwards, int curfra, int efra)
{
  if (backwards) {
    if (curfra < efra) {
      return false;
    }
  }
  else {
    if (curfra > efra) {
      return false;
    }
  }

  return true;
}

static int track_markers_initjob(bContext *C, TrackMarkersJob *tmj, bool backwards, bool sequence)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  Scene *scene = CTX_data_scene(C);
  MovieTrackingSettings *settings = &clip->tracking.settings;
  int frames_limit;
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  track_init_markers(sc, clip, framenr, &frames_limit);

  tmj->sfra = framenr;
  tmj->clip = clip;
  tmj->backwards = backwards;

  if (sequence) {
    if (backwards) {
      tmj->efra = SFRA;
    }
    else {
      tmj->efra = EFRA;
    }
    tmj->efra = BKE_movieclip_remap_scene_to_clip_frame(clip, tmj->efra);
  }
  else {
    if (backwards) {
      tmj->efra = tmj->sfra - 1;
    }
    else {
      tmj->efra = tmj->sfra + 1;
    }
  }

  /* Limit frames to be tracked by user setting. */
  if (frames_limit) {
    if (backwards) {
      tmj->efra = MAX2(tmj->efra, tmj->sfra - frames_limit);
    }
    else {
      tmj->efra = MIN2(tmj->efra, tmj->sfra + frames_limit);
    }
  }

  if (settings->speed != TRACKING_SPEED_FASTEST) {
    tmj->delay = 1.0f / scene->r.frs_sec * 1000.0f;

    if (settings->speed == TRACKING_SPEED_HALF) {
      tmj->delay *= 2;
    }
    else if (settings->speed == TRACKING_SPEED_QUARTER) {
      tmj->delay *= 4;
    }
    else if (settings->speed == TRACKING_SPEED_DOUBLE) {
      tmj->delay /= 2;
    }
  }

  tmj->context = BKE_autotrack_context_new(clip, &sc->user, backwards, true);

  clip->tracking_context = tmj->context;

  tmj->lastfra = tmj->sfra;

  /* XXX: silly to store this, but this data is needed to update scene and
   *      movie-clip numbers when tracking is finished. This introduces
   *      better feedback for artists.
   *      Maybe there's another way to solve this problem, but can't think
   *      better way atm.
   *      Anyway, this way isn't more unstable as animation rendering
   *      animation which uses the same approach (except storing screen).
   */
  tmj->scene = scene;
  tmj->main = CTX_data_main(C);
  tmj->screen = CTX_wm_screen(C);

  return track_markers_check_direction(backwards, tmj->sfra, tmj->efra);
}

static void track_markers_startjob(void *tmv, short *stop, short *do_update, float *progress)
{
  TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
  int framenr = tmj->sfra;

  while (framenr != tmj->efra) {
    if (tmj->delay > 0) {
      /* Tracking should happen with fixed fps. Calculate time
       * using current timer value before tracking frame and after.
       *
       * Small (and maybe unneeded optimization): do not calculate
       * exec_time for "Fastest" tracking
       */

      double start_time = PIL_check_seconds_timer(), exec_time;

      if (!BKE_autotrack_context_step(tmj->context)) {
        break;
      }

      exec_time = PIL_check_seconds_timer() - start_time;
      if (tmj->delay > (float)exec_time) {
        PIL_sleep_ms(tmj->delay - (float)exec_time);
      }
    }
    else if (!BKE_autotrack_context_step(tmj->context)) {
      break;
    }

    *do_update = true;
    *progress = (float)(framenr - tmj->sfra) / (tmj->efra - tmj->sfra);

    if (tmj->backwards) {
      framenr--;
    }
    else {
      framenr++;
    }

    tmj->lastfra = framenr;

    if (*stop || track_markers_testbreak()) {
      break;
    }
  }
}

static void track_markers_updatejob(void *tmv)
{
  TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
  BKE_autotrack_context_sync(tmj->context);
}

static void track_markers_endjob(void *tmv)
{
  TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
  wmWindowManager *wm = tmj->main->wm.first;

  tmj->clip->tracking_context = NULL;
  tmj->scene->r.cfra = BKE_movieclip_remap_clip_to_scene_frame(tmj->clip, tmj->lastfra);
  if (wm != NULL) {
    // XXX: ...
    // ED_update_for_newframe(tmj->main, tmj->scene);
  }

  BKE_autotrack_context_sync(tmj->context);
  BKE_autotrack_context_finish(tmj->context);

  DEG_id_tag_update(&tmj->clip->id, ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_SCENE | ND_FRAME, tmj->scene);
}

static void track_markers_freejob(void *tmv)
{
  TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
  tmj->clip->tracking_context = NULL;
  BKE_autotrack_context_free(tmj->context);
  MEM_freeN(tmj);
}

static int track_markers(bContext *C, wmOperator *op, bool use_job)
{
  TrackMarkersJob *tmj;
  ScrArea *sa = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  wmJob *wm_job;
  bool backwards = RNA_boolean_get(op->ptr, "backwards");
  bool sequence = RNA_boolean_get(op->ptr, "sequence");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  if (WM_jobs_test(CTX_wm_manager(C), sa, WM_JOB_TYPE_ANY)) {
    /* Only one tracking is allowed at a time. */
    return OPERATOR_CANCELLED;
  }

  if (clip->tracking_context) {
    return OPERATOR_CANCELLED;
  }

  if (track_count_markers(sc, clip, framenr) == 0) {
    return OPERATOR_CANCELLED;
  }

  tmj = MEM_callocN(sizeof(TrackMarkersJob), "TrackMarkersJob data");
  if (!track_markers_initjob(C, tmj, backwards, sequence)) {
    track_markers_freejob(tmj);
    return OPERATOR_CANCELLED;
  }

  /* Setup job. */
  if (use_job && sequence) {
    wm_job = WM_jobs_get(CTX_wm_manager(C),
                         CTX_wm_window(C),
                         sa,
                         "Track Markers",
                         WM_JOB_PROGRESS,
                         WM_JOB_TYPE_CLIP_TRACK_MARKERS);
    WM_jobs_customdata_set(wm_job, tmj, track_markers_freejob);

    /* If there's delay set in tracking job, tracking should happen
     * with fixed FPS. To deal with editor refresh we have to synchronize
     * tracks from job and tracks in clip. Do this in timer callback
     * to prevent threading conflicts. */
    if (tmj->delay > 0) {
      WM_jobs_timer(wm_job, tmj->delay / 1000.0f, NC_MOVIECLIP | NA_EVALUATED, 0);
    }
    else {
      WM_jobs_timer(wm_job, 0.2, NC_MOVIECLIP | NA_EVALUATED, 0);
    }

    WM_jobs_callbacks(
        wm_job, track_markers_startjob, NULL, track_markers_updatejob, track_markers_endjob);

    G.is_break = false;

    WM_jobs_start(CTX_wm_manager(C), wm_job);
    WM_cursor_wait(0);

    /* Add modal handler for ESC. */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  else {
    short stop = 0, do_update = 0;
    float progress = 0.0f;
    track_markers_startjob(tmj, &stop, &do_update, &progress);
    track_markers_endjob(tmj);
    track_markers_freejob(tmj);
    return OPERATOR_FINISHED;
  }
}

static int track_markers_exec(bContext *C, wmOperator *op)
{
  return track_markers(C, op, false);
}

static int track_markers_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return track_markers(C, op, true);
}

static int track_markers_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  /* No running tracking, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_ANY)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* Running tracking. */
  switch (event->type) {
    case ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_track_markers(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Track Markers";
  ot->description = "Track selected markers";
  ot->idname = "CLIP_OT_track_markers";

  /* api callbacks */
  ot->exec = track_markers_exec;
  ot->invoke = track_markers_invoke;
  ot->modal = track_markers_modal;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "backwards", 0, "Backwards", "Do backwards tracking");
  RNA_def_boolean(ot->srna,
                  "sequence",
                  0,
                  "Track Sequence",
                  "Track marker during image sequence rather than "
                  "single image");
}

/********************** Refine track position operator *********************/

static int refine_marker_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  bool backwards = RNA_boolean_get(op->ptr, "backwards");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
      BKE_tracking_refine_marker(clip, track, marker, backwards);
    }
  }

  DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_refine_markers(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Refine Markers";
  ot->description =
      "Refine selected markers positions "
      "by running the tracker from track's reference "
      "to current frame";
  ot->idname = "CLIP_OT_refine_markers";

  /* api callbacks */
  ot->exec = refine_marker_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "backwards", 0, "Backwards", "Do backwards tracking");
}
