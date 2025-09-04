/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph.hh"

#include "clip_intern.hh" /* own include */
#include "tracking_ops_intern.hh"

/********************** Track operator *********************/

struct TrackMarkersJob {
  AutoTrackContext *context; /* Tracking context */
  int sfra, efra, lastfra;   /* Start, end and recently tracked frames */
  int backwards;             /* Backwards tracking flag */
  MovieClip *clip;           /* Clip which is tracking */
  float delay;               /* Delay in milliseconds to allow
                              * tracking at fixed FPS */

  wmWindowManager *wm;
  Main *main;
  Scene *scene;
  bScreen *screen;
};

static bool track_markers_testbreak()
{
  return G.is_break;
}

static int track_count_markers(SpaceClip *sc, MovieClip *clip, const int framenr)
{
  int tot = 0;
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    bool selected = (sc != nullptr) ? TRACK_VIEW_SELECTED(sc, track) : TRACK_SELECTED(track);
    if (selected && (track->flag & TRACK_LOCKED) == 0) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
      if (!marker || (marker->flag & MARKER_DISABLED) == 0) {
        tot++;
      }
    }
  }
  return tot;
}

static void track_init_markers(SpaceClip *sc,
                               MovieClip *clip,
                               const int framenr,
                               int *r_frames_limit)
{
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  int frames_limit = 0;
  if (sc != nullptr) {
    clip_tracking_clear_invisible_track_selection(sc, clip);
  }
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    bool selected = (sc != nullptr) ? TRACK_VIEW_SELECTED(sc, track) : TRACK_SELECTED(track);
    if (selected) {
      if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
        BKE_tracking_marker_ensure(track, framenr);
        if (track->frames_limit) {
          if (frames_limit == 0) {
            frames_limit = track->frames_limit;
          }
          else {
            frames_limit = min_ii(frames_limit, int(track->frames_limit));
          }
        }
      }
    }
  }
  *r_frames_limit = frames_limit;
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

static bool track_markers_initjob(bContext *C, TrackMarkersJob *tmj, bool backwards, bool sequence)
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
      tmj->efra = scene->r.sfra;
    }
    else {
      tmj->efra = scene->r.efra;
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
      tmj->efra = std::max(tmj->efra, tmj->sfra - frames_limit);
    }
    else {
      tmj->efra = std::min(tmj->efra, tmj->sfra + frames_limit);
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

  tmj->context = BKE_autotrack_context_new(clip, &sc->user, backwards);

  clip->tracking_context = tmj->context;

  tmj->lastfra = tmj->sfra;

  /* XXX: silly to store this, but this data is needed to update scene and
   *      movie-clip numbers when tracking is finished. This introduces
   *      better feedback for artists.
   *      Maybe there's another way to solve this problem,
   *      but can't think better way at the moment.
   *      Anyway, this way isn't more unstable as animation rendering
   *      animation which uses the same approach (except storing screen).
   */
  tmj->scene = scene;
  tmj->main = CTX_data_main(C);
  tmj->screen = CTX_wm_screen(C);

  tmj->wm = CTX_wm_manager(C);

  if (!track_markers_check_direction(backwards, tmj->sfra, tmj->efra)) {
    return false;
  }

  WM_locked_interface_set(tmj->wm, true);

  return true;
}

static void track_markers_startjob(void *tmv, wmJobWorkerStatus *worker_status)
{
  TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
  int framenr = tmj->sfra;

  BKE_autotrack_context_start(tmj->context);

  while (framenr != tmj->efra) {
    if (tmj->delay > 0) {
      /* Tracking should happen with fixed fps. Calculate time
       * using current timer value before tracking frame and after.
       *
       * Small (and maybe unneeded optimization): do not calculate
       * exec_time for "Fastest" tracking
       */

      double start_time = BLI_time_now_seconds(), exec_time;

      if (!BKE_autotrack_context_step(tmj->context)) {
        break;
      }

      exec_time = BLI_time_now_seconds() - start_time;
      if (tmj->delay > float(exec_time)) {
        BLI_time_sleep_ms(tmj->delay - float(exec_time));
      }
    }
    else if (!BKE_autotrack_context_step(tmj->context)) {
      break;
    }

    worker_status->do_update = true;
    worker_status->progress = float(framenr - tmj->sfra) / (tmj->efra - tmj->sfra);

    if (tmj->backwards) {
      framenr--;
    }
    else {
      framenr++;
    }

    tmj->lastfra = framenr;

    if (worker_status->stop || track_markers_testbreak()) {
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
  wmWindowManager *wm = static_cast<wmWindowManager *>(tmj->main->wm.first);

  tmj->clip->tracking_context = nullptr;
  tmj->scene->r.cfra = BKE_movieclip_remap_clip_to_scene_frame(tmj->clip, tmj->lastfra);
  if (wm != nullptr) {
    /* XXX */
    // ED_update_for_newframe(tmj->main, tmj->scene);
  }

  BKE_autotrack_context_sync(tmj->context);
  BKE_autotrack_context_finish(tmj->context);

  DEG_id_tag_update(&tmj->clip->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_FRAME, tmj->scene);
}

static void track_markers_freejob(void *tmv)
{
  TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
  tmj->clip->tracking_context = nullptr;
  WM_locked_interface_set(tmj->wm, false);
  BKE_autotrack_context_free(tmj->context);
  MEM_freeN(tmj);
}

static wmOperatorStatus track_markers(bContext *C, wmOperator *op, bool use_job)
{
  TrackMarkersJob *tmj;
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  wmJob *wm_job;
  bool backwards = RNA_boolean_get(op->ptr, "backwards");
  bool sequence = RNA_boolean_get(op->ptr, "sequence");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  if (WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_ANY)) {
    /* Only one tracking is allowed at a time. */
    return OPERATOR_CANCELLED;
  }

  if (clip->tracking_context) {
    return OPERATOR_CANCELLED;
  }

  if (track_count_markers(sc, clip, framenr) == 0) {
    return OPERATOR_CANCELLED;
  }

  tmj = MEM_callocN<TrackMarkersJob>("TrackMarkersJob data");
  if (!track_markers_initjob(C, tmj, backwards, sequence)) {
    track_markers_freejob(tmj);
    return OPERATOR_CANCELLED;
  }

  /* Setup job. */
  if (use_job && sequence) {
    wm_job = WM_jobs_get(CTX_wm_manager(C),
                         CTX_wm_window(C),
                         CTX_data_scene(C),
                         "Tracking markers...",
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
        wm_job, track_markers_startjob, nullptr, track_markers_updatejob, track_markers_endjob);

    G.is_break = false;

    WM_jobs_start(CTX_wm_manager(C), wm_job);
    WM_cursor_wait(false);

    /* Add modal handler for ESC. */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  wmJobWorkerStatus worker_status = {};
  track_markers_startjob(tmj, &worker_status);
  track_markers_endjob(tmj);
  track_markers_freejob(tmj);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus track_markers_exec(bContext *C, wmOperator *op)
{
  return track_markers(C, op, false);
}

static wmOperatorStatus track_markers_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  return track_markers(C, op, true);
}

static wmOperatorStatus track_markers_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  /* No running tracking, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_ANY)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* Running tracking. */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
    default: {
      break;
    }
  }

  return OPERATOR_PASS_THROUGH;
}

static std::string track_markers_get_description(bContext * /*C*/,
                                                 wmOperatorType * /*ot*/,
                                                 PointerRNA *ptr)
{
  const bool backwards = RNA_boolean_get(ptr, "backwards");
  const bool sequence = RNA_boolean_get(ptr, "sequence");

  if (backwards && sequence) {
    return TIP_("Track the selected markers backward for the entire clip");
  }
  if (backwards && !sequence) {
    return TIP_("Track the selected markers backward by one frame");
  }
  if (!backwards && sequence) {
    return TIP_("Track the selected markers forward for the entire clip");
  }
  if (!backwards && !sequence) {
    return TIP_("Track the selected markers forward by one frame");
  }

  /* Use default description. */
  return "";
}

void CLIP_OT_track_markers(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Track Markers";
  ot->description = "Track selected markers";
  ot->idname = "CLIP_OT_track_markers";

  /* API callbacks. */
  ot->exec = track_markers_exec;
  ot->invoke = track_markers_invoke;
  ot->modal = track_markers_modal;
  ot->poll = ED_space_clip_tracking_poll;
  ot->get_description = track_markers_get_description;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "backwards", false, "Backwards", "Do backwards tracking");
  RNA_def_boolean(ot->srna,
                  "sequence",
                  false,
                  "Track Sequence",
                  "Track marker during image sequence rather than "
                  "single image");
}

/********************** Refine track position operator *********************/

static wmOperatorStatus refine_marker_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const bool backwards = RNA_boolean_get(op->ptr, "backwards");
  const int framenr = ED_space_clip_get_clip_frame_number(sc);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
      BKE_tracking_refine_marker(clip, track, marker, backwards);
    }
  }

  DEG_id_tag_update(&clip->id, ID_RECALC_SYNC_TO_EVAL);
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

  /* API callbacks. */
  ot->exec = refine_marker_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "backwards", false, "Backwards", "Do backwards tracking");
}
