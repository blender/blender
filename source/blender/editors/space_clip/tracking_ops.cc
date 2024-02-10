/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_image.h"
#include "BKE_movieclip.h"
#include "BKE_report.hh"
#include "BKE_tracking.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "clip_intern.h"
#include "tracking_ops_intern.h"

/* -------------------------------------------------------------------- */
/** \name Add Marker Operator
 * \{ */

static bool add_marker(const bContext *C, float x, float y)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingTrack *track;
  int width, height;
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  ED_space_clip_get_size(sc, &width, &height);

  if (width == 0 || height == 0) {
    return false;
  }

  track = BKE_tracking_track_add(tracking, &tracking_object->tracks, x, y, framenr, width, height);

  BKE_tracking_track_select(&tracking_object->tracks, track, TRACK_AREA_ALL, false);
  BKE_tracking_plane_tracks_deselect_all(&tracking_object->plane_tracks);

  tracking_object->active_track = track;
  tracking_object->active_plane_track = nullptr;

  return true;
}

static int add_marker_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  float pos[2];

  ClipViewLockState lock_state;
  ED_clip_view_lock_state_store(C, &lock_state);

  RNA_float_get_array(op->ptr, "location", pos);

  if (!add_marker(C, pos[0], pos[1])) {
    return OPERATOR_CANCELLED;
  }

  ED_clip_view_lock_state_restore_no_jump(C, &lock_state);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

static int add_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  if (!RNA_struct_property_is_set(op->ptr, "location")) {
    /* If location is not set, use mouse position as default. */
    float co[2];
    ED_clip_mouse_pos(sc, region, event->mval, co);
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
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Location of marker on frame",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Marker Operator
 * \{ */

static int add_marker_at_click_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ED_workspace_status_text(C, IFACE_("Use LMB click to define location where place the marker"));

  /* Add modal handler for ESC. */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int add_marker_at_click_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  switch (event->type) {
    case MOUSEMOVE:
      return OPERATOR_RUNNING_MODAL;

    case LEFTMOUSE: {
      SpaceClip *sc = CTX_wm_space_clip(C);
      MovieClip *clip = ED_space_clip_get_clip(sc);
      ARegion *region = CTX_wm_region(C);
      float pos[2];

      ED_workspace_status_text(C, nullptr);

      ED_clip_point_stable_pos(sc,
                               region,
                               event->xy[0] - region->winrct.xmin,
                               event->xy[1] - region->winrct.ymin,
                               &pos[0],
                               &pos[1]);

      if (!add_marker(C, pos[0], pos[1])) {
        return OPERATOR_CANCELLED;
      }

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
      return OPERATOR_FINISHED;
    }

    case EVT_ESCKEY:
      ED_workspace_status_text(C, nullptr);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Track Operator
 * \{ */

static int delete_track_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  bool changed = false;

  /* Delete selected plane tracks. */
  LISTBASE_FOREACH_MUTABLE (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks)
  {
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      clip_delete_plane_track(C, clip, plane_track);
      changed = true;
    }
  }

  /* Remove selected point tracks (they'll also be removed from planes which uses them). */
  LISTBASE_FOREACH_MUTABLE (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      clip_delete_track(C, clip, track);
      changed = true;
    }
  }

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
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = delete_track_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Marker Operator
 * \{ */

static int delete_marker_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  bool changed = false;

  LISTBASE_FOREACH_MUTABLE (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);
      if (marker != nullptr) {
        clip_delete_marker(C, clip, track, marker);
        changed = true;
      }
    }
  }

  LISTBASE_FOREACH_MUTABLE (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks)
  {
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get_exact(plane_track,
                                                                                   framenr);
      if (plane_marker != nullptr) {
        if (plane_track->markersnr == 1) {
          BKE_tracking_plane_track_free(plane_track);
          BLI_freelinkN(&tracking_object->plane_tracks, plane_track);
        }
        else {
          BKE_tracking_plane_marker_delete(plane_track, framenr);
        }
        changed = true;
      }
    }
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
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = delete_marker_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slide Marker Operator
 * \{ */

enum eSlideAction {
  SLIDE_ACTION_NONE,

  SLIDE_ACTION_POS,
  SLIDE_ACTION_SIZE,
  SLIDE_ACTION_OFFSET,
  SLIDE_ACTION_TILT_SIZE,
};

struct SlideMarkerData {
  short area;
  eSlideAction action;
  MovieTrackingTrack *track;
  MovieTrackingMarker *marker;

  int mval[2];
  int width, height;
  float *min, *max, *pos, (*corners)[2];

  bool lock, accurate;

  /* Data to restore on cancel. */
  float old_search_min[2], old_search_max[2], old_pos[2];
  float old_corners[4][2];
  float (*old_markers)[2];
};

static void slide_marker_tilt_slider_relative(const float pattern_corners[4][2], float r_slider[2])
{
  add_v2_v2v2(r_slider, pattern_corners[1], pattern_corners[2]);
}

static SlideMarkerData *create_slide_marker_data(SpaceClip *sc,
                                                 MovieTrackingTrack *track,
                                                 MovieTrackingMarker *marker,
                                                 const wmEvent *event,
                                                 int area,
                                                 int corner,
                                                 eSlideAction action,
                                                 int width,
                                                 int height)
{
  SlideMarkerData *data = MEM_cnew<SlideMarkerData>("slide marker data");
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  marker = BKE_tracking_marker_ensure(track, framenr);

  data->area = area;
  data->action = action;
  data->track = track;
  data->marker = marker;

  if (area == TRACK_AREA_POINT) {
    data->pos = marker->pos;
  }
  else if (area == TRACK_AREA_PAT) {
    if (action == SLIDE_ACTION_POS) {
      data->corners = marker->pattern_corners;
      data->pos = marker->pattern_corners[corner];
    }
    else if (action == SLIDE_ACTION_TILT_SIZE) {
      data->corners = marker->pattern_corners;
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

  return data;
}

static bool slide_check_corners(float (*corners)[2])
{
  float cross = 0.0f;
  const float p[2] = {0.0f, 0.0f};

  if (!isect_point_quad_v2(p, corners[0], corners[1], corners[2], corners[3])) {
    return false;
  }

  for (int i = 0; i < 4; i++) {
    float v1[2], v2[2];

    int next = (i + 1) % 4;
    int prev = (4 + i - 1) % 4;

    sub_v2_v2v2(v1, corners[i], corners[prev]);
    sub_v2_v2v2(v2, corners[next], corners[i]);

    float cur_cross = cross_v2v2(v1, v2);

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

static MovieTrackingTrack *tracking_marker_check_slide(
    bContext *C, const float co[2], int *r_area, eSlideAction *r_action, int *r_corner)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);

  TrackPickOptions options = ed_tracking_pick_options_defaults();
  options.selected_only = true;
  options.unlocked_only = true;
  options.enabled_only = true;
  const PointTrackPick track_pick = ed_tracking_pick_point_track(&options, C, co);

  if (ed_tracking_point_track_pick_empty(&track_pick) ||
      !ed_tracking_point_track_pick_can_slide(space_clip, &track_pick))
  {
    return nullptr;
  }

  const eTrackArea area = track_pick.area;
  eSlideAction action = SLIDE_ACTION_NONE;
  int corner = -1;

  switch (area) {
    case TRACK_AREA_NONE:
    case TRACK_AREA_ALL:
      BLI_assert_msg(0, "Expected single track area");
      return nullptr;

    case TRACK_AREA_POINT:
      action = SLIDE_ACTION_POS;
      break;

    case TRACK_AREA_PAT:
      if (track_pick.area_detail == TRACK_PICK_AREA_DETAIL_TILT_SIZE) {
        action = SLIDE_ACTION_TILT_SIZE;
      }
      else if (track_pick.area_detail == TRACK_PICK_AREA_DETAIL_POSITION) {
        action = SLIDE_ACTION_POS;
        corner = track_pick.corner_index;
      }
      else {
        BLI_assert_msg(0, "Unhandled pattern area");
        return nullptr;
      }
      break;

    case TRACK_AREA_SEARCH:
      if (track_pick.area_detail == TRACK_PICK_AREA_DETAIL_SIZE) {
        action = SLIDE_ACTION_SIZE;
      }
      else if (track_pick.area_detail == TRACK_PICK_AREA_DETAIL_OFFSET) {
        action = SLIDE_ACTION_OFFSET;
      }
      else {
        BLI_assert_msg(0, "Unhandled search area");
        return nullptr;
      }
      break;
  }

  if (r_area) {
    *r_area = area;
  }
  if (r_action) {
    *r_action = action;
  }
  if (r_corner) {
    *r_corner = corner;
  }

  return track_pick.track;
}

MovieTrackingTrack *tracking_find_slidable_track_in_proximity(bContext *C, const float co[2])
{
  return tracking_marker_check_slide(C, co, nullptr, nullptr, nullptr);
}

static SlideMarkerData *slide_marker_customdata(bContext *C, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  MovieTrackingTrack *track;
  int width, height;
  float co[2];
  SlideMarkerData *customdata = nullptr;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  eSlideAction action;
  int area, corner;

  ED_space_clip_get_size(sc, &width, &height);

  if (width == 0 || height == 0) {
    return nullptr;
  }

  ED_clip_mouse_pos(sc, region, event->mval, co);

  track = tracking_marker_check_slide(C, co, &area, &action, &corner);
  if (track != nullptr) {
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
    customdata = create_slide_marker_data(
        sc, track, marker, event, area, corner, action, width, height);
  }

  return customdata;
}

static int slide_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SlideMarkerData *slidedata = slide_marker_customdata(C, event);
  if (slidedata != nullptr) {
    SpaceClip *sc = CTX_wm_space_clip(C);
    MovieClip *clip = ED_space_clip_get_clip(sc);
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

    tracking_object->active_track = slidedata->track;
    tracking_object->active_plane_track = nullptr;

    op->customdata = slidedata;

    clip_tracking_hide_cursor(C);
    WM_event_add_modal_handler(C, op);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

static void cancel_mouse_slide(SlideMarkerData *data)
{
  MovieTrackingMarker *marker = data->marker;

  memcpy(marker->pattern_corners, data->old_corners, sizeof(marker->pattern_corners));
  copy_v2_v2(marker->search_min, data->old_search_min);
  copy_v2_v2(marker->search_max, data->old_search_max);
  copy_v2_v2(marker->pos, data->old_pos);

  if (data->old_markers != nullptr) {
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
    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
    const int framenr = ED_space_clip_get_clip_frame_number(sc);

    LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
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
  if (data->old_markers != nullptr) {
    MEM_freeN(data->old_markers);
  }
  MEM_freeN(data);
}

static int slide_marker_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  SlideMarkerData *data = (SlideMarkerData *)op->customdata;
  float dx, dy, mdelta[2];

  switch (event->type) {
    case EVT_LEFTCTRLKEY:
    case EVT_RIGHTCTRLKEY:
    case EVT_LEFTSHIFTKEY:
    case EVT_RIGHTSHIFTKEY:
      if (data->action == SLIDE_ACTION_SIZE) {
        if (ELEM(event->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY)) {
          data->lock = event->val == KM_RELEASE;
        }
      }

      if (ELEM(event->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY)) {
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
        data->pos[0] += dx;
        data->pos[1] += dy;

        WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
        DEG_id_tag_update(&sc->clip->id, 0);
      }
      else if (data->area == TRACK_AREA_PAT) {
        if (data->action == SLIDE_ACTION_POS) {
          float prev_pos[2];
          copy_v2_v2(prev_pos, data->pos);

          data->pos[0] += dx;
          data->pos[1] += dy;

          if (!slide_check_corners(data->corners)) {
            copy_v2_v2(data->pos, prev_pos);
          }

          /* Allow pattern to be arbitrary size and resize search area if needed. */
          BKE_tracking_marker_clamp_search_size(data->marker);
        }
        else if (data->action == SLIDE_ACTION_TILT_SIZE) {
          const float delta[2] = {dx, dy};

          /* Slider position relative to the marker position using current state of pattern
           * corners. */
          float slider[2];
          slide_marker_tilt_slider_relative(data->corners, slider);

          /* Vector which connects marker position with the slider state at the current corners
           * state.
           * The coordinate is in the pixel space. */
          float start_px[2];
          copy_v2_v2(start_px, slider);
          start_px[0] *= data->width;
          start_px[1] *= data->height;

          /* Vector which connects marker position with the slider state with the new mouse delta
           * taken into account.
           * The coordinate is in the pixel space. */
          float end_px[2];
          add_v2_v2v2(end_px, slider, delta);
          end_px[0] *= data->width;
          end_px[1] *= data->height;

          float scale = 1.0f;
          if (len_squared_v2(start_px) != 0.0f) {
            scale = len_v2(end_px) / len_v2(start_px);

            if (scale < 0.0f) {
              scale = 0.0;
            }
          }

          const float angle = -angle_signed_v2v2(start_px, end_px);

          for (int a = 0; a < 4; a++) {
            float vec[2];

            mul_v2_fl(data->corners[a], scale);

            copy_v2_v2(vec, data->corners[a]);
            vec[0] *= data->width;
            vec[1] *= data->height;

            data->corners[a][0] = (vec[0] * cosf(angle) - vec[1] * sinf(angle)) / data->width;
            data->corners[a][1] = (vec[1] * cosf(angle) + vec[0] * sinf(angle)) / data->height;
          }

          BKE_tracking_marker_clamp_search_size(data->marker);
        }
      }
      else if (data->area == TRACK_AREA_SEARCH) {
        if (data->action == SLIDE_ACTION_SIZE) {
          data->min[0] -= dx;
          data->min[1] += dy;

          data->max[0] += dx;
          data->max[1] -= dy;

          BKE_tracking_marker_clamp_search_size(data->marker);
        }
        else if (data->action == SLIDE_ACTION_OFFSET) {
          const float delta[2] = {dx, dy};
          add_v2_v2(data->min, delta);
          add_v2_v2(data->max, delta);

          BKE_tracking_marker_clamp_search_position(data->marker);
        }
      }

      data->marker->flag &= ~MARKER_TRACKED;

      copy_v2_v2_int(data->mval, event->mval);

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, nullptr);

      break;

    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        apply_mouse_slide(C, data);
        free_slide_data(data);

        clip_tracking_show_cursor(C);

        return OPERATOR_FINISHED;
      }

      break;

    case EVT_ESCKEY:
      cancel_mouse_slide(data);

      free_slide_data(data);

      clip_tracking_show_cursor(C);

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, nullptr);

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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "offset",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Offset",
                       "Offset in floating-point units, 1.0 is the width and height of the image",
                       -FLT_MAX,
                       FLT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Track Operator
 * \{ */

static int clear_track_path_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const eTrackClearAction action = eTrackClearAction(RNA_enum_get(op->ptr, "action"));
  const bool clear_active = RNA_boolean_get(op->ptr, "clear_active");
  const int framenr = ED_space_clip_get_clip_frame_number(sc);

  if (clear_active) {
    if (tracking_object->active_track != nullptr) {
      BKE_tracking_track_path_clear(tracking_object->active_track, framenr, action);
    }
  }
  else {
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (TRACK_VIEW_SELECTED(sc, track)) {
        BKE_tracking_track_path_clear(track, framenr, action);
      }
    }
  }

  BKE_tracking_dopesheet_tag_update(&clip->tracking);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_clear_track_path(wmOperatorType *ot)
{
  static const EnumPropertyItem clear_path_actions[] = {
      {TRACK_CLEAR_UPTO, "UPTO", 0, "Clear Up To", "Clear path up to current frame"},
      {TRACK_CLEAR_REMAINED,
       "REMAINED",
       0,
       "Clear Remained",
       "Clear path at remaining frames (after current)"},
      {TRACK_CLEAR_ALL, "ALL", 0, "Clear All", "Clear the whole path"},
      {0, nullptr, 0, nullptr, nullptr},
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
                  false,
                  "Clear Active",
                  "Clear active track only instead of all selected tracks");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Disable Markers Operator
 * \{ */

enum {
  MARKER_OP_DISABLE = 0,
  MARKER_OP_ENABLE = 1,
  MARKER_OP_TOGGLE = 2,
};

static int disable_markers_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int action = RNA_enum_get(op->ptr, "action");
  const int framenr = ED_space_clip_get_clip_frame_number(sc);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
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
      {0, nullptr, 0, nullptr, nullptr},
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Tracks Operator
 * \{ */

static int hide_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  const int unselected = RNA_boolean_get(op->ptr, "unselected");

  /* Hide point tracks. */
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (unselected == 0 && TRACK_VIEW_SELECTED(sc, track)) {
      track->flag |= TRACK_HIDDEN;
    }
    else if (unselected == 1 && !TRACK_VIEW_SELECTED(sc, track)) {
      track->flag |= TRACK_HIDDEN;
    }
  }

  const MovieTrackingTrack *active_track = tracking_object->active_track;
  if (active_track != nullptr && active_track->flag & TRACK_HIDDEN) {
    tracking_object->active_track = nullptr;
  }

  /* Hide place tracks. */
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if (unselected == 0 && plane_track->flag & SELECT) {
      plane_track->flag |= PLANE_TRACK_HIDDEN;
    }
    else if (unselected == 1 && (plane_track->flag & SELECT) == 0) {
      plane_track->flag |= PLANE_TRACK_HIDDEN;
    }
  }

  const MovieTrackingPlaneTrack *active_plane_track = tracking_object->active_plane_track;
  if (active_plane_track != nullptr && active_plane_track->flag & TRACK_HIDDEN) {
    tracking_object->active_plane_track = nullptr;
  }

  BKE_tracking_dopesheet_tag_update(tracking);
  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, nullptr);

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
  RNA_def_boolean(ot->srna, "unselected", false, "Unselected", "Hide unselected tracks");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Tracks Clear Operator
 * \{ */

static int hide_tracks_clear_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  /* Unhide point tracks. */
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    track->flag &= ~TRACK_HIDDEN;
  }

  /* Unhide plane tracks. */
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    plane_track->flag &= ~PLANE_TRACK_HIDDEN;
  }

  BKE_tracking_dopesheet_tag_update(&clip->tracking);

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, nullptr);

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Jump Operator
 * \{ */

static bool frame_jump_poll(bContext *C)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  return space_clip != nullptr;
}

static int frame_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  int pos = RNA_enum_get(op->ptr, "position");
  int delta;

  if (pos <= 1) { /* jump to path */
    MovieTrackingTrack *active_track = tracking_object->active_track;
    if (active_track == nullptr) {
      return OPERATOR_CANCELLED;
    }

    delta = pos == 1 ? 1 : -1;
    while (sc->user.framenr + delta >= scene->r.sfra && sc->user.framenr + delta <= scene->r.efra)
    {
      int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr + delta);
      MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(active_track, framenr);

      if (marker == nullptr || marker->flag & MARKER_DISABLED) {
        break;
      }

      sc->user.framenr += delta;
    }
  }
  else { /* to failed frame */
    if (tracking_object->reconstruction.flag & TRACKING_RECONSTRUCTED) {
      int framenr = ED_space_clip_get_clip_frame_number(sc);

      delta = pos == 3 ? 1 : -1;
      framenr += delta;

      while (framenr + delta >= scene->r.sfra && framenr + delta <= scene->r.efra) {
        MovieReconstructedCamera *cam = BKE_tracking_camera_get_reconstructed(
            tracking, tracking_object, framenr);

        if (cam == nullptr) {
          sc->user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, framenr);
          break;
        }

        framenr += delta;
      }
    }
  }

  if (scene->r.cfra != sc->user.framenr) {
    scene->r.cfra = sc->user.framenr;
    DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);

    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void CLIP_OT_frame_jump(wmOperatorType *ot)
{
  static const EnumPropertyItem position_items[] = {
      {0, "PATHSTART", 0, "Path Start", "Jump to start of current path"},
      {1, "PATHEND", 0, "Path End", "Jump to end of current path"},
      {2, "FAILEDPREV", 0, "Previous Failed", "Jump to previous failed frame"},
      {2, "FAILNEXT", 0, "Next Failed", "Jump to next failed frame"},
      {0, nullptr, 0, nullptr, nullptr},
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join Tracks Operator
 * \{ */

static int join_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingStabilization *stabilization = &tracking->stabilization;
  bool update_stabilization = false;

  MovieTrackingTrack *active_track = tracking_object->active_track;
  if (active_track == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active track to join to");
    return OPERATOR_CANCELLED;
  }

  GSet *point_tracks = BLI_gset_ptr_new(__func__);

  LISTBASE_FOREACH_MUTABLE (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track) && track != active_track) {
      BKE_tracking_tracks_join(tracking, active_track, track);

      if (track->flag & TRACK_USE_2D_STAB) {
        update_stabilization = true;
        if ((active_track->flag & TRACK_USE_2D_STAB) == 0) {
          active_track->flag |= TRACK_USE_2D_STAB;
        }
        else {
          stabilization->tot_track--;
        }
        BLI_assert(0 <= stabilization->tot_track);
      }
      if (track->flag & TRACK_USE_2D_STAB_ROT) {
        update_stabilization = true;
        if ((active_track->flag & TRACK_USE_2D_STAB_ROT) == 0) {
          active_track->flag |= TRACK_USE_2D_STAB_ROT;
        }
        else {
          stabilization->tot_rot_track--;
        }
        BLI_assert(0 <= stabilization->tot_rot_track);
      }

      LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
        if (BKE_tracking_plane_track_has_point_track(plane_track, track)) {
          BKE_tracking_plane_track_replace_point_track(plane_track, track, active_track);
          if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
            BLI_gset_insert(point_tracks, plane_track);
          }
        }
      }

      BKE_tracking_track_free(track);
      BLI_freelinkN(&tracking_object->tracks, track);
    }
  }

  if (update_stabilization) {
    WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
  }

  GSetIterator gs_iter;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  GSET_ITER (gs_iter, point_tracks) {
    MovieTrackingPlaneTrack *plane_track = static_cast<MovieTrackingPlaneTrack *>(
        BLI_gsetIterator_getKey(&gs_iter));
    BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
  }

  BLI_gset_free(point_tracks, nullptr);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Average Tracks Operator
 * \{ */

static int average_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  /* Collect source tracks. */
  int num_source_tracks;
  MovieTrackingTrack **source_tracks = BKE_tracking_selected_tracks_in_active_object(
      tracking, &num_source_tracks);
  if (num_source_tracks == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Create new empty track, which will be the averaged result.
   * Makes it simple to average all selection to it. */
  MovieTrackingTrack *result_track = BKE_tracking_track_add_empty(tracking,
                                                                  &tracking_object->tracks);

  /* Perform averaging. */
  BKE_tracking_tracks_average(result_track, source_tracks, num_source_tracks);

  const bool keep_original = RNA_boolean_get(op->ptr, "keep_original");
  if (!keep_original) {
    for (int i = 0; i < num_source_tracks; i++) {
      clip_delete_track(C, clip, source_tracks[i]);
    }
  }

  /* Update selection, making the result track active and selected. */
  /* TODO(sergey): Should become some sort of utility function available for all operators. */

  BKE_tracking_track_select(&tracking_object->tracks, result_track, TRACK_AREA_ALL, false);
  BKE_tracking_plane_tracks_deselect_all(&tracking_object->plane_tracks);

  tracking_object->active_track = result_track;
  tracking_object->active_plane_track = nullptr;

  /* Inform the dependency graph and interface about changes. */
  DEG_id_tag_update(&clip->id, 0);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  /* Free memory. */
  MEM_freeN(source_tracks);

  return OPERATOR_FINISHED;
}

static int average_tracks_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  PropertyRNA *prop_keep_original = RNA_struct_find_property(op->ptr, "keep_original");
  if (!RNA_property_is_set(op->ptr, prop_keep_original)) {
    SpaceClip *space_clip = CTX_wm_space_clip(C);
    MovieClip *clip = ED_space_clip_get_clip(space_clip);
    MovieTracking *tracking = &clip->tracking;

    const int num_selected_tracks = BKE_tracking_count_selected_tracks_in_active_object(tracking);

    if (num_selected_tracks == 1) {
      RNA_property_boolean_set(op->ptr, prop_keep_original, false);
    }
  }

  return average_tracks_exec(C, op);
}

void CLIP_OT_average_tracks(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Average Tracks";
  ot->description = "Average selected tracks into active";
  ot->idname = "CLIP_OT_average_tracks";

  /* API callbacks. */
  ot->exec = average_tracks_exec;
  ot->invoke = average_tracks_invoke;
  ot->poll = ED_space_clip_tracking_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna, "keep_original", true, "Keep Original", "Keep original tracks");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lock Tracks Operator
 * \{ */

enum {
  TRACK_ACTION_LOCK = 0,
  TRACK_ACTION_UNLOCK = 1,
  TRACK_ACTION_TOGGLE = 2,
};

static int lock_tracks_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int action = RNA_enum_get(op->ptr, "action");

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
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
      {0, nullptr, 0, nullptr, nullptr},
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Keyframe Operator
 * \{ */

enum {
  SOLVER_KEYFRAME_A = 0,
  SOLVER_KEYFRAME_B = 1,
};

static int set_solver_keyframe_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  const int keyframe = RNA_enum_get(op->ptr, "keyframe");
  const int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr);

  if (keyframe == SOLVER_KEYFRAME_A) {
    tracking_object->keyframe1 = framenr;
  }
  else {
    tracking_object->keyframe2 = framenr;
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_solver_keyframe(wmOperatorType *ot)
{
  static const EnumPropertyItem keyframe_items[] = {
      {SOLVER_KEYFRAME_A, "KEYFRAME_A", 0, "Keyframe A", ""},
      {SOLVER_KEYFRAME_B, "KEYFRAME_B", 0, "Keyframe B", ""},
      {0, nullptr, 0, nullptr, nullptr},
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Track Copy Color Operator
 * \{ */

static int track_copy_color_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  MovieTrackingTrack *active_track = tracking_object->active_track;
  if (active_track == nullptr) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track) && track != active_track) {
      track->flag &= ~TRACK_CUSTOMCOLOR;
      if (active_track->flag & TRACK_CUSTOMCOLOR) {
        copy_v3_v3(track->color, active_track->color);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clean Tracks Operator
 * \{ */

static bool is_track_clean(MovieTrackingTrack *track, int frames, int del)
{
  bool ok = true;
  int prev = -1, count = 0;
  MovieTrackingMarker *markers = track->markers, *new_markers = nullptr;
  int start_disabled = 0;
  int markersnr = track->markersnr;

  if (del) {
    new_markers = MEM_cnew_array<MovieTrackingMarker>(markersnr, "track cleaned markers");
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
          ok = false;

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
    ok = false;
  }

  if (del) {
    MEM_freeN(track->markers);

    if (count) {
      track->markers = new_markers;
    }
    else {
      track->markers = nullptr;
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
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  int frames = RNA_int_get(op->ptr, "frames");
  int action = RNA_enum_get(op->ptr, "action");
  float error = RNA_float_get(op->ptr, "error");

  if (error && action == TRACKING_CLEAN_DELETE_SEGMENT) {
    action = TRACKING_CLEAN_DELETE_TRACK;
  }

  LISTBASE_FOREACH_MUTABLE (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
      bool ok;

      ok = is_track_clean(track, frames, action == TRACKING_CLEAN_DELETE_SEGMENT) &&
           ((error == 0.0f) || (track->flag & TRACK_HAS_BUNDLE) == 0 || (track->error < error));

      if (!ok) {
        if (action == TRACKING_CLEAN_SELECT) {
          BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
        }
        else if (action == TRACKING_CLEAN_DELETE_TRACK) {
          if (track == tracking_object->active_track) {
            tracking_object->active_track = nullptr;
          }
          BKE_tracking_track_free(track);
          BLI_freelinkN(&tracking_object->tracks, track);
          track = nullptr;
        }

        /* Happens when all tracking segments are not long enough. */
        if (track && track->markersnr == 0) {
          if (track == tracking_object->active_track) {
            tracking_object->active_track = nullptr;
          }
          BKE_tracking_track_free(track);
          BLI_freelinkN(&tracking_object->tracks, track);
        }
      }
    }
  }

  DEG_id_tag_update(&clip->id, 0);
  BKE_tracking_dopesheet_tag_update(tracking);

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, clip);

  return OPERATOR_FINISHED;
}

static int clean_tracks_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
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
      {0, nullptr, 0, nullptr, nullptr},
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
              "Affect tracks which are tracked less than the "
              "specified number of frames",
              0,
              INT_MAX);
  RNA_def_float(ot->srna,
                "error",
                0.0f,
                0.0f,
                FLT_MAX,
                "Reprojection Error",
                "Affect tracks which have a larger reprojection error",
                0.0f,
                100.0f);
  RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Cleanup action to execute");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Tracking Object
 * \{ */

static int tracking_object_new_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;

  BKE_tracking_object_add(tracking, "Object");

  DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Tracking Object
 * \{ */

static int tracking_object_remove_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
    BKE_report(op->reports, RPT_WARNING, "Object used for camera tracking cannot be deleted");
    return OPERATOR_CANCELLED;
  }

  BKE_tracking_object_delete(tracking, tracking_object);

  DEG_id_tag_update(&clip->id, ID_RECALC_COPY_ON_WRITE);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Tracks to Clipboard Operator
 * \{ */

static int copy_tracks_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  clip_tracking_clear_invisible_track_selection(sc, clip);

  BKE_tracking_clipboard_copy_tracks(tracking, tracking_object);

  return OPERATOR_FINISHED;
}

void CLIP_OT_copy_tracks(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Tracks";
  ot->description = "Copy the selected tracks to the internal clipboard";
  ot->idname = "CLIP_OT_copy_tracks";

  /* api callbacks */
  ot->exec = copy_tracks_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Tracks From Clipboard Operator
 * \{ */

static bool paste_tracks_poll(bContext *C)
{
  if (ED_space_clip_tracking_poll(C)) {
    return BKE_tracking_clipboard_has_tracks();
  }

  return false;
}

static int paste_tracks_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  BKE_tracking_tracks_deselect_all(&tracking_object->tracks);
  BKE_tracking_clipboard_paste_tracks(tracking, tracking_object);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_paste_tracks(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Tracks";
  ot->description = "Paste tracks from the internal clipboard";
  ot->idname = "CLIP_OT_paste_tracks";

  /* api callbacks */
  ot->exec = paste_tracks_exec;
  ot->poll = paste_tracks_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Insert Track Keyframe Operator
 * \{ */

static void keyframe_set_flag(bContext *C, bool set)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      if (set) {
        MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);
        marker->flag &= ~MARKER_TRACKED;
      }
      else {
        MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);
        if (marker != nullptr) {
          marker->flag |= MARKER_TRACKED;
        }
      }
    }
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
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

static int keyframe_insert_exec(bContext *C, wmOperator * /*op*/)
{
  keyframe_set_flag(C, true);
  return OPERATOR_FINISHED;
}

void CLIP_OT_keyframe_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Keyframe";
  ot->description = "Insert a keyframe to selected tracks at current frame";
  ot->idname = "CLIP_OT_keyframe_insert";

  /* api callbacks */
  ot->poll = ED_space_clip_tracking_poll;
  ot->exec = keyframe_insert_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Track Keyframe Operator
 * \{ */

static int keyframe_delete_exec(bContext *C, wmOperator * /*op*/)
{
  keyframe_set_flag(C, false);
  return OPERATOR_FINISHED;
}

void CLIP_OT_keyframe_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframe";
  ot->description = "Delete a keyframe from selected tracks at current frame";
  ot->idname = "CLIP_OT_keyframe_delete";

  /* api callbacks */
  ot->poll = ED_space_clip_tracking_poll;
  ot->exec = keyframe_delete_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image from plane track marker
 * \{ */

static ImBuf *sample_plane_marker_image_for_operator(bContext *C)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  const int clip_frame_number = ED_space_clip_get_clip_frame_number(space_clip);

  MovieClip *clip = ED_space_clip_get_clip(space_clip);

  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  const MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(
      tracking_object->active_plane_track, clip_frame_number);

  ImBuf *frame_ibuf = ED_space_clip_get_buffer(space_clip);
  if (frame_ibuf == nullptr) {
    return nullptr;
  }

  ImBuf *plane_ibuf = BKE_tracking_get_plane_imbuf(frame_ibuf, plane_marker);

  IMB_freeImBuf(frame_ibuf);

  return plane_ibuf;
}

static bool new_image_from_plane_marker_poll(bContext *C)
{
  if (!ED_space_clip_tracking_poll(C)) {
    return false;
  }

  SpaceClip *space_clip = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  const MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  if (tracking_object->active_plane_track == nullptr) {
    return false;
  }

  return true;
}

static int new_image_from_plane_marker_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingPlaneTrack *plane_track = tracking_object->active_plane_track;

  ImBuf *plane_ibuf = sample_plane_marker_image_for_operator(C);
  if (plane_ibuf == nullptr) {
    return OPERATOR_CANCELLED;
  }

  plane_track->image = BKE_image_add_from_imbuf(CTX_data_main(C), plane_ibuf, plane_track->name);

  IMB_freeImBuf(plane_ibuf);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_new_image_from_plane_marker(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Image from Plane Marker";
  ot->description = "Create new image from the content of the plane marker";
  ot->idname = "CLIP_OT_new_image_from_plane_marker";

  /* api callbacks */
  ot->poll = new_image_from_plane_marker_poll;
  ot->exec = new_image_from_plane_marker_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool update_image_from_plane_marker_poll(bContext *C)
{
  if (!ED_space_clip_tracking_poll(C)) {
    return false;
  }

  SpaceClip *space_clip = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  const MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  if (tracking_object->active_plane_track == nullptr ||
      tracking_object->active_plane_track->image == nullptr)
  {
    return false;
  }

  const Image *image = tracking_object->active_plane_track->image;
  return image->type == IMA_TYPE_IMAGE && ELEM(image->source, IMA_SRC_FILE, IMA_SRC_GENERATED);
}

static int update_image_from_plane_marker_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingPlaneTrack *plane_track = tracking_object->active_plane_track;

  ImBuf *plane_ibuf = sample_plane_marker_image_for_operator(C);
  if (plane_ibuf == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BKE_image_replace_imbuf(plane_track->image, plane_ibuf);

  IMB_freeImBuf(plane_ibuf);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, plane_track->image);

  BKE_image_partial_update_mark_full_update(plane_track->image);

  return OPERATOR_FINISHED;
}

void CLIP_OT_update_image_from_plane_marker(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Image from Plane Marker";
  ot->description =
      "Update current image used by plane marker from the content of the plane marker";
  ot->idname = "CLIP_OT_update_image_from_plane_marker";

  /* api callbacks */
  ot->poll = update_image_from_plane_marker_poll;
  ot->exec = update_image_from_plane_marker_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
