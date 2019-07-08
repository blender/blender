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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_tracking.h"
#include "BKE_report.h"

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
  ListBase *tracks_base = BKE_tracking_get_active_tracks(tracking);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  plane_track = BKE_tracking_plane_track_add(tracking, plane_tracks_base, tracks_base, framenr);

  if (plane_track == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Need at least 4 selected point tracks to create a plane");
    return OPERATOR_CANCELLED;
  }
  else {
    BKE_tracking_tracks_deselect_all(tracks_base);

    plane_track->flag |= SELECT;
    clip->tracking.act_track = NULL;
    clip->tracking.act_plane_track = plane_track;

    /* Compute homoraphies and apply them on marker's corner, so we've got
     * quite nice motion from the very beginning.
     */
    BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
  }

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
  int event_type;
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

static float mouse_to_plane_slide_zone_distance_squared(const float co[2],
                                                        const float slide_zone[2],
                                                        int width,
                                                        int height)
{
  float pixel_co[2] = {co[0] * width, co[1] * height},
        pixel_slide_zone[2] = {slide_zone[0] * width, slide_zone[1] * height};
  return SQUARE(pixel_co[0] - pixel_slide_zone[0]) + SQUARE(pixel_co[1] - pixel_slide_zone[1]);
}

static MovieTrackingPlaneTrack *tracking_plane_marker_check_slide(bContext *C,
                                                                  const wmEvent *event,
                                                                  int *corner_r)
{
  const float distance_clip_squared = 12.0f * 12.0f;
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  int width, height;
  float co[2];
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  ED_space_clip_get_size(sc, &width, &height);
  if (width == 0 || height == 0) {
    return NULL;
  }

  ED_clip_mouse_pos(sc, ar, event->mval, co);

  float min_distance_squared = FLT_MAX;
  int min_corner = -1;
  MovieTrackingPlaneTrack *min_plane_track = NULL;
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track != NULL;
       plane_track = plane_track->next) {
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);
      for (int i = 0; i < 4; i++) {
        float distance_squared = mouse_to_plane_slide_zone_distance_squared(
            co, plane_marker->corners[i], width, height);

        if (distance_squared < min_distance_squared) {
          min_distance_squared = distance_squared;
          min_corner = i;
          min_plane_track = plane_track;
        }
      }
    }
  }

  if (min_distance_squared < distance_clip_squared / sc->zoom) {
    if (corner_r != NULL) {
      *corner_r = min_corner;
    }
    return min_plane_track;
  }

  return NULL;
}

static void *slide_plane_marker_customdata(bContext *C, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);
  MovieTrackingPlaneTrack *plane_track;
  int width, height;
  float co[2];
  SlidePlaneMarkerData *customdata = NULL;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  int corner;

  ED_space_clip_get_size(sc, &width, &height);
  if (width == 0 || height == 0) {
    return NULL;
  }

  ED_clip_mouse_pos(sc, ar, event->mval, co);

  plane_track = tracking_plane_marker_check_slide(C, event, &corner);
  if (plane_track) {
    MovieTrackingPlaneMarker *plane_marker;

    customdata = MEM_callocN(sizeof(SlidePlaneMarkerData), "slide plane marker data");

    customdata->event_type = event->type;

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

    tracking->act_plane_track = slidedata->plane_track;
    tracking->act_track = NULL;

    op->customdata = slidedata;

    clip_tracking_hide_cursor(C);
    WM_event_add_modal_handler(C, op);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

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
    case LEFTCTRLKEY:
    case RIGHTCTRLKEY:
    case LEFTSHIFTKEY:
    case RIGHTSHIFTKEY:
      if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY)) {
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
      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
      if (event->type == data->event_type && event->val == KM_RELEASE) {
        /* Marker is now keyframed. */
        data->plane_marker->flag &= ~PLANE_MARKER_TRACKED;

        slide_plane_marker_update_homographies(sc, data);

        free_slide_plane_marker_data(op->customdata);

        clip_tracking_show_cursor(C);

        DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
        WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

        return OPERATOR_FINISHED;
      }

      break;

    case ESCKEY:
      cancel_mouse_slide_plane_marker(op->customdata);

      free_slide_plane_marker_data(op->customdata);

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
