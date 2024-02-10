/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h" /* SELECT */
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_report.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"

#include "clip_intern.h"

/********************** solve camera operator *********************/

struct SolveCameraJob {
  wmWindowManager *wm;
  Scene *scene;
  MovieClip *clip;
  MovieClipUser user;

  ReportList *reports;

  char stats_message[256];

  MovieReconstructContext *context;
};

static bool solve_camera_initjob(
    bContext *C, SolveCameraJob *scj, wmOperator *op, char *error_msg, int max_error)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  Scene *scene = CTX_data_scene(C);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  int width, height;

  if (!BKE_tracking_reconstruction_check(tracking, tracking_object, error_msg, max_error)) {
    return false;
  }

  /* Could fail if footage uses images with different sizes. */
  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  scj->wm = CTX_wm_manager(C);
  scj->clip = clip;
  scj->scene = scene;
  scj->reports = op->reports;
  scj->user = sc->user;

  scj->context = BKE_tracking_reconstruction_context_new(clip,
                                                         tracking_object,
                                                         tracking_object->keyframe1,
                                                         tracking_object->keyframe2,
                                                         width,
                                                         height);

  tracking->stats = MEM_cnew<MovieTrackingStats>("solve camera stats");

  WM_set_locked_interface(scj->wm, true);

  return true;
}

static void solve_camera_updatejob(void *scv)
{
  SolveCameraJob *scj = (SolveCameraJob *)scv;
  MovieTracking *tracking = &scj->clip->tracking;

  STRNCPY(tracking->stats->message, scj->stats_message);
}

static void solve_camera_startjob(void *scv, wmJobWorkerStatus *worker_status)
{
  SolveCameraJob *scj = (SolveCameraJob *)scv;
  BKE_tracking_reconstruction_solve(scj->context,
                                    &worker_status->stop,
                                    &worker_status->do_update,
                                    &worker_status->progress,
                                    scj->stats_message,
                                    sizeof(scj->stats_message));
}

static void solve_camera_freejob(void *scv)
{
  SolveCameraJob *scj = (SolveCameraJob *)scv;
  MovieTracking *tracking = &scj->clip->tracking;
  Scene *scene = scj->scene;
  MovieClip *clip = scj->clip;
  int solved;

  /* WindowManager is missing in the job when initialization is incomplete.
   * In this case the interface is not locked either. */
  if (scj->wm != nullptr) {
    WM_set_locked_interface(scj->wm, false);
  }

  if (!scj->context) {
    /* job weren't fully initialized due to some error */
    MEM_freeN(scj);
    return;
  }

  solved = BKE_tracking_reconstruction_finish(scj->context, tracking);
  if (!solved) {
    const char *error_message = BKE_tracking_reconstruction_error_message_get(scj->context);
    if (error_message[0]) {
      BKE_report(scj->reports, RPT_ERROR, error_message);
    }
    else {
      BKE_report(
          scj->reports, RPT_WARNING, "Some data failed to reconstruct (see console for details)");
    }
  }
  else {
    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
    BKE_reportf(scj->reports,
                RPT_INFO,
                "Average re-projection error: %.2f px",
                tracking_object->reconstruction.error);
  }

  /* Set currently solved clip as active for scene. */
  if (scene->clip != nullptr) {
    id_us_min(&clip->id);
  }
  scene->clip = clip;
  id_us_plus(&clip->id);

  /* Set blender camera focal length so result would look fine there. */
  if (scene->camera != nullptr && scene->camera->data &&
      GS(((ID *)scene->camera->data)->name) == ID_CA)
  {
    Camera *camera = (Camera *)scene->camera->data;
    int width, height;
    BKE_movieclip_get_size(clip, &scj->user, &width, &height);
    BKE_tracking_camera_to_blender(tracking, scene, camera, width, height);
    DEG_id_tag_update(&camera->id, ID_RECALC_COPY_ON_WRITE);
    WM_main_add_notifier(NC_OBJECT, camera);
  }

  MEM_freeN(tracking->stats);
  tracking->stats = nullptr;

  DEG_id_tag_update(&clip->id, 0);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, nullptr);

  /* Update active clip displayed in scene buttons. */
  WM_main_add_notifier(NC_SCENE, scene);

  BKE_tracking_reconstruction_context_free(scj->context);
  MEM_freeN(scj);
}

static int solve_camera_exec(bContext *C, wmOperator *op)
{
  SolveCameraJob *scj;
  char error_msg[256] = "\0";
  scj = MEM_cnew<SolveCameraJob>("SolveCameraJob data");
  if (!solve_camera_initjob(C, scj, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    solve_camera_freejob(scj);
    return OPERATOR_CANCELLED;
  }
  wmJobWorkerStatus worker_status = {};
  solve_camera_startjob(scj, &worker_status);
  solve_camera_freejob(scj);
  return OPERATOR_FINISHED;
}

static int solve_camera_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  SolveCameraJob *scj;
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;
  wmJob *wm_job;
  char error_msg[256] = "\0";

  if (WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_CLIP_SOLVE_CAMERA)) {
    /* only one solve is allowed at a time */
    return OPERATOR_CANCELLED;
  }

  scj = MEM_cnew<SolveCameraJob>("SolveCameraJob data");
  if (!solve_camera_initjob(C, scj, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    solve_camera_freejob(scj);
    return OPERATOR_CANCELLED;
  }

  STRNCPY(tracking->stats->message, "Solving camera | Preparing solve");

  /* Hide reconstruction statistics from previous solve. */
  reconstruction->flag &= ~TRACKING_RECONSTRUCTED;
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  /* Setup job. */
  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       CTX_data_scene(C),
                       "Solve Camera",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_CLIP_SOLVE_CAMERA);
  WM_jobs_customdata_set(wm_job, scj, solve_camera_freejob);
  WM_jobs_timer(wm_job, 0.1, NC_MOVIECLIP | NA_EVALUATED, 0);
  WM_jobs_callbacks(wm_job, solve_camera_startjob, nullptr, solve_camera_updatejob, nullptr);

  G.is_break = false;

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_cursor_wait(false);

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int solve_camera_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  /* No running solver, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_CLIP_SOLVE_CAMERA)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* Running solver. */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_solve_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Solve Camera";
  ot->description = "Solve camera motion from tracks";
  ot->idname = "CLIP_OT_solve_camera";

  /* api callbacks */
  ot->exec = solve_camera_exec;
  ot->invoke = solve_camera_invoke;
  ot->modal = solve_camera_modal;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** clear solution operator *********************/

static int clear_solution_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    track->flag &= ~TRACK_HAS_BUNDLE;
  }

  MEM_SAFE_FREE(reconstruction->cameras);

  reconstruction->camnr = 0;
  reconstruction->flag &= ~TRACKING_RECONSTRUCTED;

  DEG_id_tag_update(&clip->id, 0);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

void CLIP_OT_clear_solution(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Solution";
  ot->description = "Clear all calculated data";
  ot->idname = "CLIP_OT_clear_solution";

  /* api callbacks */
  ot->exec = clear_solution_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
