/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"

#include "clip_intern.h"
#include "tracking_ops_intern.h"

/********************** Create plane track operator *********************/

static int create_plane_track_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingPlaneTrack *plane_track;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  plane_track = BKE_tracking_plane_track_add(
      tracking, &tracking_object->plane_tracks, &tracking_object->tracks, framenr);

  if (plane_track == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Need at least 4 selected point tracks to create a plane");
    return OPERATOR_CANCELLED;
  }

  BKE_tracking_tracks_deselect_all(&tracking_object->tracks);

  plane_track->flag |= SELECT;
  tracking_object->active_track = nullptr;
  tracking_object->active_plane_track = plane_track;

  /* Compute homoraphies and apply them on marker's corner, so we've got
   * quite nice motion from the very beginning.
   */
  BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);

  DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_create_plane_track(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create Plane Track";
  ot->description = "Create new plane track out of selected point tracks";
  ot->idname = "CLIP_OT_create_plane_track";

  /* api callbacks */
  ot->exec = create_plane_track_tracks_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Slide plane marker corner operator *********************/

typedef struct SlidePlaneMarkerData {
  int launch_event;
  MovieTrackingPlaneTrack *plane_track;
  MovieTrackingPlaneMarker *plane_marker;
  int width, height;
  int corner_index;
  float *corner;
  int previous_mval[2];
  float previous_corner[2];
  float old_corner[2];
  bool accurate;
} SlidePlaneMarkerData;

static MovieTrackingPlaneTrack *tracking_plane_marker_check_slide(bContext *C,
                                                                  const wmEvent *event,
                                                                  int *r_corner)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  float co[2];
  ED_clip_mouse_pos(space_clip, region, event->mval, co);

  TrackPickOptions options = ed_tracking_pick_options_defaults();
  options.selected_only = true;
  options.unlocked_only = true;
  options.enabled_only = true;
  const PlaneTrackPick track_pick = ed_tracking_pick_plane_track(&options, C, co);

  if (ed_tracking_plane_track_pick_empty(&track_pick) ||
      !ed_tracking_plane_track_pick_can_slide(&track_pick))
  {
    return nullptr;
  }

  if (r_corner != nullptr) {
    *r_corner = track_pick.corner_index;
  }

  return track_pick.plane_track;
}

static SlidePlaneMarkerData *slide_plane_marker_customdata(bContext *C, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);
  MovieTrackingPlaneTrack *plane_track;
  int width, height;
  float co[2];
  SlidePlaneMarkerData *customdata = nullptr;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  int corner;

  ED_space_clip_get_size(sc, &width, &height);
  if (width == 0 || height == 0) {
    return nullptr;
  }

  ED_clip_mouse_pos(sc, region, event->mval, co);

  plane_track = tracking_plane_marker_check_slide(C, event, &corner);
  if (plane_track) {
    MovieTrackingPlaneMarker *plane_marker;

    customdata = MEM_cnew<SlidePlaneMarkerData>("slide plane marker data");

    customdata->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

    plane_marker = BKE_tracking_plane_marker_ensure(plane_track, framenr);

    customdata->plane_track = plane_track;
    customdata->plane_marker = plane_marker;
    customdata->width = width;
    customdata->height = height;

    customdata->previous_mval[0] = event->mval[0];
    customdata->previous_mval[1] = event->mval[1];

    customdata->corner_index = corner;
    customdata->corner = plane_marker->corners[corner];

    copy_v2_v2(customdata->previous_corner, customdata->corner);
    copy_v2_v2(customdata->old_corner, customdata->corner);
  }

  return customdata;
}

static int slide_plane_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SlidePlaneMarkerData *slidedata = slide_plane_marker_customdata(C, event);

  if (slidedata) {
    SpaceClip *sc = CTX_wm_space_clip(C);
    MovieClip *clip = ED_space_clip_get_clip(sc);
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

    tracking_object->active_plane_track = slidedata->plane_track;
    tracking_object->active_track = nullptr;

    op->customdata = slidedata;

    clip_tracking_hide_cursor(C);
    WM_event_add_modal_handler(C, op);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

static void cancel_mouse_slide_plane_marker(SlidePlaneMarkerData *data)
{
  copy_v2_v2(data->corner, data->old_corner);
}

static void free_slide_plane_marker_data(SlidePlaneMarkerData *data)
{
  MEM_freeN(data);
}

static void slide_plane_marker_update_homographies(SpaceClip *sc, SlidePlaneMarkerData *data)
{
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  BKE_tracking_track_plane_from_existing_motion(data->plane_track, framenr);
}

static int slide_plane_marker_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  SlidePlaneMarkerData *data = (SlidePlaneMarkerData *)op->customdata;
  float dx, dy, mdelta[2];
  int next_corner_index, prev_corner_index, diag_corner_index;
  const float *next_corner, *prev_corner, *diag_corner;
  float next_edge[2], prev_edge[2], next_diag_edge[2], prev_diag_edge[2];

  switch (event->type) {
    case EVT_LEFTCTRLKEY:
    case EVT_RIGHTCTRLKEY:
    case EVT_LEFTSHIFTKEY:
    case EVT_RIGHTSHIFTKEY:
      if (ELEM(event->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY)) {
        data->accurate = event->val == KM_PRESS;
      }
      ATTR_FALLTHROUGH;
    case MOUSEMOVE:
      mdelta[0] = event->mval[0] - data->previous_mval[0];
      mdelta[1] = event->mval[1] - data->previous_mval[1];

      dx = mdelta[0] / data->width / sc->zoom;
      dy = mdelta[1] / data->height / sc->zoom;

      if (data->accurate) {
        dx /= 5.0f;
        dy /= 5.0f;
      }

      data->corner[0] = data->previous_corner[0] + dx;
      data->corner[1] = data->previous_corner[1] + dy;

      /*
       *                              prev_edge
       *   (Corner 3, current) <-----------------------   (Corner 2, previous)
       *           |                                              ^
       *           |                                              |
       *           |                                              |
       *           |                                              |
       * next_edge |                                              | next_diag_edge
       *           |                                              |
       *           |                                              |
       *           |                                              |
       *           v                                              |
       *    (Corner 0, next)   ----------------------->   (Corner 1, diagonal)
       *                             prev_diag_edge
       */

      next_corner_index = (data->corner_index + 1) % 4;
      prev_corner_index = (data->corner_index + 3) % 4;
      diag_corner_index = (data->corner_index + 2) % 4;

      next_corner = data->plane_marker->corners[next_corner_index];
      prev_corner = data->plane_marker->corners[prev_corner_index];
      diag_corner = data->plane_marker->corners[diag_corner_index];

      sub_v2_v2v2(next_edge, next_corner, data->corner);
      sub_v2_v2v2(prev_edge, data->corner, prev_corner);
      sub_v2_v2v2(next_diag_edge, prev_corner, diag_corner);
      sub_v2_v2v2(prev_diag_edge, diag_corner, next_corner);

      if (cross_v2v2(prev_edge, next_edge) < 0.0f) {
        closest_to_line_v2(data->corner, data->corner, prev_corner, next_corner);
      }

      if (cross_v2v2(next_diag_edge, prev_edge) < 0.0f) {
        closest_to_line_v2(data->corner, data->corner, prev_corner, diag_corner);
      }

      if (cross_v2v2(next_edge, prev_diag_edge) < 0.0f) {
        closest_to_line_v2(data->corner, data->corner, next_corner, diag_corner);
      }

      data->previous_mval[0] = event->mval[0];
      data->previous_mval[1] = event->mval[1];
      copy_v2_v2(data->previous_corner, data->corner);

      DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, nullptr);

      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
      if (event->type == data->launch_event && event->val == KM_RELEASE) {
        /* Marker is now keyframed. */
        data->plane_marker->flag &= ~PLANE_MARKER_TRACKED;

        slide_plane_marker_update_homographies(sc, data);

        free_slide_plane_marker_data(data);

        clip_tracking_show_cursor(C);

        DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
        WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

        return OPERATOR_FINISHED;
      }

      break;

    case EVT_ESCKEY:
      cancel_mouse_slide_plane_marker(data);

      free_slide_plane_marker_data(data);

      clip_tracking_show_cursor(C);

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

      return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_slide_plane_marker(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Slide Plane Marker";
  ot->description = "Slide plane marker areas";
  ot->idname = "CLIP_OT_slide_plane_marker";

  /* api callbacks */
  ot->poll = ED_space_clip_tracking_poll;
  ot->invoke = slide_plane_marker_invoke;
  ot->modal = slide_plane_marker_modal;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING;
}
