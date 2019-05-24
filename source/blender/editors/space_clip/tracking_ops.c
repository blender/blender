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
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_report.h"
#include "BKE_sound.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLT_translation.h"

#include "clip_intern.h"
#include "tracking_ops_intern.h"

/********************** add marker operator *********************/

static bool add_marker(const bContext *C, float x, float y)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  MovieTrackingTrack *track;
  int width, height;
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  ED_space_clip_get_size(sc, &width, &height);

  if (width == 0 || height == 0) {
    return false;
  }

  track = BKE_tracking_track_add(tracking, tracksbase, x, y, framenr, width, height);

  BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, 0);
  BKE_tracking_plane_tracks_deselect_all(plane_tracks_base);

  clip->tracking.act_track = track;
  clip->tracking.act_plane_track = NULL;

  return true;
}

static int add_marker_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  float pos[2];

  RNA_float_get_array(op->ptr, "location", pos);

  if (!add_marker(C, pos[0], pos[1])) {
    return OPERATOR_CANCELLED;
  }

  /* Reset offset from locked position, so frame jumping wouldn't be so
   * confusing.
   */
  sc->xlockof = 0;
  sc->ylockof = 0;

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

static int add_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  if (!RNA_struct_property_is_set(op->ptr, "location")) {
    /* If location is not set, use mouse positio nas default. */
    float co[2];
    ED_clip_mouse_pos(sc, ar, event->mval, co);
    RNA_float_set_array(op->ptr, "location", co);
  }

  return add_marker_exec(C, op);
}

void CLIP_OT_add_marker(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Marker";
  ot->idname = "CLIP_OT_add_marker";
  ot->description = "Place new marker at specified location";

  /* api callbacks */
  ot->invoke = add_marker_invoke;
  ot->exec = add_marker_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Location of marker on frame",
                       -1.0f,
                       1.0f);
}

/********************** add marker operator *********************/

static int add_marker_at_click_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  ED_workspace_status_text(C, IFACE_("Use LMB click to define location where place the marker"));

  /* Add modal handler for ESC. */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int add_marker_at_click_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  switch (event->type) {
    case MOUSEMOVE:
      return OPERATOR_RUNNING_MODAL;

    case LEFTMOUSE: {
      SpaceClip *sc = CTX_wm_space_clip(C);
      MovieClip *clip = ED_space_clip_get_clip(sc);
      ARegion *ar = CTX_wm_region(C);
      float pos[2];

      ED_workspace_status_text(C, NULL);

      ED_clip_point_stable_pos(
          sc, ar, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, &pos[0], &pos[1]);

      if (!add_marker(C, pos[0], pos[1])) {
        return OPERATOR_CANCELLED;
      }

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
      return OPERATOR_FINISHED;
    }

    case ESCKEY:
      ED_workspace_status_text(C, NULL);
      return OPERATOR_CANCELLED;
  }

  return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_add_marker_at_click(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Marker at Click";
  ot->idname = "CLIP_OT_add_marker_at_click";
  ot->description = "Place new marker at the desired (clicked) position";

  /* api callbacks */
  ot->invoke = add_marker_at_click_invoke;
  ot->poll = ED_space_clip_tracking_poll;
  ot->modal = add_marker_at_click_modal;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
}

/********************** delete track operator *********************/

static int delete_track_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  bool changed = false;
  /* Delete selected plane tracks. */
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first, *next_plane_track;
       plane_track != NULL;
       plane_track = next_plane_track) {
    next_plane_track = plane_track->next;
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      clip_delete_plane_track(C, clip, plane_track);
      changed = true;
    }
  }
  /* Remove selected point tracks (they'll also be removed from planes which
   * uses them).
   */
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  for (MovieTrackingTrack *track = tracksbase->first, *next_track; track != NULL;
       track = next_track) {
    next_track = track->next;
    if (TRACK_VIEW_SELECTED(sc, track)) {
      clip_delete_track(C, clip, track);
      changed = true;
    }
  }
  /* Nothing selected now, unlock view so it can be scrolled nice again. */
  sc->flag &= ~SC_LOCK_SELECTION;
  if (changed) {
    WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
  }
  return OPERATOR_FINISHED;
}

void CLIP_OT_delete_track(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Track";
  ot->idname = "CLIP_OT_delete_track";
  ot->description = "Delete selected tracks";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->exec = delete_track_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** delete marker operator *********************/

static int delete_marker_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  bool has_selection = false;
  bool changed = false;

  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  for (MovieTrackingTrack *track = tracksbase->first, *next_track; track != NULL;
       track = next_track) {
    next_track = track->next;
    if (TRACK_VIEW_SELECTED(sc, track)) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);
      if (marker != NULL) {
        has_selection |= track->markersnr > 1;
        clip_delete_marker(C, clip, track, marker);
        changed = true;
      }
    }
  }

  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first, *plane_track_next;
       plane_track != NULL;
       plane_track = plane_track_next) {
    plane_track_next = plane_track->next;
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get_exact(plane_track,
                                                                                   framenr);
      if (plane_marker != NULL) {
        if (plane_track->markersnr == 1) {
          BKE_tracking_plane_track_free(plane_track);
          BLI_freelinkN(plane_tracks_base, plane_track);
        }
        else {
          BKE_tracking_plane_marker_delete(plane_track, framenr);
        }
        changed = true;
      }
    }
  }

  if (!has_selection) {
    /* Nothing selected now, unlock view so it can be scrolled nice again. */
    sc->flag &= ~SC_LOCK_SELECTION;
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void CLIP_OT_delete_marker(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Marker";
  ot->idname = "CLIP_OT_delete_marker";
  ot->description = "Delete marker for current frame from selected tracks";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->exec = delete_marker_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** slide marker operator *********************/

enum {
  SLIDE_ACTION_POS = 0,
  SLIDE_ACTION_SIZE,
  SLIDE_ACTION_OFFSET,
  SLIDE_ACTION_TILT_SIZE,
};

typedef struct {
  short area, action;
  MovieTrackingTrack *track;
  MovieTrackingMarker *marker;

  int mval[2];
  int width, height;
  float *min, *max, *pos, *offset, (*corners)[2];
  float spos[2];

  bool lock, accurate;

  /* Data to restore on cancel. */
  float old_search_min[2], old_search_max[2], old_pos[2], old_offset[2];
  float old_corners[4][2];
  float (*old_markers)[2];
} SlideMarkerData;

static void slide_marker_tilt_slider(const MovieTrackingMarker *marker, float r_slider[2])
{
  add_v2_v2v2(r_slider, marker->pattern_corners[1], marker->pattern_corners[2]);
  add_v2_v2(r_slider, marker->pos);
}

static SlideMarkerData *create_slide_marker_data(SpaceClip *sc,
                                                 MovieTrackingTrack *track,
                                                 MovieTrackingMarker *marker,
                                                 const wmEvent *event,
                                                 int area,
                                                 int corner,
                                                 int action,
                                                 int width,
                                                 int height)
{
  SlideMarkerData *data = MEM_callocN(sizeof(SlideMarkerData), "slide marker data");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  marker = BKE_tracking_marker_ensure(track, framenr);

  data->area = area;
  data->action = action;
  data->track = track;
  data->marker = marker;

  if (area == TRACK_AREA_POINT) {
    data->pos = marker->pos;
    data->offset = track->offset;
  }
  else if (area == TRACK_AREA_PAT) {
    if (action == SLIDE_ACTION_SIZE) {
      data->corners = marker->pattern_corners;
    }
    else if (action == SLIDE_ACTION_OFFSET) {
      data->pos = marker->pos;
      data->offset = track->offset;
      data->old_markers = MEM_callocN(sizeof(*data->old_markers) * track->markersnr,
                                      "slide marekrs");
      for (int a = 0; a < track->markersnr; a++) {
        copy_v2_v2(data->old_markers[a], track->markers[a].pos);
      }
    }
    else if (action == SLIDE_ACTION_POS) {
      data->corners = marker->pattern_corners;
      data->pos = marker->pattern_corners[corner];
      copy_v2_v2(data->spos, data->pos);
    }
    else if (action == SLIDE_ACTION_TILT_SIZE) {
      data->corners = marker->pattern_corners;
      slide_marker_tilt_slider(marker, data->spos);
    }
  }
  else if (area == TRACK_AREA_SEARCH) {
    data->min = marker->search_min;
    data->max = marker->search_max;
  }

  data->mval[0] = event->mval[0];
  data->mval[1] = event->mval[1];

  data->width = width;
  data->height = height;

  if (action == SLIDE_ACTION_SIZE) {
    data->lock = true;
  }

  /* Backup marker's settings. */
  memcpy(data->old_corners, marker->pattern_corners, sizeof(data->old_corners));
  copy_v2_v2(data->old_search_min, marker->search_min);
  copy_v2_v2(data->old_search_max, marker->search_max);
  copy_v2_v2(data->old_pos, marker->pos);
  copy_v2_v2(data->old_offset, track->offset);

  return data;
}

static float mouse_to_slide_zone_distance_squared(const float co[2],
                                                  const float slide_zone[2],
                                                  int width,
                                                  int height)
{
  float pixel_co[2] = {co[0] * width, co[1] * height},
        pixel_slide_zone[2] = {slide_zone[0] * width, slide_zone[1] * height};
  return SQUARE(pixel_co[0] - pixel_slide_zone[0]) + SQUARE(pixel_co[1] - pixel_slide_zone[1]);
}

static float mouse_to_search_corner_distance_squared(
    const MovieTrackingMarker *marker, const float co[2], int corner, int width, int height)
{
  float side_zone[2];
  if (corner == 0) {
    side_zone[0] = marker->pos[0] + marker->search_max[0];
    side_zone[1] = marker->pos[1] + marker->search_min[1];
  }
  else {
    side_zone[0] = marker->pos[0] + marker->search_min[0];
    side_zone[1] = marker->pos[1] + marker->search_max[1];
  }
  return mouse_to_slide_zone_distance_squared(co, side_zone, width, height);
}

static float mouse_to_closest_pattern_corner_distance_squared(
    const MovieTrackingMarker *marker, const float co[2], int width, int height, int *r_corner)
{
  float min_distance_squared = FLT_MAX;
  for (int i = 0; i < 4; i++) {
    float corner_co[2];
    add_v2_v2v2(corner_co, marker->pattern_corners[i], marker->pos);
    float distance_squared = mouse_to_slide_zone_distance_squared(co, corner_co, width, height);
    if (distance_squared < min_distance_squared) {
      min_distance_squared = distance_squared;
      *r_corner = i;
    }
  }
  return min_distance_squared;
}

static float mouse_to_offset_distance_squared(const MovieTrackingTrack *track,
                                              const MovieTrackingMarker *marker,
                                              const float co[2],
                                              int width,
                                              int height)
{
  float pos[2];
  add_v2_v2v2(pos, marker->pos, track->offset);
  return mouse_to_slide_zone_distance_squared(co, pos, width, height);
}

static int mouse_to_tilt_distance_squared(const MovieTrackingMarker *marker,
                                          const float co[2],
                                          int width,
                                          int height)
{
  float slider[2];
  slide_marker_tilt_slider(marker, slider);
  return mouse_to_slide_zone_distance_squared(co, slider, width, height);
}

static bool slide_check_corners(float (*corners)[2])
{
  int i, next, prev;
  float cross = 0.0f;
  float p[2] = {0.0f, 0.0f};

  if (!isect_point_quad_v2(p, corners[0], corners[1], corners[2], corners[3])) {
    return false;
  }

  for (i = 0; i < 4; i++) {
    float v1[2], v2[2], cur_cross;

    next = (i + 1) % 4;
    prev = (4 + i - 1) % 4;

    sub_v2_v2v2(v1, corners[i], corners[prev]);
    sub_v2_v2v2(v2, corners[next], corners[i]);

    cur_cross = cross_v2v2(v1, v2);

    if (fabsf(cur_cross) > FLT_EPSILON) {
      if (cross == 0.0f) {
        cross = cur_cross;
      }
      else if (cross * cur_cross < 0.0f) {
        return false;
      }
    }
  }

  return true;
}

MovieTrackingTrack *tracking_marker_check_slide(
    bContext *C, const wmEvent *event, int *area_r, int *action_r, int *corner_r)
{
  const float distance_clip_squared = 12.0f * 12.0f;
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTrackingTrack *track;
  int width, height;
  float co[2];
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  float global_min_distance_squared = FLT_MAX;

  /* Sliding zone designator which is the closest to the mouse
   * across all the tracks.
   */
  int min_action = -1, min_area = 0, min_corner = -1;
  MovieTrackingTrack *min_track = NULL;

  ED_space_clip_get_size(sc, &width, &height);

  if (width == 0 || height == 0) {
    return NULL;
  }

  ED_clip_mouse_pos(sc, ar, event->mval, co);

  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
      /* Sliding zone designator which is the closest to the mouse for
       * the current tracks.
       */
      float min_distance_squared = FLT_MAX;
      int action = -1, area = 0, corner = -1;

      if ((marker->flag & MARKER_DISABLED) == 0) {
        float distance_squared;

        /* We start checking with whether the mouse is close enough
         * to the pattern offset area.
         */
        distance_squared = mouse_to_offset_distance_squared(track, marker, co, width, height);
        area = TRACK_AREA_POINT;
        action = SLIDE_ACTION_POS;

        /* NOTE: All checks here are assuming there's no maximum distance
         * limit, so checks are quite simple here.
         * Actual distance clipping happens later once all the sliding
         * zones are checked.
         */
        min_distance_squared = distance_squared;

        /* If search area is visible, check how close to it's sliding
         * zones mouse is.
         */
        if (sc->flag & SC_SHOW_MARKER_SEARCH) {
          distance_squared = mouse_to_search_corner_distance_squared(marker, co, 1, width, height);
          if (distance_squared < min_distance_squared) {
            area = TRACK_AREA_SEARCH;
            action = SLIDE_ACTION_OFFSET;
            min_distance_squared = distance_squared;
          }

          distance_squared = mouse_to_search_corner_distance_squared(marker, co, 0, width, height);
          if (distance_squared < min_distance_squared) {
            area = TRACK_AREA_SEARCH;
            action = SLIDE_ACTION_SIZE;
            min_distance_squared = distance_squared;
          }
        }

        /* If pattern area is visible, check which corner is closest to
         * the mouse.
         */
        if (sc->flag & SC_SHOW_MARKER_PATTERN) {
          int current_corner = -1;
          distance_squared = mouse_to_closest_pattern_corner_distance_squared(
              marker, co, width, height, &current_corner);
          if (distance_squared < min_distance_squared) {
            area = TRACK_AREA_PAT;
            action = SLIDE_ACTION_POS;
            corner = current_corner;
            min_distance_squared = distance_squared;
          }

          /* Here we also check whether the mouse is actually closer to
           * the widget which controls scale and tilt.
           */
          distance_squared = mouse_to_tilt_distance_squared(marker, co, width, height);
          if (distance_squared < min_distance_squared) {
            area = TRACK_AREA_PAT;
            action = SLIDE_ACTION_TILT_SIZE;
            min_distance_squared = distance_squared;
          }
        }

        if (min_distance_squared < global_min_distance_squared) {
          min_area = area;
          min_action = action;
          min_corner = corner;
          min_track = track;
          global_min_distance_squared = min_distance_squared;
        }
      }
    }

    track = track->next;
  }

  if (global_min_distance_squared < distance_clip_squared / sc->zoom) {
    if (area_r) {
      *area_r = min_area;
    }
    if (action_r) {
      *action_r = min_action;
    }
    if (corner_r) {
      *corner_r = min_corner;
    }
    return min_track;
  }
  return NULL;
}

static void *slide_marker_customdata(bContext *C, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  MovieTrackingTrack *track;
  int width, height;
  float co[2];
  void *customdata = NULL;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  int area, action, corner;

  ED_space_clip_get_size(sc, &width, &height);

  if (width == 0 || height == 0) {
    return NULL;
  }

  ED_clip_mouse_pos(sc, ar, event->mval, co);

  track = tracking_marker_check_slide(C, event, &area, &action, &corner);
  if (track != NULL) {
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
    customdata = create_slide_marker_data(
        sc, track, marker, event, area, corner, action, width, height);
  }

  return customdata;
}

static int slide_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SlideMarkerData *slidedata = slide_marker_customdata(C, event);
  if (slidedata != NULL) {
    SpaceClip *sc = CTX_wm_space_clip(C);
    MovieClip *clip = ED_space_clip_get_clip(sc);
    MovieTracking *tracking = &clip->tracking;

    tracking->act_track = slidedata->track;
    tracking->act_plane_track = NULL;

    op->customdata = slidedata;

    clip_tracking_hide_cursor(C);
    WM_event_add_modal_handler(C, op);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

static void cancel_mouse_slide(SlideMarkerData *data)
{
  MovieTrackingTrack *track = data->track;
  MovieTrackingMarker *marker = data->marker;

  memcpy(marker->pattern_corners, data->old_corners, sizeof(marker->pattern_corners));
  copy_v2_v2(marker->search_min, data->old_search_min);
  copy_v2_v2(marker->search_max, data->old_search_max);
  copy_v2_v2(marker->pos, data->old_pos);
  copy_v2_v2(track->offset, data->old_offset);

  if (data->old_markers != NULL) {
    for (int a = 0; a < data->track->markersnr; a++) {
      copy_v2_v2(data->track->markers[a].pos, data->old_markers[a]);
    }
  }
}

static void apply_mouse_slide(bContext *C, SlideMarkerData *data)
{
  if (data->area == TRACK_AREA_POINT) {
    SpaceClip *sc = CTX_wm_space_clip(C);
    MovieClip *clip = ED_space_clip_get_clip(sc);
    MovieTracking *tracking = &clip->tracking;
    ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
    int framenr = ED_space_clip_get_clip_frame_number(sc);

    for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track != NULL;
         plane_track = plane_track->next) {
      if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
        if (BKE_tracking_plane_track_has_point_track(plane_track, data->track)) {
          BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
        }
      }
    }
  }
}

static void free_slide_data(SlideMarkerData *data)
{
  if (data->old_markers != NULL) {
    MEM_freeN(data->old_markers);
  }
  MEM_freeN(data);
}

static int slide_marker_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  SlideMarkerData *data = (SlideMarkerData *)op->customdata;
  float dx, dy, mdelta[2];

  switch (event->type) {
    case LEFTCTRLKEY:
    case RIGHTCTRLKEY:
    case LEFTSHIFTKEY:
    case RIGHTSHIFTKEY:
      if (data->action == SLIDE_ACTION_SIZE) {
        if (ELEM(event->type, LEFTCTRLKEY, RIGHTCTRLKEY)) {
          data->lock = event->val == KM_RELEASE;
        }
      }

      if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY)) {
        data->accurate = event->val == KM_PRESS;
      }
      ATTR_FALLTHROUGH;
    case MOUSEMOVE:
      mdelta[0] = event->mval[0] - data->mval[0];
      mdelta[1] = event->mval[1] - data->mval[1];

      dx = mdelta[0] / data->width / sc->zoom;

      if (data->lock) {
        dy = -dx / data->height * data->width;
      }
      else {
        dy = mdelta[1] / data->height / sc->zoom;
      }

      if (data->accurate) {
        dx /= 5.0f;
        dy /= 5.0f;
      }

      if (data->area == TRACK_AREA_POINT) {
        if (data->action == SLIDE_ACTION_OFFSET) {
          data->offset[0] = data->old_offset[0] + dx;
          data->offset[1] = data->old_offset[1] + dy;
        }
        else {
          data->pos[0] = data->old_pos[0] + dx;
          data->pos[1] = data->old_pos[1] + dy;
        }

        WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
        DEG_id_tag_update(&sc->clip->id, 0);
      }
      else if (data->area == TRACK_AREA_PAT) {
        if (data->action == SLIDE_ACTION_SIZE) {
          float start[2], end[2];
          float scale;

          ED_clip_point_stable_pos(sc, ar, data->mval[0], data->mval[1], &start[0], &start[1]);

          sub_v2_v2(start, data->old_pos);

          if (len_squared_v2(start) != 0.0f) {
            float mval[2];

            if (data->accurate) {
              mval[0] = data->mval[0] + (event->mval[0] - data->mval[0]) / 5.0f;
              mval[1] = data->mval[1] + (event->mval[1] - data->mval[1]) / 5.0f;
            }
            else {
              mval[0] = event->mval[0];
              mval[1] = event->mval[1];
            }

            ED_clip_point_stable_pos(sc, ar, mval[0], mval[1], &end[0], &end[1]);

            sub_v2_v2(end, data->old_pos);
            scale = len_v2(end) / len_v2(start);

            if (scale > 0.0f) {
              for (int a = 0; a < 4; a++) {
                mul_v2_v2fl(data->corners[a], data->old_corners[a], scale);
              }
            }
          }

          BKE_tracking_marker_clamp(data->marker, CLAMP_PAT_DIM);
        }
        else if (data->action == SLIDE_ACTION_OFFSET) {
          float d[2] = {dx, dy};
          for (int a = 0; a < data->track->markersnr; a++) {
            add_v2_v2v2(data->track->markers[a].pos, data->old_markers[a], d);
          }
          sub_v2_v2v2(data->offset, data->old_offset, d);
        }
        else if (data->action == SLIDE_ACTION_POS) {
          float spos[2];

          copy_v2_v2(spos, data->pos);

          data->pos[0] = data->spos[0] + dx;
          data->pos[1] = data->spos[1] + dy;

          if (!slide_check_corners(data->corners)) {
            copy_v2_v2(data->pos, spos);
          }

          /* Currently only patterns are allowed to have such
           * combination of event and data.
           */
          BKE_tracking_marker_clamp(data->marker, CLAMP_PAT_DIM);
        }
        else if (data->action == SLIDE_ACTION_TILT_SIZE) {
          float start[2], end[2];
          float scale = 1.0f, angle = 0.0f;
          float mval[2];

          if (data->accurate) {
            mval[0] = data->mval[0] + (event->mval[0] - data->mval[0]) / 5.0f;
            mval[1] = data->mval[1] + (event->mval[1] - data->mval[1]) / 5.0f;
          }
          else {
            mval[0] = event->mval[0];
            mval[1] = event->mval[1];
          }

          sub_v2_v2v2(start, data->spos, data->old_pos);

          ED_clip_point_stable_pos(sc, ar, mval[0], mval[1], &end[0], &end[1]);
          sub_v2_v2(end, data->old_pos);

          if (len_squared_v2(start) != 0.0f) {
            scale = len_v2(end) / len_v2(start);

            if (scale < 0.0f) {
              scale = 0.0;
            }
          }

          angle = -angle_signed_v2v2(start, end);

          for (int a = 0; a < 4; a++) {
            float vec[2];

            mul_v2_v2fl(data->corners[a], data->old_corners[a], scale);

            copy_v2_v2(vec, data->corners[a]);
            vec[0] *= data->width;
            vec[1] *= data->height;

            data->corners[a][0] = (vec[0] * cosf(angle) - vec[1] * sinf(angle)) / data->width;
            data->corners[a][1] = (vec[1] * cosf(angle) + vec[0] * sinf(angle)) / data->height;
          }

          BKE_tracking_marker_clamp(data->marker, CLAMP_PAT_DIM);
        }
      }
      else if (data->area == TRACK_AREA_SEARCH) {
        if (data->action == SLIDE_ACTION_SIZE) {
          data->min[0] = data->old_search_min[0] - dx;
          data->max[0] = data->old_search_max[0] + dx;

          data->min[1] = data->old_search_min[1] + dy;
          data->max[1] = data->old_search_max[1] - dy;

          BKE_tracking_marker_clamp(data->marker, CLAMP_SEARCH_DIM);
        }
        else if (data->area == TRACK_AREA_SEARCH) {
          float d[2] = {dx, dy};
          add_v2_v2v2(data->min, data->old_search_min, d);
          add_v2_v2v2(data->max, data->old_search_max, d);
        }

        BKE_tracking_marker_clamp(data->marker, CLAMP_SEARCH_POS);
      }

      data->marker->flag &= ~MARKER_TRACKED;

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

      break;

    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        apply_mouse_slide(C, op->customdata);
        free_slide_data(op->customdata);

        clip_tracking_show_cursor(C);

        return OPERATOR_FINISHED;
      }

      break;

    case ESCKEY:
      cancel_mouse_slide(op->customdata);

      free_slide_data(op->customdata);

      clip_tracking_show_cursor(C);

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

      return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_slide_marker(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Slide Marker";
  ot->description = "Slide marker areas";
  ot->idname = "CLIP_OT_slide_marker";

  /* api callbacks */
  ot->poll = ED_space_clip_tracking_poll;
  ot->invoke = slide_marker_invoke;
  ot->modal = slide_marker_modal;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR | OPTYPE_BLOCKING;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "offset",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Offset",
                       "Offset in floating point units, 1.0 is the width and height of the image",
                       -FLT_MAX,
                       FLT_MAX);
}

/********************** clear track operator *********************/

static int clear_track_path_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  int action = RNA_enum_get(op->ptr, "action");
  const bool clear_active = RNA_boolean_get(op->ptr, "clear_active");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  if (clear_active) {
    MovieTrackingTrack *track = BKE_tracking_track_get_active(tracking);
    if (track != NULL) {
      BKE_tracking_track_path_clear(track, framenr, action);
    }
  }
  else {
    for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
      if (TRACK_VIEW_SELECTED(sc, track)) {
        BKE_tracking_track_path_clear(track, framenr, action);
      }
    }
  }

  BKE_tracking_dopesheet_tag_update(tracking);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_clear_track_path(wmOperatorType *ot)
{
  static const EnumPropertyItem clear_path_actions[] = {
      {TRACK_CLEAR_UPTO, "UPTO", 0, "Clear up-to", "Clear path up to current frame"},
      {TRACK_CLEAR_REMAINED,
       "REMAINED",
       0,
       "Clear remained",
       "Clear path at remaining frames (after current)"},
      {TRACK_CLEAR_ALL, "ALL", 0, "Clear all", "Clear the whole path"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Clear Track Path";
  ot->description = "Clear tracks after/before current position or clear the whole track";
  ot->idname = "CLIP_OT_clear_track_path";

  /* api callbacks */
  ot->exec = clear_track_path_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "action",
               clear_path_actions,
               TRACK_CLEAR_REMAINED,
               "Action",
               "Clear action to execute");
  RNA_def_boolean(ot->srna,
                  "clear_active",
                  0,
                  "Clear Active",
                  "Clear active track only instead of all selected tracks");
}

/********************** disable markers operator *********************/

enum {
  MARKER_OP_DISABLE = 0,
  MARKER_OP_ENABLE = 1,
  MARKER_OP_TOGGLE = 2,
};

static int disable_markers_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  int action = RNA_enum_get(op->ptr, "action");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);
      switch (action) {
        case MARKER_OP_DISABLE:
          marker->flag |= MARKER_DISABLED;
          break;
        case MARKER_OP_ENABLE:
          marker->flag &= ~MARKER_DISABLED;
          break;
        case MARKER_OP_TOGGLE:
          marker->flag ^= MARKER_DISABLED;
          break;
      }
    }
  }

  DEG_id_tag_update(&clip->id, 0);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_disable_markers(wmOperatorType *ot)
{
  static const EnumPropertyItem actions_items[] = {
      {MARKER_OP_DISABLE, "DISABLE", 0, "Disable", "Disable selected markers"},
      {MARKER_OP_ENABLE, "ENABLE", 0, "Enable", "Enable selected markers"},
      {MARKER_OP_TOGGLE, "TOGGLE", 0, "Toggle", "Toggle disabled flag for selected markers"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Disable Markers";
  ot->description = "Disable/enable selected markers";
  ot->idname = "CLIP_OT_disable_markers";

  /* api callbacks */
  ot->exec = disable_markers_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Disable action to execute");
}

/********************** set principal center operator *********************/

static int set_center_principal_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int width, height;

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  if (width == 0 || height == 0) {
    return OPERATOR_CANCELLED;
  }

  clip->tracking.camera.principal[0] = ((float)width) / 2.0f;
  clip->tracking.camera.principal[1] = ((float)height) / 2.0f;

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_center_principal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Principal to Center";
  ot->description = "Set optical center to center of footage";
  ot->idname = "CLIP_OT_set_center_principal";

  /* api callbacks */
  ot->exec = set_center_principal_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** hide tracks operator *********************/

static int hide_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  int unselected;

  unselected = RNA_boolean_get(op->ptr, "unselected");

  /* Hide point tracks. */
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (unselected == 0 && TRACK_VIEW_SELECTED(sc, track)) {
      track->flag |= TRACK_HIDDEN;
    }
    else if (unselected == 1 && !TRACK_VIEW_SELECTED(sc, track)) {
      track->flag |= TRACK_HIDDEN;
    }
  }

  if (act_track != NULL && act_track->flag & TRACK_HIDDEN) {
    clip->tracking.act_track = NULL;
  }

  /* Hide place tracks. */
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  MovieTrackingPlaneTrack *act_plane_track = BKE_tracking_plane_track_get_active(tracking);
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track != NULL;
       plane_track = plane_track->next) {
    if (unselected == 0 && plane_track->flag & SELECT) {
      plane_track->flag |= PLANE_TRACK_HIDDEN;
    }
    else if (unselected == 1 && (plane_track->flag & SELECT) == 0) {
      plane_track->flag |= PLANE_TRACK_HIDDEN;
    }
  }
  if (act_plane_track != NULL && act_plane_track->flag & TRACK_HIDDEN) {
    clip->tracking.act_plane_track = NULL;
  }

  if (unselected == 0) {
    /* No selection on screen now, unlock view so it can be
     * scrolled nice again.
     */
    sc->flag &= ~SC_LOCK_SELECTION;
  }

  BKE_tracking_dopesheet_tag_update(tracking);
  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_hide_tracks(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Tracks";
  ot->description = "Hide selected tracks";
  ot->idname = "CLIP_OT_hide_tracks";

  /* api callbacks */
  ot->exec = hide_tracks_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected tracks");
}

/********************** hide tracks clear operator *********************/

static int hide_tracks_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;

  /* Unhide point tracks. */
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    track->flag &= ~TRACK_HIDDEN;
  }

  /* Unhide plane tracks. */
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track != NULL;
       plane_track = plane_track->next) {
    plane_track->flag &= ~PLANE_TRACK_HIDDEN;
  }

  BKE_tracking_dopesheet_tag_update(tracking);

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_hide_tracks_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Tracks Clear";
  ot->description = "Clear hide selected tracks";
  ot->idname = "CLIP_OT_hide_tracks_clear";

  /* api callbacks */
  ot->exec = hide_tracks_clear_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** frame jump operator *********************/

static bool frame_jump_poll(bContext *C)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  return space_clip != NULL;
}

static int frame_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  int pos = RNA_enum_get(op->ptr, "position");
  int delta;

  if (pos <= 1) { /* jump to path */
    MovieTrackingTrack *track = BKE_tracking_track_get_active(tracking);
    if (track == NULL) {
      return OPERATOR_CANCELLED;
    }

    delta = pos == 1 ? 1 : -1;
    while (sc->user.framenr + delta >= SFRA && sc->user.framenr + delta <= EFRA) {
      int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr + delta);
      MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);

      if (marker == NULL || marker->flag & MARKER_DISABLED) {
        break;
      }

      sc->user.framenr += delta;
    }
  }
  else { /* to failed frame */
    if (tracking->reconstruction.flag & TRACKING_RECONSTRUCTED) {
      int framenr = ED_space_clip_get_clip_frame_number(sc);
      MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

      delta = pos == 3 ? 1 : -1;
      framenr += delta;

      while (framenr + delta >= SFRA && framenr + delta <= EFRA) {
        MovieReconstructedCamera *cam = BKE_tracking_camera_get_reconstructed(
            tracking, object, framenr);

        if (cam == NULL) {
          sc->user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, framenr);
          break;
        }

        framenr += delta;
      }
    }
  }

  if (CFRA != sc->user.framenr) {
    CFRA = sc->user.framenr;
    BKE_sound_seek_scene(CTX_data_main(C), scene);

    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_frame_jump(wmOperatorType *ot)
{
  static const EnumPropertyItem position_items[] = {
      {0, "PATHSTART", 0, "Path Start", "Jump to start of current path"},
      {1, "PATHEND", 0, "Path End", "Jump to end of current path"},
      {2, "FAILEDPREV", 0, "Previous Failed", "Jump to previous failed frame"},
      {2, "FAILNEXT", 0, "Next Failed", "Jump to next failed frame"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Jump to Frame";
  ot->description = "Jump to special frame";
  ot->idname = "CLIP_OT_frame_jump";

  /* api callbacks */
  ot->exec = frame_jump_exec;
  ot->poll = frame_jump_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "position", position_items, 0, "Position", "Position to jump to");
}

/********************** join tracks operator *********************/

static int join_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingStabilization *stab = &tracking->stabilization;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  bool update_stabilization = false;

  MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
  if (act_track == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active track to join to");
    return OPERATOR_CANCELLED;
  }

  GSet *point_tracks = BLI_gset_ptr_new(__func__);

  for (MovieTrackingTrack *track = tracksbase->first, *next_track; track != NULL;
       track = next_track) {
    next_track = track->next;
    if (TRACK_VIEW_SELECTED(sc, track) && track != act_track) {
      BKE_tracking_tracks_join(tracking, act_track, track);

      if (track->flag & TRACK_USE_2D_STAB) {
        update_stabilization = true;
        if ((act_track->flag & TRACK_USE_2D_STAB) == 0) {
          act_track->flag |= TRACK_USE_2D_STAB;
        }
        else {
          stab->tot_track--;
        }
        BLI_assert(0 <= stab->tot_track);
      }
      if (track->flag & TRACK_USE_2D_STAB_ROT) {
        update_stabilization = true;
        if ((act_track->flag & TRACK_USE_2D_STAB_ROT) == 0) {
          act_track->flag |= TRACK_USE_2D_STAB_ROT;
        }
        else {
          stab->tot_rot_track--;
        }
        BLI_assert(0 <= stab->tot_rot_track);
      }

      for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track != NULL;
           plane_track = plane_track->next) {
        if (BKE_tracking_plane_track_has_point_track(plane_track, track)) {
          BKE_tracking_plane_track_replace_point_track(plane_track, track, act_track);
          if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
            BLI_gset_insert(point_tracks, plane_track);
          }
        }
      }

      BKE_tracking_track_free(track);
      BLI_freelinkN(tracksbase, track);
    }
  }

  if (update_stabilization) {
    WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
  }

  GSetIterator gs_iter;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  GSET_ITER (gs_iter, point_tracks) {
    MovieTrackingPlaneTrack *plane_track = BLI_gsetIterator_getKey(&gs_iter);
    BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
  }

  BLI_gset_free(point_tracks, NULL);
  DEG_id_tag_update(&clip->id, 0);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_join_tracks(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join Tracks";
  ot->description = "Join selected tracks";
  ot->idname = "CLIP_OT_join_tracks";

  /* api callbacks */
  ot->exec = join_tracks_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** lock tracks operator *********************/

enum {
  TRACK_ACTION_LOCK = 0,
  TRACK_ACTION_UNLOCK = 1,
  TRACK_ACTION_TOGGLE = 2,
};

static int lock_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  int action = RNA_enum_get(op->ptr, "action");

  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      switch (action) {
        case TRACK_ACTION_LOCK:
          track->flag |= TRACK_LOCKED;
          break;
        case TRACK_ACTION_UNLOCK:
          track->flag &= ~TRACK_LOCKED;
          break;
        case TRACK_ACTION_TOGGLE:
          track->flag ^= TRACK_LOCKED;
          break;
      }
    }
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_lock_tracks(wmOperatorType *ot)
{
  static const EnumPropertyItem actions_items[] = {
      {TRACK_ACTION_LOCK, "LOCK", 0, "Lock", "Lock selected tracks"},
      {TRACK_ACTION_UNLOCK, "UNLOCK", 0, "Unlock", "Unlock selected tracks"},
      {TRACK_ACTION_TOGGLE, "TOGGLE", 0, "Toggle", "Toggle locked flag for selected tracks"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Lock Tracks";
  ot->description = "Lock/unlock selected tracks";
  ot->idname = "CLIP_OT_lock_tracks";

  /* api callbacks */
  ot->exec = lock_tracks_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Lock action to execute");
}

/********************** set keyframe operator *********************/

enum {
  SOLVER_KEYFRAME_A = 0,
  SOLVER_KEYFRAME_B = 1,
};

static int set_solver_keyframe_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
  int keyframe = RNA_enum_get(op->ptr, "keyframe");
  int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr);

  if (keyframe == SOLVER_KEYFRAME_A) {
    object->keyframe1 = framenr;
  }
  else {
    object->keyframe2 = framenr;
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_solver_keyframe(wmOperatorType *ot)
{
  static const EnumPropertyItem keyframe_items[] = {
      {SOLVER_KEYFRAME_A, "KEYFRAME_A", 0, "Keyframe A", ""},
      {SOLVER_KEYFRAME_B, "KEYFRAME_B", 0, "Keyframe B", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Solver Keyframe";
  ot->description = "Set keyframe used by solver";
  ot->idname = "CLIP_OT_set_solver_keyframe";

  /* api callbacks */
  ot->exec = set_solver_keyframe_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "keyframe", keyframe_items, 0, "Keyframe", "Keyframe to set");
}

/********************** track copy color operator *********************/

static int track_copy_color_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);

  MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
  if (act_track == NULL) {
    return OPERATOR_CANCELLED;
  }

  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track) && track != act_track) {
      track->flag &= ~TRACK_CUSTOMCOLOR;
      if (act_track->flag & TRACK_CUSTOMCOLOR) {
        copy_v3_v3(track->color, act_track->color);
        track->flag |= TRACK_CUSTOMCOLOR;
      }
    }
  }

  DEG_id_tag_update(&clip->id, 0);
  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_track_copy_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Color";
  ot->description = "Copy color to all selected tracks";
  ot->idname = "CLIP_OT_track_copy_color";

  /* api callbacks */
  ot->exec = track_copy_color_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** clean tracks operator *********************/

static bool is_track_clean(MovieTrackingTrack *track, int frames, int del)
{
  bool ok = true;
  int prev = -1, count = 0;
  MovieTrackingMarker *markers = track->markers, *new_markers = NULL;
  int start_disabled = 0;
  int markersnr = track->markersnr;

  if (del) {
    new_markers = MEM_callocN(markersnr * sizeof(MovieTrackingMarker), "track cleaned markers");
  }

  for (int a = 0; a < markersnr; a++) {
    int end = 0;

    if (prev == -1) {
      if ((markers[a].flag & MARKER_DISABLED) == 0) {
        prev = a;
      }
      else {
        start_disabled = 1;
      }
    }

    if (prev >= 0) {
      end = a == markersnr - 1;
      end |= (a < markersnr - 1) && (markers[a].framenr != markers[a + 1].framenr - 1 ||
                                     markers[a].flag & MARKER_DISABLED);
    }

    if (end) {
      int segok = 1, len = 0;

      if (a != prev && markers[a].framenr != markers[a - 1].framenr + 1) {
        len = a - prev;
      }
      else if (markers[a].flag & MARKER_DISABLED) {
        len = a - prev;
      }
      else {
        len = a - prev + 1;
      }

      if (frames) {
        if (len < frames) {
          segok = 0;
          ok = 0;

          if (!del) {
            break;
          }
        }
      }

      if (del) {
        if (segok) {
          int t = len;

          if (markers[a].flag & MARKER_DISABLED) {
            t++;
          }

          /* Place disabled marker in front of current segment. */
          if (start_disabled) {
            memcpy(new_markers + count, markers + prev, sizeof(MovieTrackingMarker));
            new_markers[count].framenr--;
            new_markers[count].flag |= MARKER_DISABLED;

            count++;
            start_disabled = 0;
          }

          memcpy(new_markers + count, markers + prev, t * sizeof(MovieTrackingMarker));
          count += t;
        }
        else if (markers[a].flag & MARKER_DISABLED) {
          /* Current segment which would be deleted was finished by
           * disabled marker, so next segment should be started from
           * disabled marker.
           */
          start_disabled = 1;
        }
      }

      prev = -1;
    }
  }

  if (del && count == 0) {
    ok = 0;
  }

  if (del) {
    MEM_freeN(track->markers);

    if (count) {
      track->markers = new_markers;
    }
    else {
      track->markers = NULL;
      MEM_freeN(new_markers);
    }

    track->markersnr = count;
  }

  return ok;
}

static int clean_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
  int frames = RNA_int_get(op->ptr, "frames");
  int action = RNA_enum_get(op->ptr, "action");
  float error = RNA_float_get(op->ptr, "error");

  if (error && action == TRACKING_CLEAN_DELETE_SEGMENT) {
    action = TRACKING_CLEAN_DELETE_TRACK;
  }

  for (MovieTrackingTrack *track = tracksbase->first, *next_track; track != NULL;
       track = next_track) {
    next_track = track->next;

    if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
      bool ok;

      ok = (is_track_clean(track, frames, action == TRACKING_CLEAN_DELETE_SEGMENT)) &&
           ((error == 0.0f) || (track->flag & TRACK_HAS_BUNDLE) == 0 || (track->error < error));

      if (!ok) {
        if (action == TRACKING_CLEAN_SELECT) {
          BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
        }
        else if (action == TRACKING_CLEAN_DELETE_TRACK) {
          if (track == act_track) {
            clip->tracking.act_track = NULL;
          }
          BKE_tracking_track_free(track);
          BLI_freelinkN(tracksbase, track);
          track = NULL;
        }

        /* Happens when all tracking segments are not long enough. */
        if (track && track->markersnr == 0) {
          if (track == act_track) {
            clip->tracking.act_track = NULL;
          }
          BKE_tracking_track_free(track);
          BLI_freelinkN(tracksbase, track);
        }
      }
    }
  }

  DEG_id_tag_update(&clip->id, 0);
  BKE_tracking_dopesheet_tag_update(tracking);

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, clip);

  return OPERATOR_FINISHED;
}

static int clean_tracks_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if (!RNA_struct_property_is_set(op->ptr, "frames")) {
    RNA_int_set(op->ptr, "frames", clip->tracking.settings.clean_frames);
  }

  if (!RNA_struct_property_is_set(op->ptr, "error")) {
    RNA_float_set(op->ptr, "error", clip->tracking.settings.clean_error);
  }

  if (!RNA_struct_property_is_set(op->ptr, "action")) {
    RNA_enum_set(op->ptr, "action", clip->tracking.settings.clean_action);
  }

  return clean_tracks_exec(C, op);
}

void CLIP_OT_clean_tracks(wmOperatorType *ot)
{
  static const EnumPropertyItem actions_items[] = {
      {TRACKING_CLEAN_SELECT, "SELECT", 0, "Select", "Select unclean tracks"},
      {TRACKING_CLEAN_DELETE_TRACK, "DELETE_TRACK", 0, "Delete Track", "Delete unclean tracks"},
      {TRACKING_CLEAN_DELETE_SEGMENT,
       "DELETE_SEGMENTS",
       0,
       "Delete Segments",
       "Delete unclean segments of tracks"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Clean Tracks";
  ot->description = "Clean tracks with high error values or few frames";
  ot->idname = "CLIP_OT_clean_tracks";

  /* api callbacks */
  ot->exec = clean_tracks_exec;
  ot->invoke = clean_tracks_invoke;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(ot->srna,
              "frames",
              0,
              0,
              INT_MAX,
              "Tracked Frames",
              "Effect on tracks which are tracked less than "
              "specified amount of frames",
              0,
              INT_MAX);
  RNA_def_float(ot->srna,
                "error",
                0.0f,
                0.0f,
                FLT_MAX,
                "Reprojection Error",
                "Effect on tracks which have got larger re-projection error",
                0.0f,
                100.0f);
  RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Cleanup action to execute");
}

/********************** add tracking object *********************/

static int tracking_object_new_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;

  BKE_tracking_object_add(tracking, "Object");

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_tracking_object_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Tracking Object";
  ot->description = "Add new object for tracking";
  ot->idname = "CLIP_OT_tracking_object_new";

  /* api callbacks */
  ot->exec = tracking_object_new_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** remove tracking object *********************/

static int tracking_object_remove_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *object;

  object = BKE_tracking_object_get_active(tracking);

  if (object->flag & TRACKING_OBJECT_CAMERA) {
    BKE_report(op->reports, RPT_WARNING, "Object used for camera tracking cannot be deleted");
    return OPERATOR_CANCELLED;
  }

  BKE_tracking_object_delete(tracking, object);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_tracking_object_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Tracking Object";
  ot->description = "Remove object for tracking";
  ot->idname = "CLIP_OT_tracking_object_remove";

  /* api callbacks */
  ot->exec = tracking_object_remove_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** copy tracks to clipboard operator *********************/

static int copy_tracks_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

  clip_tracking_clear_invisible_track_selection(sc, clip);

  BKE_tracking_clipboard_copy_tracks(tracking, object);

  return OPERATOR_FINISHED;
}

void CLIP_OT_copy_tracks(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Tracks";
  ot->description = "Copy selected tracks to clipboard";
  ot->idname = "CLIP_OT_copy_tracks";

  /* api callbacks */
  ot->exec = copy_tracks_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/********************* paste tracks from clipboard operator ********************/

static bool paste_tracks_poll(bContext *C)
{
  if (ED_space_clip_tracking_poll(C)) {
    return BKE_tracking_clipboard_has_tracks();
  }

  return 0;
}

static int paste_tracks_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
  ListBase *tracks_base = BKE_tracking_object_get_tracks(tracking, object);

  BKE_tracking_tracks_deselect_all(tracks_base);
  BKE_tracking_clipboard_paste_tracks(tracking, object);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_paste_tracks(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Tracks";
  ot->description = "Paste tracks from clipboard";
  ot->idname = "CLIP_OT_paste_tracks";

  /* api callbacks */
  ot->exec = paste_tracks_exec;
  ot->poll = paste_tracks_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Insert track keyframe operator *********************/

static void keyframe_set_flag(bContext *C, bool set)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  ListBase *tracks_base = BKE_tracking_get_active_tracks(tracking);
  for (MovieTrackingTrack *track = tracks_base->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      if (set) {
        MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);
        marker->flag &= ~MARKER_TRACKED;
      }
      else {
        MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);
        if (marker != NULL) {
          marker->flag |= MARKER_TRACKED;
        }
      }
    }
  }

  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track != NULL;
       plane_track = plane_track->next) {
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      if (set) {
        MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_ensure(plane_track,
                                                                                  framenr);
        if (plane_marker->flag & PLANE_MARKER_TRACKED) {
          plane_marker->flag &= ~PLANE_MARKER_TRACKED;
          BKE_tracking_track_plane_from_existing_motion(plane_track, plane_marker->framenr);
        }
      }
      else {
        MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get_exact(plane_track,
                                                                                     framenr);
        if (plane_marker) {
          if ((plane_marker->flag & PLANE_MARKER_TRACKED) == 0) {
            plane_marker->flag |= PLANE_MARKER_TRACKED;
            BKE_tracking_retrack_plane_from_existing_motion_at_segment(plane_track,
                                                                       plane_marker->framenr);
          }
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
}

static int keyframe_insert_exec(bContext *C, wmOperator *UNUSED(op))
{
  keyframe_set_flag(C, true);
  return OPERATOR_FINISHED;
}

void CLIP_OT_keyframe_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert keyframe";
  ot->description = "Insert a keyframe to selected tracks at current frame";
  ot->idname = "CLIP_OT_keyframe_insert";

  /* api callbacks */
  ot->poll = ED_space_clip_tracking_poll;
  ot->exec = keyframe_insert_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Delete track keyframe operator *********************/

static int keyframe_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  keyframe_set_flag(C, false);
  return OPERATOR_FINISHED;
}

void CLIP_OT_keyframe_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete keyframe";
  ot->description = "Delete a keyframe from selected tracks at current frame";
  ot->idname = "CLIP_OT_keyframe_delete";

  /* api callbacks */
  ot->poll = ED_space_clip_tracking_poll;
  ot->exec = keyframe_delete_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
