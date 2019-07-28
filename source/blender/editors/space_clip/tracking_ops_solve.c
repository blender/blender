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

#include "DNA_camera_types.h"
#include "DNA_object_types.h" /* SELECT */
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_movieclip.h"
#include "BKE_report.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"

#include "clip_intern.h"

/********************** solve camera operator *********************/

typedef struct {
  Scene *scene;
  MovieClip *clip;
  MovieClipUser user;

  ReportList *reports;

  char stats_message[256];

  struct MovieReconstructContext *context;
} SolveCameraJob;

static bool solve_camera_initjob(
    bContext *C, SolveCameraJob *scj, wmOperator *op, char *error_msg, int max_error)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  Scene *scene = CTX_data_scene(C);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
  int width, height;

  if (!BKE_tracking_reconstruction_check(tracking, object, error_msg, max_error)) {
    return false;
  }

  /* Could fail if footage uses images with different sizes. */
  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  scj->clip = clip;
  scj->scene = scene;
  scj->reports = op->reports;
  scj->user = sc->user;

  scj->context = BKE_tracking_reconstruction_context_new(
      clip, object, object->keyframe1, object->keyframe2, width, height);

  tracking->stats = MEM_callocN(sizeof(MovieTrackingStats), "solve camera stats");

  return true;
}

static void solve_camera_updatejob(void *scv)
{
  SolveCameraJob *scj = (SolveCameraJob *)scv;
  MovieTracking *tracking = &scj->clip->tracking;

  BLI_strncpy(tracking->stats->message, scj->stats_message, sizeof(tracking->stats->message));
}

static void solve_camera_startjob(void *scv, short *stop, short *do_update, float *progress)
{
  SolveCameraJob *scj = (SolveCameraJob *)scv;
  BKE_tracking_reconstruction_solve(
      scj->context, stop, do_update, progress, scj->stats_message, sizeof(scj->stats_message));
}

static void solve_camera_freejob(void *scv)
{
  SolveCameraJob *scj = (SolveCameraJob *)scv;
  MovieTracking *tracking = &scj->clip->tracking;
  Scene *scene = scj->scene;
  MovieClip *clip = scj->clip;
  int solved;

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
    BKE_reportf(scj->reports,
                RPT_INFO,
                "Average re-projection error: %.3f",
                tracking->reconstruction.error);
  }

  /* Set currently solved clip as active for scene. */
  if (scene->clip != NULL) {
    id_us_min(&clip->id);
  }
  scene->clip = clip;
  id_us_plus(&clip->id);

  /* Set blender camera focal length so result would look fine there. */
  if (scene->camera != NULL && scene->camera->data &&
      GS(((ID *)scene->camera->data)->name) == ID_CA) {
    Camera *camera = (Camera *)scene->camera->data;
    int width, height;
    BKE_movieclip_get_size(clip, &scj->user, &width, &height);
    BKE_tracking_camera_to_blender(tracking, scene, camera, width, height);
    DEG_id_tag_update(&camera->id, ID_RECALC_COPY_ON_WRITE);
    WM_main_add_notifier(NC_OBJECT, camera);
  }

  MEM_freeN(tracking->stats);
  tracking->stats = NULL;

  DEG_id_tag_update(&clip->id, 0);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, NULL);

  /* Update active clip displayed in scene buttons. */
  WM_main_add_notifier(NC_SCENE, scene);

  BKE_tracking_reconstruction_context_free(scj->context);
  MEM_freeN(scj);
}

static int solve_camera_exec(bContext *C, wmOperator *op)
{
  SolveCameraJob *scj;
  char error_msg[256] = "\0";
  scj = MEM_callocN(sizeof(SolveCameraJob), "SolveCameraJob data");
  if (!solve_camera_initjob(C, scj, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    solve_camera_freejob(scj);
    return OPERATOR_CANCELLED;
  }
  solve_camera_startjob(scj, NULL, NULL, NULL);
  solve_camera_freejob(scj);
  return OPERATOR_FINISHED;
}

static int solve_camera_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SolveCameraJob *scj;
  ScrArea *sa = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);
  wmJob *wm_job;
  char error_msg[256] = "\0";

  if (WM_jobs_test(CTX_wm_manager(C), sa, WM_JOB_TYPE_ANY)) {
    /* only one solve is allowed at a time */
    return OPERATOR_CANCELLED;
  }

  scj = MEM_callocN(sizeof(SolveCameraJob), "SolveCameraJob data");
  if (!solve_camera_initjob(C, scj, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    solve_camera_freejob(scj);
    return OPERATOR_CANCELLED;
  }

  BLI_strncpy(tracking->stats->message,
              "Solving camera | Preparing solve",
              sizeof(tracking->stats->message));

  /* Hide reconstruction statistics from previous solve. */
  reconstruction->flag &= ~TRACKING_RECONSTRUCTED;
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  /* Setup job. */
  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       sa,
                       "Solve Camera",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_CLIP_SOLVE_CAMERA);
  WM_jobs_customdata_set(wm_job, scj, solve_camera_freejob);
  WM_jobs_timer(wm_job, 0.1, NC_MOVIECLIP | NA_EVALUATED, 0);
  WM_jobs_callbacks(wm_job, solve_camera_startjob, NULL, solve_camera_updatejob, NULL);

  G.is_break = false;

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_cursor_wait(0);

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int solve_camera_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  /* No running solver, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_ANY)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* Running solver. */
  switch (event->type) {
    case ESCKEY:
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

static int clear_solution_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);

  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    track->flag &= ~TRACK_HAS_BUNDLE;
  }

  if (reconstruction->cameras != NULL) {
    MEM_freeN(reconstruction->cameras);
    reconstruction->cameras = NULL;
  }

  reconstruction->camnr = 0;
  reconstruction->flag &= ~TRACKING_RECONSTRUCTED;

  DEG_id_tag_update(&clip->id, 0);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);

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
